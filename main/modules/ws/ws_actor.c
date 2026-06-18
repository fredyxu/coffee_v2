#include "modules/ws/ws_actor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_settings.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "modules/key/cw_keyer_actor.h"
#include "modules/wifi/wifi.h"
#include "modules/ws/ws_client.h"
#include "modules/ws/ws_cw_cache.h"

#define WS_FINAL_URL_MAX 256
#define WS_DEVICE_ID_MAX 32
#define WS_ACTOR_POLL_TICKS pdMS_TO_TICKS(200)

typedef enum {
	WS_ACTOR_STATE_IDLE = 0,
	WS_ACTOR_STATE_WAIT_WIFI,
	WS_ACTOR_STATE_CONNECTING,
	WS_ACTOR_STATE_CONNECTED,
	WS_ACTOR_STATE_BACKOFF,
} ws_actor_state_t;

typedef struct {
	QueueHandle_t queue;
	TaskHandle_t task;
	msg_sub_handle_t sub_handle;
	ws_actor_state_t state;
	bool initialized;
	bool wifi_connected;
	uint32_t reconnect_delay_ms;
	TickType_t next_reconnect_tick;
	TickType_t next_heartbeat_tick;
	char active_url[WS_FINAL_URL_MAX];
	TaskHandle_t wifi_stop_waiter;
	esp_err_t wifi_stop_result;
} ws_actor_ctx_t;

static ws_actor_ctx_t s_actor = {
	.sub_handle = MSG_SUB_HANDLE_INVALID,
};
static portMUX_TYPE s_wifi_stop_wait_mux = portMUX_INITIALIZER_UNLOCKED;

static void ws_actor_client_event_cb(const ws_client_event_t *event, void *user_ctx);

static void ws_actor_set_state(ws_actor_state_t state)
{
	if(s_actor.state == state) {
		return;
	}

	// LOG("ws state: %s -> %s", ws_actor_state_name(s_actor.state), ws_actor_state_name(state));
	s_actor.state = state;
}

static bool ws_actor_tick_reached(TickType_t now, TickType_t target)
{
	return (int32_t)(now - target) >= 0;
}

static void ws_actor_schedule_reconnect(void)
{
	if(!app_settings.ws_auto_reconnect) {
		ws_actor_set_state(WS_ACTOR_STATE_WAIT_WIFI);
		return;
	}

	if(s_actor.reconnect_delay_ms < WS_RECONNECT_MIN_MS) {
		s_actor.reconnect_delay_ms = WS_RECONNECT_MIN_MS;
	} else {
		s_actor.reconnect_delay_ms *= 2;
		if(s_actor.reconnect_delay_ms > WS_RECONNECT_MAX_MS) {
			s_actor.reconnect_delay_ms = WS_RECONNECT_MAX_MS;
		}
	}

	s_actor.next_reconnect_tick = xTaskGetTickCount() + pdMS_TO_TICKS(s_actor.reconnect_delay_ms);
	ws_actor_set_state(WS_ACTOR_STATE_BACKOFF);
}

static void ws_actor_schedule_heartbeat(void)
{
	s_actor.next_heartbeat_tick = xTaskGetTickCount() + pdMS_TO_TICKS(WS_HEARTBEAT_INTERVAL_MS);
}

static void ws_actor_make_device_id(char *buf, size_t buf_size)
{
	if(buf == NULL || buf_size == 0) {
		return;
	}

	uint8_t mac[6] = {0};
	esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
	if(err != ESP_OK) {
		(void)snprintf(buf, buf_size, "%sunknown", WS_DEVICE_ID_PREFIX);
		return;
	}

	(void)snprintf(
		buf,
		buf_size,
		"%s%02x%02x%02x%02x%02x%02x",
		WS_DEVICE_ID_PREFIX,
		mac[0],
		mac[1],
		mac[2],
		mac[3],
		mac[4],
		mac[5]
	);
}

static bool ws_actor_is_unreserved_url_char(char c)
{
	return (c >= 'A' && c <= 'Z') ||
		   (c >= 'a' && c <= 'z') ||
		   (c >= '0' && c <= '9') ||
		   c == '-' ||
		   c == '_' ||
		   c == '.' ||
		   c == '~';
}

