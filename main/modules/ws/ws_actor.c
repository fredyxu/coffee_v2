#include "modules/ws/ws_actor.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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
#include "modules/wifi/wifi.h"
#include "modules/ws/ws_client.h"

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
	char active_url[WS_FINAL_URL_MAX];
} ws_actor_ctx_t;

static ws_actor_ctx_t s_actor = {
	.sub_handle = MSG_SUB_HANDLE_INVALID,
};

static void ws_actor_client_event_cb(const ws_client_event_t *event, void *user_ctx);

static const char *ws_actor_state_name(ws_actor_state_t state)
{
	switch(state) {
		case WS_ACTOR_STATE_IDLE:
			return "idle";
		case WS_ACTOR_STATE_WAIT_WIFI:
			return "wait_wifi";
		case WS_ACTOR_STATE_CONNECTING:
			return "connecting";
		case WS_ACTOR_STATE_CONNECTED:
			return "connected";
		case WS_ACTOR_STATE_BACKOFF:
			return "backoff";
		default:
			return "unknown";
	}
}

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
	char callsign[sizeof(app_settings.ws_callsign) * 3] = {0};
	char room[sizeof(app_settings.ws_room) * 3] = {0};
	char version[32] = {0};

	ws_actor_make_device_id(device_id, sizeof(device_id));
	(void)ws_actor_url_encode(app_settings.ws_callsign, callsign, sizeof(callsign));
	(void)ws_actor_url_encode(app_settings.ws_room, room, sizeof(room));
	(void)ws_actor_url_encode(APP_FIRMWARE_VERSION, version, sizeof(version));

	const char joiner = strchr(app_settings.ws_url, '?') == NULL ? '?' : '&';
	int written = snprintf(
		buf,
		buf_size,
		"%s%cdevice_id=%s&callsign=%s&room=%s&fw_version=%s",
		app_settings.ws_url,
		joiner,
		device_id,
		callsign,
		room,
		version
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

		case MSG_EVT_SYS_WIFI_DISCONNECTED:
			s_actor.wifi_connected = false;
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
				(void)ws_client_stop();
				ws_actor_set_state(WS_ACTOR_STATE_IDLE);
			}
			break;

		case MSG_EVT_CMD_WS_RECONNECT:
			s_actor.reconnect_delay_ms = 0;
			(void)ws_client_stop();
			(void)ws_actor_connect();
			break;

		case MSG_EVT_SYS_WS_CONNECTED:
			s_actor.reconnect_delay_ms = 0;
			ws_actor_set_state(WS_ACTOR_STATE_CONNECTED);
			break;

		case MSG_EVT_SYS_WS_DISCONNECTED:
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