static size_t ws_actor_url_encode(const char *src, char *dst, size_t dst_size)
{
	static const char hex[] = "0123456789ABCDEF";

	if(src == NULL || dst == NULL || dst_size == 0) {
		return 0;
	}

	size_t out = 0;
	for(size_t i = 0; src[i] != '\0'; i++) {
		unsigned char c = (unsigned char)src[i];
		if(ws_actor_is_unreserved_url_char((char)c)) {
			if(out + 1 >= dst_size) {
				break;
			}
			dst[out++] = (char)c;
		} else {
			if(out + 3 >= dst_size) {
				break;
			}
			dst[out++] = '%';
			dst[out++] = hex[(c >> 4) & 0x0F];
			dst[out++] = hex[c & 0x0F];
		}
	}

	dst[out] = '\0';
	return out;
}

static esp_err_t ws_actor_build_url(char *buf, size_t buf_size)
{
	if(buf == NULL || buf_size == 0 || app_settings.ws_url[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	char device_id[WS_DEVICE_ID_MAX] = {0};
	char callsign[sizeof(app_settings.user_callsign) * 3] = {0};
	char room[sizeof(app_settings.ws_room) * 3] = {0};
	char version[32] = {0};
	char token[sizeof(WS_DEFAULT_TOKEN) * 3] = {0};

	ws_actor_make_device_id(device_id, sizeof(device_id));
	(void)ws_actor_url_encode(app_settings.user_callsign, callsign, sizeof(callsign));
	(void)ws_actor_url_encode(app_settings.ws_room, room, sizeof(room));
	(void)ws_actor_url_encode(APP_FIRMWARE_VERSION, version, sizeof(version));
	(void)ws_actor_url_encode(WS_DEFAULT_TOKEN, token, sizeof(token));

	const char joiner = strchr(app_settings.ws_url, '?') == NULL ? '?' : '&';
	int written = snprintf(
		buf,
		buf_size,
		"%s%cdevice_id=%s&callsign=%s&room=%s&fw_version=%s&token=%s",
		app_settings.ws_url,
		joiner,
		device_id,
		callsign,
		room,
		version,
		token
	);

	if(written < 0 || (size_t)written >= buf_size) {
		return ESP_ERR_NO_MEM;
	}

	return ESP_OK;
}

static esp_err_t ws_actor_connect(void)
{
	if(!app_settings.ws_enable) {
		(void)ws_client_stop();
		ws_actor_set_state(WS_ACTOR_STATE_IDLE);
		return ESP_OK;
	}

	if(!s_actor.wifi_connected && !wifi_module_is_connected()) {
		ws_actor_set_state(WS_ACTOR_STATE_WAIT_WIFI);
		return ESP_ERR_INVALID_STATE;
	}

	char url[WS_FINAL_URL_MAX] = {0};
	esp_err_t err = ws_actor_build_url(url, sizeof(url));
	if(err != ESP_OK) {
		LOG("ws build url failed: %s", esp_err_to_name(err));
		ws_actor_schedule_reconnect();
		return err;
	}

	if(strcmp(s_actor.active_url, url) == 0 && ws_client_is_connected()) {
		ws_actor_set_state(WS_ACTOR_STATE_CONNECTED);
		return ESP_OK;
	}

	(void)ws_client_deinit();
	err = ws_client_init(&(ws_client_config_t) {
		.url = url,
		.event_cb = ws_actor_client_event_cb,
		.event_user_ctx = NULL,
	});
	if(err != ESP_OK) {
		LOG("ws client init failed: %s", esp_err_to_name(err));
		ws_actor_schedule_reconnect();
		return err;
	}

	(void)snprintf(s_actor.active_url, sizeof(s_actor.active_url), "%s", url);
	ws_actor_set_state(WS_ACTOR_STATE_CONNECTING);
	err = ws_client_start();
	if(err != ESP_OK) {
		LOG("ws client start failed: %s", esp_err_to_name(err));
		ws_actor_schedule_reconnect();
		return err;
	}

	return ESP_OK;
}

static size_t ws_actor_json_escaped_len(const char *text)
{
	size_t len = 0;
	if(text == NULL) {
		return 0;
	}

	for(size_t i = 0; text[i] != '\0'; i++) {
		unsigned char c = (unsigned char)text[i];
		if(c == '"' || c == '\\') {
			len += 2;
		} else if(c == '\n' || c == '\r' || c == '\t') {
			len += 2;
		} else {
			len += 1;
		}
	}

	return len;
}

static void ws_actor_json_escape(const char *src, char *dst)
{
	if(src == NULL || dst == NULL) {
		return;
	}

	size_t out = 0;
	for(size_t i = 0; src[i] != '\0'; i++) {
		char c = src[i];
		switch(c) {
			case '"':
				dst[out++] = '\\';
				dst[out++] = '"';
				break;
			case '\\':
				dst[out++] = '\\';
				dst[out++] = '\\';
				break;
			case '\n':
				dst[out++] = '\\';
				dst[out++] = 'n';
				break;
			case '\r':
				dst[out++] = '\\';
				dst[out++] = 'r';
				break;
			case '\t':
				dst[out++] = '\\';
				dst[out++] = 't';
				break;
			default:
				dst[out++] = c;
				break;
		}
	}
	dst[out] = '\0';
}

static void ws_actor_send_cw(void)
{
	if(!app_settings.ws_enable || !s_actor.wifi_connected || !wifi_module_is_connected() ||
	   !ws_client_is_connected()) {
		return;
	}

	const char *code = cw_keyer_actor_get_raw_text();
	if(code == NULL || code[0] == '\0') {
		return;
	}

	const char *room = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	const char *room_name = strcmp(room, "default") == 0 ? "大厅" : room;
	const char *callsign = app_settings.user_callsign;

	size_t room_len = ws_actor_json_escaped_len(room);
	size_t room_name_len = ws_actor_json_escaped_len(room_name);
	size_t callsign_len = ws_actor_json_escaped_len(callsign);
	size_t code_len = ws_actor_json_escaped_len(code);

	size_t json_size = room_len + room_name_len + callsign_len + code_len + 96;
	char *json = (char *)malloc(json_size);
	char *room_esc = (char *)malloc(room_len + 1);
	char *room_name_esc = (char *)malloc(room_name_len + 1);
	char *callsign_esc = (char *)malloc(callsign_len + 1);
	char *code_esc = (char *)malloc(code_len + 1);
	if(json == NULL || room_esc == NULL || room_name_esc == NULL ||
	   callsign_esc == NULL || code_esc == NULL) {
		free(json);
		free(room_esc);
		free(room_name_esc);
		free(callsign_esc);
		free(code_esc);
		return;
	}

	ws_actor_json_escape(room, room_esc);
	ws_actor_json_escape(room_name, room_name_esc);
	ws_actor_json_escape(callsign, callsign_esc);
	ws_actor_json_escape(code, code_esc);

	int written = snprintf(
		json,
		json_size,
		"{\"type\":\"cw\",\"room\":\"%s\",\"room_name\":\"%s\",\"callsign\":\"%s\",\"code\":\"%s\"}",
		room_esc,
		room_name_esc,
		callsign_esc,
		code_esc
	);

	if(written > 0 && (size_t)written < json_size && ws_client_send_text(json) == ESP_OK) {
		ws_actor_schedule_heartbeat();
		(void)msg_send_cmd_value(MSG_SRC_WS, MSG_EVT_CMD_CW_CLEAR, 1, 0);
	}

	free(json);
	free(room_esc);
	free(room_name_esc);
	free(callsign_esc);
	free(code_esc);
}

static void ws_actor_send_heartbeat(void)
{
	if(s_actor.state != WS_ACTOR_STATE_CONNECTED || !app_settings.ws_enable ||
	   !s_actor.wifi_connected || !wifi_module_is_connected()) {
		return;
	}

	if(!ws_client_is_connected()) {
		(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_DISCONNECTED, 0, 0);
		return;
	}

	esp_err_t err = ws_client_send_text(WS_HEARTBEAT_PAYLOAD);
	if(err == ESP_OK) {
		ws_actor_schedule_heartbeat();
		return;
	}

	(void)ws_client_stop();
	(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_DISCONNECTED, 0, 0);
}

static char *ws_actor_copy_ws_payload(const ws_client_event_t *event)
{
	if(event == NULL || event->data == NULL || event->data_len <= 0) {
		return NULL;
	}

	char *text = (char *)malloc((size_t)event->data_len + 1);
	if(text == NULL) {
		return NULL;
	}

	memcpy(text, event->data, (size_t)event->data_len);
	text[event->data_len] = '\0';
	return text;
}

static bool ws_actor_json_get_string(const char *json,
									 const char *key,
									 char *out,
									 size_t out_size)
{
	if(json == NULL || key == NULL || out == NULL || out_size == 0) {
		return false;
	}

	out[0] = '\0';

	char pattern[32] = {0};
	int pattern_len = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
	if(pattern_len <= 0 || (size_t)pattern_len >= sizeof(pattern)) {
		return false;
	}

	const char *p = strstr(json, pattern);
	if(p == NULL) {
		return false;
	}

	p += pattern_len;
	while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
		p++;
	}
	if(*p != ':') {
		return false;
	}
	p++;
	while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
		p++;
	}
	if(*p != '"') {
		return false;
	}
	p++;

	size_t out_len = 0;
	while(*p != '\0' && *p != '"') {
		char c = *p++;
		if(c == '\\' && *p != '\0') {
			char esc = *p++;
			switch(esc) {
				case '"':
				case '\\':
				case '/':
					c = esc;
					break;
				case 'n':
					c = '\n';
					break;
				case 'r':
					c = '\r';
					break;
				case 't':
					c = '\t';
					break;
				default:
					c = esc;
					break;
			}
		}

		if(out_len + 1 < out_size) {
			out[out_len++] = c;
		}
	}

	out[out_len] = '\0';
	return *p == '"';
}

static void ws_actor_handle_cw_payload(const char *json)
{
	ws_cw_record_t record = {0};

	if(!ws_actor_json_get_string(json, "code", record.code, sizeof(record.code))) {
		return;
	}

	(void)ws_actor_json_get_string(json, "room", record.room, sizeof(record.room));
	(void)ws_actor_json_get_string(json, "room_name", record.room_name, sizeof(record.room_name));
	(void)ws_actor_json_get_string(json, "callsign", record.callsign, sizeof(record.callsign));
	(void)ws_actor_json_get_string(json, "from", record.from, sizeof(record.from));
	(void)ws_actor_json_get_string(json, "sender_id", record.sender_id, sizeof(record.sender_id));
	(void)ws_actor_json_get_string(json, "date", record.date, sizeof(record.date));
	(void)ws_actor_json_get_string(json, "time", record.time, sizeof(record.time));
	(void)ws_actor_json_get_string(json, "role", record.role, sizeof(record.role));
	record.level = 0;

	uint32_t seq = 0;
	if(ws_cw_cache_add(&record, &seq) != ESP_OK) {
		return;
	}

	// LOG("ws cw received: seq=%lu from=%s callsign=%s room=%s code=%s time=%s",
	// 	(unsigned long)seq,
	// 	record.from,
	// 	record.callsign,
	// 	record.room,
	// 	record.code,
	// 	record.time);

	(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_CW_RECEIVED, (int)seq, 0);
}

static void ws_actor_handle_ws_data(const ws_client_event_t *event)
{
	if(event == NULL || event->binary) {
		return;
	}

	char *json = ws_actor_copy_ws_payload(event);
	if(json == NULL) {
		return;
	}

	char type[16] = {0};
	if(ws_actor_json_get_string(json, "type", type, sizeof(type)) && strcmp(type, "cw") == 0) {
		ws_actor_handle_cw_payload(json);
	}

	free(json);
}

static void ws_actor_poll_heartbeat(void)
{
	if(s_actor.state != WS_ACTOR_STATE_CONNECTED) {
		return;
	}

	if(ws_actor_tick_reached(xTaskGetTickCount(), s_actor.next_heartbeat_tick)) {
		ws_actor_send_heartbeat();
	}
}

static void ws_actor_client_event_cb(const ws_client_event_t *event, void *user_ctx)
{
	(void)user_ctx;
	if(event == NULL || s_actor.queue == NULL) {
		return;
	}

	switch(event->id) {
		case WS_CLIENT_EVT_CONNECTED:
			(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_CONNECTED, 1, 0);
			break;
		case WS_CLIENT_EVT_DISCONNECTED:
		case WS_CLIENT_EVT_ERROR:
			(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_DISCONNECTED, 0, 0);
			break;
		case WS_CLIENT_EVT_DATA:
			ws_actor_handle_ws_data(event);
			break;
		default:
			break;
	}
}

static void ws_actor_apply_msg(const msg_t *msg)
{
	if(msg == NULL) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_SYS_WIFI_CONNECTED:
			s_actor.wifi_connected = true;
			s_actor.reconnect_delay_ms = 0;
			(void)ws_actor_connect();
			break;

		case MSG_EVT_SYS_WIFI_STOPPING:
			s_actor.wifi_connected = false;
			s_actor.reconnect_delay_ms = 0;
			s_actor.next_heartbeat_tick = 0;
			esp_err_t deinit_err = ws_client_deinit();
			s_actor.active_url[0] = '\0';
			ws_actor_set_state(WS_ACTOR_STATE_WAIT_WIFI);
			TaskHandle_t waiter = NULL;
			portENTER_CRITICAL(&s_wifi_stop_wait_mux);
			s_actor.wifi_stop_result = deinit_err;
			waiter = s_actor.wifi_stop_waiter;
			portEXIT_CRITICAL(&s_wifi_stop_wait_mux);
			if(waiter != NULL) {
				xTaskNotifyGive(waiter);
			}
			break;

		case MSG_EVT_SYS_WIFI_DISCONNECTED:
			s_actor.wifi_connected = false;
			s_actor.next_heartbeat_tick = 0;
			(void)ws_client_stop();
			ws_actor_set_state(WS_ACTOR_STATE_WAIT_WIFI);
			break;

		case MSG_EVT_CMD_WS_SET_ENABLE:
			(void)app_settings_update(&(app_settings_update_t) {
				.id = APP_SETTING_ID_WS_ENABLE,
				.value.b = msg->data.value != 0,
			});
			if(msg->data.value != 0) {
				s_actor.reconnect_delay_ms = 0;
				(void)ws_actor_connect();
			} else {
				s_actor.next_heartbeat_tick = 0;
				(void)ws_client_stop();
				ws_actor_set_state(WS_ACTOR_STATE_IDLE);
			}
			break;

		case MSG_EVT_CMD_WS_RECONNECT:
			s_actor.reconnect_delay_ms = 0;
			s_actor.next_heartbeat_tick = 0;
			(void)ws_client_stop();
			(void)ws_actor_connect();
			break;

		case MSG_EVT_CMD_WS_SEND_CW:
			ws_actor_send_cw();
			break;

		case MSG_EVT_SYS_WS_CONNECTED:
			s_actor.reconnect_delay_ms = 0;
			ws_actor_schedule_heartbeat();
			ws_actor_set_state(WS_ACTOR_STATE_CONNECTED);
			break;

		case MSG_EVT_SYS_WS_DISCONNECTED:
			s_actor.next_heartbeat_tick = 0;
			if(s_actor.state != WS_ACTOR_STATE_WAIT_WIFI && app_settings.ws_enable) {
				ws_actor_schedule_reconnect();
			}
			break;

		default:
			break;
	}
}

static void ws_actor_task(void *arg)
{
	(void)arg;

	while(1) {
		msg_t msg = {0};
		while(xQueueReceive(s_actor.queue, &msg, 0) == pdTRUE) {
			ws_actor_apply_msg(&msg);
		}

		if(s_actor.state == WS_ACTOR_STATE_BACKOFF &&
		   ws_actor_tick_reached(xTaskGetTickCount(), s_actor.next_reconnect_tick)) {
			(void)ws_actor_connect();
		}

		ws_actor_poll_heartbeat();

		vTaskDelay(WS_ACTOR_POLL_TICKS);
	}
}

static esp_err_t ws_actor_subscribe(void)
{
	const msg_topic_t topics[] = {
		MSG_TOPIC_WIFI_EVENT,
		MSG_TOPIC_WEBSOCKET_CMD,
		MSG_TOPIC_WEBSOCKET_EVENT,
	};
	return msg_sub(s_actor.queue, topics, sizeof(topics) / sizeof(topics[0]), &s_actor.sub_handle);
}

esp_err_t ws_actor_init(void)
{
	if(s_actor.initialized) {
		return ESP_OK;
	}

	esp_err_t err = msg_actor_queue_create_with_len(WS_ACTOR_QUEUE_LEN, &s_actor.queue);
	if(err != ESP_OK) {
		return err;
	}

	err = ws_actor_subscribe();
	if(err != ESP_OK) {
		return err;
	}

	BaseType_t ok = xTaskCreatePinnedToCore(
		ws_actor_task,
		"ws_actor",
		WS_ACTOR_TASK_STACK,
		NULL,
		TASK_PRIO_CON,
		&s_actor.task,
		TASK_CORE_CON
	);
	if(ok != pdPASS) {
		return ESP_FAIL;
	}

	s_actor.initialized = true;
	s_actor.wifi_connected = wifi_module_is_connected();
	ws_actor_set_state(s_actor.wifi_connected ? WS_ACTOR_STATE_IDLE : WS_ACTOR_STATE_WAIT_WIFI);

	if(s_actor.wifi_connected && app_settings.ws_enable) {
		s_actor.reconnect_delay_ms = 0;
		(void)ws_actor_connect();
	}

	return ESP_OK;
}

esp_err_t ws_actor_prepare_wifi_stop(TickType_t timeout_ticks)
{
	if(!s_actor.initialized || s_actor.queue == NULL) {
		return ESP_OK;
	}

	TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
	if(current_task == NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	portENTER_CRITICAL(&s_wifi_stop_wait_mux);
	if(s_actor.wifi_stop_waiter != NULL) {
		portEXIT_CRITICAL(&s_wifi_stop_wait_mux);
		return ESP_ERR_INVALID_STATE;
	}
	s_actor.wifi_stop_waiter = current_task;
	s_actor.wifi_stop_result = ESP_ERR_TIMEOUT;
	portEXIT_CRITICAL(&s_wifi_stop_wait_mux);

	esp_err_t err = msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_STOPPING, 0, 0);
	if(err != ESP_OK) {
		portENTER_CRITICAL(&s_wifi_stop_wait_mux);
		s_actor.wifi_stop_waiter = NULL;
		portEXIT_CRITICAL(&s_wifi_stop_wait_mux);
		return err;
	}

	if(ulTaskNotifyTake(pdTRUE, timeout_ticks) == 0) {
		portENTER_CRITICAL(&s_wifi_stop_wait_mux);
		if(s_actor.wifi_stop_waiter == current_task) {
			s_actor.wifi_stop_waiter = NULL;
		}
		portEXIT_CRITICAL(&s_wifi_stop_wait_mux);
		return ESP_ERR_TIMEOUT;
	}

	portENTER_CRITICAL(&s_wifi_stop_wait_mux);
	err = s_actor.wifi_stop_result;
	if(s_actor.wifi_stop_waiter == current_task) {
		s_actor.wifi_stop_waiter = NULL;
	}
	portEXIT_CRITICAL(&s_wifi_stop_wait_mux);

	return err;
}

esp_err_t ws_actor_deinit(void)
{
	if(!s_actor.initialized) {
		return ESP_OK;
	}

	if(s_actor.sub_handle != MSG_SUB_HANDLE_INVALID) {
		(void)msg_unsub(s_actor.sub_handle, NULL, 0);
		s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
	}

	(void)ws_client_deinit();

	if(s_actor.task != NULL) {
		vTaskDelete(s_actor.task);
		s_actor.task = NULL;
	}

	if(s_actor.queue != NULL) {
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
	}

	memset(&s_actor, 0, sizeof(s_actor));
	s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
	return ESP_OK;
}
