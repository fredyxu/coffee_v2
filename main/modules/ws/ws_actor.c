#include "modules/ws/ws_actor.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/app_settings.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "apps/esp_sntp.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "modules/audio/audio_actor.h"
#include "modules/key/cw_keyer_actor.h"
#include "modules/wifi/wifi.h"
#include "modules/ws/ws_client.h"
#include "modules/ws/ws_cw_cache.h"
#include "modules/ws/ws_room_cache.h"

#if INTERCOM_ROOM_SYNC_ENABLE
#include "cJSON.h"
#endif
#define WS_FINAL_URL_MAX 256
#define WS_DEVICE_ID_MAX 32
#define WS_ACTOR_POLL_TICKS pdMS_TO_TICKS(200)
#define WS_TIME_SYNC_TIMEOUT_TICKS pdMS_TO_TICKS(15000)
#define WS_MIN_TLS_UNIX_TIME 1767225600

#define WS_INTERCOM_AUDIO_MAGIC_0 'C'
#define WS_INTERCOM_AUDIO_MAGIC_1 'A'
#define WS_INTERCOM_AUDIO_VERSION 1
#define WS_INTERCOM_AUDIO_CODEC_PCM16 0
#define WS_INTERCOM_AUDIO_CODEC_G711_ULAW 1
#define WS_INTERCOM_AUDIO_FRAME_PACKET_BYTES (sizeof(ws_intercom_audio_header_t) + INTERCOM_AUDIO_FRAME_BYTES)
#define WS_INTERCOM_AUDIO_BATCH_FRAMES INTERCOM_AUDIO_WS_BATCH_FRAMES
#define WS_INTERCOM_AUDIO_HEADER_BYTES 12
#define WS_INTERCOM_AUDIO_MAX_FRAME_SAMPLES 320

typedef enum {
	WS_ACTOR_STATE_IDLE = 0,
	WS_ACTOR_STATE_WAIT_WIFI,
	WS_ACTOR_STATE_WAIT_TIME,
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
	TickType_t time_sync_deadline_tick;
	bool time_synced_once;
	char active_url[WS_FINAL_URL_MAX];
	TaskHandle_t wifi_stop_waiter;
	esp_err_t wifi_stop_result;
	uint32_t audio_send_consecutive_failures;
	bool intercom_room_joined;
	bool intercom_tx_active;
	bool intercom_rx_active;
	bool intercom_rx_stream_started;
	bool intercom_rx_have_seq;
	uint16_t intercom_rx_last_seq;
	uint32_t intercom_rx_frames;
	uint32_t intercom_rx_batches;
	uint32_t intercom_rx_seq_gaps;
	uint32_t intercom_rx_underruns;
	uint32_t intercom_rx_overflows;
	uint32_t intercom_rx_invalid;
	size_t intercom_rx_prebuffer_count;
} ws_actor_ctx_t;

typedef struct __attribute__((packed)) {
	uint8_t magic[2];
	uint8_t version;
	uint8_t codec;
	uint16_t seq;
	uint16_t samples;
	uint32_t sample_rate;
} ws_intercom_audio_header_t;

static ws_actor_ctx_t s_actor = {
	.sub_handle = MSG_SUB_HANDLE_INVALID,
};
static uint8_t s_intercom_audio_tx_batch[WS_INTERCOM_AUDIO_FRAME_PACKET_BYTES * WS_INTERCOM_AUDIO_BATCH_FRAMES];
static size_t s_intercom_audio_tx_batch_count;
static audio_stream_chunk_t s_intercom_rx_prebuffer[INTERCOM_RX_PLAYBACK_PREBUFFER_FRAMES];
static portMUX_TYPE s_wifi_stop_wait_mux = portMUX_INITIALIZER_UNLOCKED;

static void ws_actor_client_event_cb(const ws_client_event_t *event, void *user_ctx);

static uint8_t ws_actor_pcm16_to_ulaw(int16_t sample)
{
	const uint16_t bias = 0x84;
	const int16_t clip = 32635;
	uint8_t sign = 0;
	int32_t pcm = sample;

	if(pcm < 0) {
		pcm = -pcm;
		sign = 0x80;
	}
	if(pcm > clip) {
		pcm = clip;
	}
	pcm += bias;

	uint8_t exponent = 7;
	for(uint16_t mask = 0x4000; exponent > 0 && (pcm & mask) == 0; mask >>= 1) {
		exponent--;
	}
	uint8_t mantissa = (uint8_t)((pcm >> (exponent + 3)) & 0x0F);
	return (uint8_t)(~(sign | (exponent << 4) | mantissa));
}

static size_t ws_actor_encode_intercom_payload(uint8_t *dst, const int16_t *samples, size_t sample_count)
{
	if(dst == NULL || samples == NULL) {
		return 0;
	}

#if INTERCOM_AUDIO_CODEC == INTERCOM_AUDIO_CODEC_G711_ULAW
	for(size_t i = 0; i < sample_count; i++) {
		dst[i] = ws_actor_pcm16_to_ulaw(samples[i]);
	}
	return sample_count;
#else
	size_t payload_bytes = sample_count * sizeof(int16_t);
	memcpy(dst, samples, payload_bytes);
	return payload_bytes;
#endif
}

static void ws_actor_set_state(ws_actor_state_t state)
{
	if(s_actor.state == state) {
		return;
	}

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

static bool ws_actor_ready_for_intercom_audio(void)
{
	return app_settings.ws_enable &&
		   s_actor.wifi_connected &&
		   wifi_module_is_connected() &&
		   ws_client_is_connected();
}

static void ws_actor_start_sntp(void)
{
	if(esp_sntp_enabled()) {
		return;
	}

	setenv("TZ", "CST-8", 1);
	tzset();
	esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
	esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
	esp_sntp_setservername(0, "ntp.aliyun.com");
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 1
	esp_sntp_setservername(1, "time.cloudflare.com");
#endif
#if CONFIG_LWIP_SNTP_MAX_SERVERS > 2
	esp_sntp_setservername(2, "pool.ntp.org");
#endif
	esp_sntp_init();
}

static bool ws_actor_time_ready(void)
{
	time_t now = 0;
	time(&now);
	sntp_sync_status_t status = sntp_get_sync_status();

	if(status == SNTP_SYNC_STATUS_COMPLETED) {
		s_actor.time_synced_once = true;
	}

	if(now >= WS_MIN_TLS_UNIX_TIME) {
		s_actor.time_synced_once = true;
	}

	return s_actor.time_synced_once;
}

static void ws_actor_wait_time_sync(void)
{
	ws_actor_start_sntp();
	s_actor.time_sync_deadline_tick = xTaskGetTickCount() + WS_TIME_SYNC_TIMEOUT_TICKS;
	ws_actor_set_state(WS_ACTOR_STATE_WAIT_TIME);
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

	if(!ws_actor_time_ready()) {
		ws_actor_wait_time_sync();
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
		(void)ws_client_deinit();
		s_actor.active_url[0] = '\0';
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

static void ws_actor_send_intercom_signal(const char *type)
{
	if(type == NULL || !app_settings.ws_enable || !s_actor.wifi_connected ||
	   !wifi_module_is_connected() || !ws_client_is_connected()) {
		return;
	}

	const char *room = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	const char *callsign = app_settings.user_callsign;

	size_t type_len = ws_actor_json_escaped_len(type);
	size_t room_len = ws_actor_json_escaped_len(room);
	size_t callsign_len = ws_actor_json_escaped_len(callsign);
	size_t json_size = type_len + room_len + callsign_len + 64;

	char *json = (char *)malloc(json_size);
	char *type_esc = (char *)malloc(type_len + 1);
	char *room_esc = (char *)malloc(room_len + 1);
	char *callsign_esc = (char *)malloc(callsign_len + 1);
	if(json == NULL || type_esc == NULL || room_esc == NULL || callsign_esc == NULL) {
		free(json);
		free(type_esc);
		free(room_esc);
		free(callsign_esc);
		return;
	}

	ws_actor_json_escape(type, type_esc);
	ws_actor_json_escape(room, room_esc);
	ws_actor_json_escape(callsign, callsign_esc);

	int written = snprintf(
		json,
		json_size,
		"{\"type\":\"%s\",\"room\":\"%s\",\"callsign\":\"%s\"}",
		type_esc,
		room_esc,
		callsign_esc
	);

	if(written > 0 && (size_t)written < json_size && ws_client_send_text(json) == ESP_OK) {
		ws_actor_schedule_heartbeat();
	}

	free(json);
	free(type_esc);
	free(room_esc);
	free(callsign_esc);
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

#if INTERCOM_ROOM_SYNC_ENABLE
static esp_err_t ws_actor_send_json_text(const char *json)
{
	if(json == NULL || !app_settings.ws_enable || !s_actor.wifi_connected ||
	   !wifi_module_is_connected() || !ws_client_is_connected()) {
		return ESP_ERR_INVALID_STATE;
	}

	esp_err_t err = ws_client_send_text(json);
	if(err == ESP_OK) {
		ws_actor_schedule_heartbeat();
	}
	return err;
}

static void ws_actor_send_room_list_req(void)
{
	esp_err_t err = ws_actor_send_json_text("{\"type\":\"room_list_req\"}");
	if(err == ESP_OK) {
		LOG("ws room list request sent");
	}
	if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		LOG("ws room list request failed: %s", esp_err_to_name(err));
	} else if(err == ESP_ERR_INVALID_STATE) {
		LOG("ws room list request skipped: not connected");
	}
}

static void ws_actor_send_room_users_req(void)
{
	const char *room = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	size_t room_len = ws_actor_json_escaped_len(room);
	size_t json_size = room_len + 48;
	char *json = (char *)malloc(json_size);
	char *room_esc = (char *)malloc(room_len + 1);
	if(json == NULL || room_esc == NULL) {
		free(json);
		free(room_esc);
		return;
	}

	ws_actor_json_escape(room, room_esc);
	int written = snprintf(json, json_size, "{\"type\":\"room_users_req\",\"room\":\"%s\"}", room_esc);
	free(room_esc);
	if(written <= 0 || (size_t)written >= json_size) {
		free(json);
		return;
	}

	esp_err_t err = ws_actor_send_json_text(json);
	if(err == ESP_OK) {
		LOG("ws room users request sent: room=%s", room);
	}
	if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
		LOG("ws room users request failed: room=%s err=%s", room, esp_err_to_name(err));
	} else if(err == ESP_ERR_INVALID_STATE) {
		LOG("ws room users request skipped: room=%s not connected", room);
	}
	free(json);
}

static void ws_actor_send_intercom_room_presence(const char *type)
{
	if(type == NULL) {
		return;
	}

	const char *room = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	const char *callsign = app_settings.user_callsign[0] != '\0' ?
						   app_settings.user_callsign :
						   app_settings.ws_callsign;
	if(callsign == NULL || callsign[0] == '\0') {
		callsign = USER_DEFAULT_CALLSIGN;
	}

	size_t type_len = ws_actor_json_escaped_len(type);
	size_t room_len = ws_actor_json_escaped_len(room);
	size_t callsign_len = ws_actor_json_escaped_len(callsign);
	size_t json_size = type_len + room_len + callsign_len + 80;
	char *json = (char *)malloc(json_size);
	char *type_esc = (char *)malloc(type_len + 1);
	char *room_esc = (char *)malloc(room_len + 1);
	char *callsign_esc = (char *)malloc(callsign_len + 1);
	if(json == NULL || type_esc == NULL || room_esc == NULL || callsign_esc == NULL) {
		free(json);
		free(type_esc);
		free(room_esc);
		free(callsign_esc);
		return;
	}

	ws_actor_json_escape(type, type_esc);
	ws_actor_json_escape(room, room_esc);
	ws_actor_json_escape(callsign, callsign_esc);
	int written = snprintf(
		json,
		json_size,
		"{\"type\":\"%s\",\"room\":\"%s\",\"callsign\":\"%s\"}",
		type_esc,
		room_esc,
		callsign_esc
	);
	if(written > 0 && (size_t)written < json_size) {
		esp_err_t err = ws_actor_send_json_text(json);
		LOG("ws intercom room presence sent: type=%s room=%s err=%s",
			type,
			room,
			esp_err_to_name(err));
	}

	free(json);
	free(type_esc);
	free(room_esc);
	free(callsign_esc);
}
#endif

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

static bool ws_actor_json_get_bool(const char *json, const char *key, bool *out)
{
	if(json == NULL || key == NULL || out == NULL) {
		return false;
	}

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

	if(strncmp(p, "true", 4) == 0) {
		*out = true;
		return true;
	}
	if(strncmp(p, "false", 5) == 0) {
		*out = false;
		return true;
	}

	return false;
}

static bool ws_actor_is_self_callsign(const char *callsign)
{
	if(callsign == NULL || callsign[0] == '\0') {
		return false;
	}

	return (app_settings.user_callsign[0] != '\0' && strcmp(callsign, app_settings.user_callsign) == 0) ||
		   (app_settings.ws_callsign[0] != '\0' && strcmp(callsign, app_settings.ws_callsign) == 0);
}

static uint16_t ws_actor_read_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t ws_actor_read_le32(const uint8_t *p)
{
	return (uint32_t)p[0] |
		   ((uint32_t)p[1] << 8) |
		   ((uint32_t)p[2] << 16) |
		   ((uint32_t)p[3] << 24);
}

static int16_t ws_actor_ulaw_to_pcm16(uint8_t u_val)
{
	uint8_t u = (uint8_t)(~u_val);
	int sign = u & 0x80;
	int exponent = (u >> 4) & 0x07;
	int mantissa = u & 0x0F;
	int sample = ((mantissa << 3) + 0x84) << exponent;
	sample -= 0x84;
	if(sign != 0) {
		sample = -sample;
	}
	if(sample > INT16_MAX) {
		sample = INT16_MAX;
	} else if(sample < INT16_MIN) {
		sample = INT16_MIN;
	}
	return (int16_t)sample;
}

static void ws_actor_stop_intercom_rx_audio(const char *reason)
{
	if(!s_actor.intercom_rx_active && !s_actor.intercom_rx_stream_started &&
	   s_actor.intercom_rx_prebuffer_count == 0) {
		return;
	}

	// LOG("ws intercom rx stop: reason=%s frames=%u batches=%u gaps=%u overflow=%u invalid=%u prebuffer=%u",
	// 	reason != NULL ? reason : "-",
	// 	(unsigned)s_actor.intercom_rx_frames,
	// 	(unsigned)s_actor.intercom_rx_batches,
	// 	(unsigned)s_actor.intercom_rx_seq_gaps,
	// 	(unsigned)s_actor.intercom_rx_overflows,
	// 	(unsigned)s_actor.intercom_rx_invalid,
	// 	(unsigned)s_actor.intercom_rx_prebuffer_count);
	if(s_actor.intercom_rx_stream_started) {
		(void)audio_actor_stream_stop(pdMS_TO_TICKS(20));
	}
	s_actor.intercom_rx_active = false;
	s_actor.intercom_rx_stream_started = false;
	s_actor.intercom_rx_have_seq = false;
	s_actor.intercom_rx_last_seq = 0;
	s_actor.intercom_rx_frames = 0;
	s_actor.intercom_rx_batches = 0;
	s_actor.intercom_rx_seq_gaps = 0;
	s_actor.intercom_rx_underruns = 0;
	s_actor.intercom_rx_overflows = 0;
	s_actor.intercom_rx_invalid = 0;
	s_actor.intercom_rx_prebuffer_count = 0;
}

static void ws_actor_start_intercom_rx_audio(const char *callsign)
{
	if(!s_actor.intercom_room_joined) {
		return;
	}
	if(s_actor.intercom_tx_active || ws_actor_is_self_callsign(callsign)) {
		return;
	}

	if(s_actor.intercom_rx_active) {
		return;
	}

	(void)audio_actor_stop(pdMS_TO_TICKS(20));
	s_actor.intercom_rx_active = true;
	s_actor.intercom_rx_stream_started = false;
	s_actor.intercom_rx_have_seq = false;
	s_actor.intercom_rx_last_seq = 0;
	s_actor.intercom_rx_frames = 0;
	s_actor.intercom_rx_batches = 0;
	s_actor.intercom_rx_seq_gaps = 0;
	s_actor.intercom_rx_underruns = 0;
	s_actor.intercom_rx_overflows = 0;
	s_actor.intercom_rx_invalid = 0;
	s_actor.intercom_rx_prebuffer_count = 0;
	// LOG("ws intercom rx start: speaker=%s prebuffer=%u",
	// 	callsign != NULL && callsign[0] != '\0' ? callsign : "-",
	// 	(unsigned)INTERCOM_RX_PLAYBACK_PREBUFFER_FRAMES);
}

static esp_err_t ws_actor_push_intercom_rx_chunk(const audio_stream_chunk_t *chunk)
{
	if(chunk == NULL || chunk->sample_count == 0 || chunk->sample_rate == 0) {
		return ESP_ERR_INVALID_ARG;
	}

	if(!s_actor.intercom_rx_stream_started) {
		if(s_actor.intercom_rx_prebuffer_count < INTERCOM_RX_PLAYBACK_PREBUFFER_FRAMES) {
			s_intercom_rx_prebuffer[s_actor.intercom_rx_prebuffer_count++] = *chunk;
		}
		if(s_actor.intercom_rx_prebuffer_count < INTERCOM_RX_PLAYBACK_PREBUFFER_FRAMES) {
			return ESP_OK;
		}

		esp_err_t err = audio_actor_stream_start(chunk->sample_rate, pdMS_TO_TICKS(20));
		if(err != ESP_OK) {
			LOG("ws intercom rx stream start failed: %s", esp_err_to_name(err));
			return err;
		}
		vTaskDelay(1);
		s_actor.intercom_rx_stream_started = true;
		for(size_t i = 0; i < s_actor.intercom_rx_prebuffer_count; i++) {
			err = audio_actor_stream_push(
				s_intercom_rx_prebuffer[i].samples,
				s_intercom_rx_prebuffer[i].sample_count,
				s_intercom_rx_prebuffer[i].sample_rate,
				pdMS_TO_TICKS(INTERCOM_RX_PLAYBACK_PUSH_TIMEOUT_MS)
			);
			if(err != ESP_OK) {
				if(err == ESP_ERR_TIMEOUT) {
					s_actor.intercom_rx_overflows++;
				} else {
					s_actor.intercom_rx_invalid++;
					LOG("ws intercom rx prebuffer push failed: err=%s", esp_err_to_name(err));
				}
			}
		}
		s_actor.intercom_rx_prebuffer_count = 0;
		return ESP_OK;
	}

	esp_err_t err = audio_actor_stream_push(
		chunk->samples,
		chunk->sample_count,
		chunk->sample_rate,
		pdMS_TO_TICKS(INTERCOM_RX_PLAYBACK_PUSH_TIMEOUT_MS)
	);
	if(err != ESP_OK) {
		if(err == ESP_ERR_TIMEOUT) {
			s_actor.intercom_rx_overflows++;
			if(s_actor.intercom_rx_overflows <= 5 || (s_actor.intercom_rx_overflows % 50U) == 0U) {
				LOG("ws intercom rx playback overflow: count=%u err=%s",
					(unsigned)s_actor.intercom_rx_overflows,
					esp_err_to_name(err));
			}
		} else {
			s_actor.intercom_rx_invalid++;
			if(s_actor.intercom_rx_invalid <= 5 || (s_actor.intercom_rx_invalid % 50U) == 0U) {
				LOG("ws intercom rx playback push failed: invalid=%u err=%s",
					(unsigned)s_actor.intercom_rx_invalid,
					esp_err_to_name(err));
			}
		}
	}
	return err;
}

static void ws_actor_handle_intercom_audio_binary(const ws_client_event_t *event)
{
	if(event == NULL || !event->binary || event->data == NULL || event->data_len <= 0) {
		return;
	}
	if(s_actor.intercom_tx_active) {
		return;
	}
	if(!s_actor.intercom_room_joined) {
		if(s_actor.intercom_rx_active || s_actor.intercom_rx_stream_started) {
			ws_actor_stop_intercom_rx_audio("not_in_room");
		}
		return;
	}
	if(!s_actor.intercom_rx_active) {
		ws_actor_start_intercom_rx_audio("-");
	}

	const uint8_t *data = (const uint8_t *)event->data;
	size_t len = (size_t)event->data_len;
	size_t offset = 0;
	uint32_t batch_frames = 0;

	while(offset < len) {
		size_t remain = len - offset;
		if(remain < WS_INTERCOM_AUDIO_HEADER_BYTES) {
			s_actor.intercom_rx_invalid++;
			LOG("ws intercom rx invalid: short_header remain=%u len=%u",
				(unsigned)remain,
				(unsigned)len);
			break;
		}

		const uint8_t *frame = data + offset;
		if(frame[0] != WS_INTERCOM_AUDIO_MAGIC_0 ||
		   frame[1] != WS_INTERCOM_AUDIO_MAGIC_1 ||
		   frame[2] != WS_INTERCOM_AUDIO_VERSION) {
			s_actor.intercom_rx_invalid++;
			LOG("ws intercom rx invalid: bad_header offset=%u len=%u magic=%02x%02x ver=%u",
				(unsigned)offset,
				(unsigned)len,
				frame[0],
				frame[1],
				frame[2]);
			break;
		}

		uint8_t codec = frame[3];
		uint16_t seq = ws_actor_read_le16(frame + 4);
		uint16_t samples = ws_actor_read_le16(frame + 6);
		uint32_t sample_rate = ws_actor_read_le32(frame + 8);
		size_t payload_len = 0;
		if(codec == WS_INTERCOM_AUDIO_CODEC_G711_ULAW) {
			payload_len = samples;
		} else if(codec == WS_INTERCOM_AUDIO_CODEC_PCM16) {
			payload_len = (size_t)samples * 2U;
		} else {
			s_actor.intercom_rx_invalid++;
			LOG("ws intercom rx invalid: unsupported_codec=%u seq=%u", codec, (unsigned)seq);
			break;
		}

		if(samples == 0 || samples > WS_INTERCOM_AUDIO_MAX_FRAME_SAMPLES || sample_rate == 0) {
			s_actor.intercom_rx_invalid++;
			LOG("ws intercom rx invalid: samples=%u sample_rate=%u seq=%u",
				(unsigned)samples,
				(unsigned)sample_rate,
				(unsigned)seq);
			break;
		}

		size_t frame_len = WS_INTERCOM_AUDIO_HEADER_BYTES + payload_len;
		if(remain < frame_len) {
			s_actor.intercom_rx_invalid++;
			LOG("ws intercom rx invalid: short_payload remain=%u need=%u seq=%u",
				(unsigned)remain,
				(unsigned)frame_len,
				(unsigned)seq);
			break;
		}

		if(s_actor.intercom_rx_have_seq) {
			uint16_t expected = (uint16_t)(s_actor.intercom_rx_last_seq + 1U);
			if(seq != expected) {
				s_actor.intercom_rx_seq_gaps++;
				if(s_actor.intercom_rx_seq_gaps <= 5 || (s_actor.intercom_rx_seq_gaps % 50U) == 0U) {
					LOG("ws intercom rx seq gap: expected=%u actual=%u gaps=%u",
						(unsigned)expected,
						(unsigned)seq,
						(unsigned)s_actor.intercom_rx_seq_gaps);
				}
			}
		}
		s_actor.intercom_rx_have_seq = true;
		s_actor.intercom_rx_last_seq = seq;

		const uint8_t *payload = frame + WS_INTERCOM_AUDIO_HEADER_BYTES;
		size_t sample_offset = 0;
		while(sample_offset < samples) {
			size_t chunk_samples = (size_t)samples - sample_offset;
			if(chunk_samples > AUDIO_STREAM_CHUNK_MAX_SAMPLES) {
				chunk_samples = AUDIO_STREAM_CHUNK_MAX_SAMPLES;
			}

			audio_stream_chunk_t chunk = {
				.sample_rate = sample_rate,
				.sample_count = chunk_samples,
			};
			if(codec == WS_INTERCOM_AUDIO_CODEC_G711_ULAW) {
				for(size_t i = 0; i < chunk_samples; i++) {
					chunk.samples[i] = ws_actor_ulaw_to_pcm16(payload[sample_offset + i]);
				}
			} else {
				for(size_t i = 0; i < chunk_samples; i++) {
					size_t pcm_index = (sample_offset + i) * 2U;
					chunk.samples[i] = (int16_t)ws_actor_read_le16(payload + pcm_index);
				}
			}

			(void)ws_actor_push_intercom_rx_chunk(&chunk);
			sample_offset += chunk_samples;
		}

		s_actor.intercom_rx_frames++;
		batch_frames++;
		offset += frame_len;
	}

	s_actor.intercom_rx_batches++;
	// if(s_actor.intercom_rx_batches <= 3 || (s_actor.intercom_rx_batches % 50U) == 0U) {
	// 	LOG("ws intercom rx batch: bytes=%u frames=%u total=%u prebuffer=%u stream=%d",
	// 		(unsigned)len,
	// 		(unsigned)batch_frames,
	// 		(unsigned)s_actor.intercom_rx_frames,
	// 		(unsigned)s_actor.intercom_rx_prebuffer_count,
	// 		(int)s_actor.intercom_rx_stream_started);
	// }
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

	(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_CW_RECEIVED, (int)seq, 0);
}

static void ws_actor_handle_intercom_talk_ack(const char *json)
{
	bool ok = false;
	if(!ws_actor_json_get_bool(json, "ok", &ok)) {
		LOG("ws intercom talk ack ignored: missing ok");
		return;
	}

	char reason[24] = {0};
	(void)ws_actor_json_get_string(json, "reason", reason, sizeof(reason));
	int ack_code = ok ? MSG_INTERCOM_TALK_ACK_OK : MSG_INTERCOM_TALK_ACK_LOCAL_ERROR;
	if(!ok && strcmp(reason, "busy") == 0) {
		ack_code = MSG_INTERCOM_TALK_ACK_BUSY;
	}
	if(!ok) {
		s_actor.intercom_tx_active = false;
	}
	LOG("ws intercom talk ack: ok=%d reason=%s code=%d",
		(int)ok,
		reason[0] != '\0' ? reason : "-",
		ack_code);

	(void)msg_send_sys_value(
		MSG_SRC_WS,
		MSG_EVT_SYS_INTERCOM_TALK_START_ACK,
		ack_code,
		0
	);
}

static void ws_actor_handle_intercom_talking(const char *json)
{
	bool talking = false;
	char callsign[sizeof(app_settings.user_callsign)] = {0};

	(void)ws_actor_json_get_bool(json, "talking", &talking);
	(void)ws_actor_json_get_string(json, "callsign", callsign, sizeof(callsign));
	// LOG("ws intercom talking: callsign=%s talking=%d",
	// 	callsign[0] != '\0' ? callsign : "-",
	// 	(int)talking);

	if(!s_actor.intercom_room_joined) {
		if(!talking) {
			ws_actor_stop_intercom_rx_audio("talking_false_not_in_room");
		}
		return;
	}

	(void)msg_send_sys_text(
		MSG_SRC_WS,
		MSG_EVT_SYS_INTERCOM_SPEAKER_CHANGED,
		talking ? callsign : "",
		0
	);

	if(talking) {
		ws_actor_start_intercom_rx_audio(callsign);
	} else {
		ws_actor_stop_intercom_rx_audio("talking_false");
		if(callsign[0] == '\0' || strcmp(callsign, app_settings.user_callsign) == 0 ||
		   strcmp(callsign, app_settings.ws_callsign) == 0) {
			s_actor.intercom_tx_active = false;
		}
	}
}

#if INTERCOM_ROOM_SYNC_ENABLE
static void ws_actor_cjson_copy_string(cJSON *object, const char *key, char *out, size_t out_size)
{
	if(out == NULL || out_size == 0) {
		return;
	}
	out[0] = '\0';
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	if(cJSON_IsString(item) && item->valuestring != NULL) {
		strncpy(out, item->valuestring, out_size - 1);
		out[out_size - 1] = '\0';
	}
}

static uint32_t ws_actor_cjson_u32(cJSON *object, const char *key)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	if(cJSON_IsNumber(item) && item->valuedouble >= 0) {
		return (uint32_t)item->valuedouble;
	}
	return 0;
}

static int ws_actor_cjson_int(cJSON *object, const char *key)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	return cJSON_IsNumber(item) ? item->valueint : 0;
}

static bool ws_actor_cjson_bool(cJSON *object, const char *key)
{
	cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
	return cJSON_IsTrue(item);
}

static void ws_actor_handle_room_list_snapshot(const char *json)
{
	cJSON *root = cJSON_Parse(json);
	if(root == NULL) {
		LOG("ws room list parse failed");
		return;
	}

	ws_room_snapshot_t *snapshot = (ws_room_snapshot_t *)calloc(1, sizeof(ws_room_snapshot_t));
	if(snapshot == NULL) {
		cJSON_Delete(root);
		return;
	}
	snapshot->revision = ws_actor_cjson_u32(root, "revision");
	snapshot->truncated = ws_actor_cjson_bool(root, "truncated");
	ws_actor_cjson_copy_string(root, "server_time", snapshot->server_time, sizeof(snapshot->server_time));

	cJSON *rooms = cJSON_GetObjectItemCaseSensitive(root, "rooms");
	if(cJSON_IsArray(rooms)) {
		cJSON *room = NULL;
		cJSON_ArrayForEach(room, rooms) {
			if(snapshot->count >= ROOM_LIST_MAX_COUNT) {
				snapshot->truncated = true;
				break;
			}
			if(!cJSON_IsObject(room)) {
				continue;
			}
			ws_room_record_t *out = &snapshot->rooms[snapshot->count];
			ws_actor_cjson_copy_string(room, "id", out->id, sizeof(out->id));
			ws_actor_cjson_copy_string(room, "name", out->name, sizeof(out->name));
			out->user_count = ws_actor_cjson_int(room, "user_count");
			out->locked = ws_actor_cjson_bool(room, "locked");
			if(out->id[0] == '\0') {
				continue;
			}
			if(out->name[0] == '\0') {
				strncpy(out->name, out->id, sizeof(out->name) - 1);
			}
			snapshot->count++;
		}
	}

	esp_err_t update_err = ws_room_cache_update_rooms(snapshot);
	LOG("ws room list snapshot: revision=%u count=%u truncated=%d server_time=%s update=%s",
		(unsigned)snapshot->revision,
		(unsigned)snapshot->count,
		(int)snapshot->truncated,
		snapshot->server_time[0] != '\0' ? snapshot->server_time : "-",
		esp_err_to_name(update_err));
	if(update_err == ESP_OK) {
		(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_ROOM_LIST_UPDATED, (int)snapshot->revision, 0);
	}
	free(snapshot);
	cJSON_Delete(root);
}

static void ws_actor_handle_room_users_snapshot(const char *json)
{
	cJSON *root = cJSON_Parse(json);
	if(root == NULL) {
		LOG("ws room users parse failed");
		return;
	}

	ws_room_users_snapshot_t *snapshot = (ws_room_users_snapshot_t *)calloc(1, sizeof(ws_room_users_snapshot_t));
	if(snapshot == NULL) {
		cJSON_Delete(root);
		return;
	}
	snapshot->revision = ws_actor_cjson_u32(root, "revision");
	snapshot->truncated = ws_actor_cjson_bool(root, "truncated");
	ws_actor_cjson_copy_string(root, "room", snapshot->room, sizeof(snapshot->room));
	ws_actor_cjson_copy_string(root, "server_time", snapshot->server_time, sizeof(snapshot->server_time));

	cJSON *users = cJSON_GetObjectItemCaseSensitive(root, "users");
	if(cJSON_IsArray(users)) {
		cJSON *user = NULL;
		cJSON_ArrayForEach(user, users) {
			if(snapshot->count >= ROOM_USERS_MAX_COUNT) {
				snapshot->truncated = true;
				break;
			}
			if(!cJSON_IsObject(user)) {
				continue;
			}
			ws_room_user_record_t *out = &snapshot->users[snapshot->count];
			ws_actor_cjson_copy_string(user, "device_id", out->device_id, sizeof(out->device_id));
			ws_actor_cjson_copy_string(user, "callsign", out->callsign, sizeof(out->callsign));
			ws_actor_cjson_copy_string(user, "fw_version", out->fw_version, sizeof(out->fw_version));
			out->talking = ws_actor_cjson_bool(user, "talking");
			if(out->callsign[0] == '\0') {
				strncpy(out->callsign, out->device_id[0] != '\0' ? out->device_id : "unknown", sizeof(out->callsign) - 1);
			}
			snapshot->count++;
		}
	}

	const char *current_room = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	if(snapshot->room[0] != '\0' && strcmp(snapshot->room, current_room) != 0) {
		// LOG("ws room users ignored: snapshot_room=%s current_room=%s revision=%u count=%u",
		// 	snapshot->room,
		// 	current_room,
		// 	(unsigned)snapshot->revision,
		// 	(unsigned)snapshot->count);
		free(snapshot);
		cJSON_Delete(root);
		return;
	}

	esp_err_t update_err = ws_room_cache_update_users(snapshot);
	LOG("ws room users snapshot: room=%s revision=%u count=%u truncated=%d server_time=%s update=%s",
		snapshot->room[0] != '\0' ? snapshot->room : "-",
		(unsigned)snapshot->revision,
		(unsigned)snapshot->count,
		(int)snapshot->truncated,
		snapshot->server_time[0] != '\0' ? snapshot->server_time : "-",
		esp_err_to_name(update_err));
	for(size_t i = 0; i < snapshot->count && i < 4; i++) {
		LOG("ws room user[%u]: callsign=%s device=%s talking=%d fw=%s",
			(unsigned)i,
			snapshot->users[i].callsign,
			snapshot->users[i].device_id,
			(int)snapshot->users[i].talking,
			snapshot->users[i].fw_version);
	}
	if(update_err == ESP_OK) {
		(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_ROOM_USERS_UPDATED, (int)snapshot->revision, 0);
	}
	free(snapshot);
	cJSON_Delete(root);
}
#endif

static void ws_actor_handle_ws_data(const ws_client_event_t *event)
{
	if(event == NULL) {
		return;
	}

	if(event->binary) {
		ws_actor_handle_intercom_audio_binary(event);
		return;
	}

	char *json = ws_actor_copy_ws_payload(event);
	if(json == NULL) {
		return;
	}

	char type[32] = {0};
	if(ws_actor_json_get_string(json, "type", type, sizeof(type)) && strcmp(type, "cw") == 0) {
		LOG("ws json received: type=cw len=%d", event->data_len);
		ws_actor_handle_cw_payload(json);
	} else if(strcmp(type, "intercom_talk_start_ack") == 0) {
		ws_actor_handle_intercom_talk_ack(json);
	} else if(strcmp(type, "intercom_talking") == 0) {
		ws_actor_handle_intercom_talking(json);
	} else if(strcmp(type, "room_list") == 0) {
#if INTERCOM_ROOM_SYNC_ENABLE
		if(s_actor.intercom_tx_active) {
			free(json);
			return;
		}
		ws_actor_handle_room_list_snapshot(json);
#else
		// LOG("ws room list ignored: room sync disabled len=%d", event->data_len);
#endif
	} else if(strcmp(type, "room_users") == 0) {
#if INTERCOM_ROOM_SYNC_ENABLE
		if(s_actor.intercom_tx_active) {
			free(json);
			return;
		}
		ws_actor_handle_room_users_snapshot(json);
#else
		// LOG("ws room users ignored: room sync disabled len=%d", event->data_len);
#endif
	} else if(s_actor.intercom_tx_active &&
			  (strcmp(type, "intercom_audio_diag") == 0 || strcmp(type, "pong") == 0)) {
		/* Keep the audio transmit window free of diagnostic/UI-only work. */
	} else {
		// LOG("ws json ignored: type=%s len=%d", type[0] != '\0' ? type : "-", event->data_len);
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

static void ws_actor_poll_time_sync(void)
{
	if(s_actor.state != WS_ACTOR_STATE_WAIT_TIME) {
		return;
	}

	if(!s_actor.wifi_connected && !wifi_module_is_connected()) {
		ws_actor_set_state(WS_ACTOR_STATE_WAIT_WIFI);
		return;
	}

	if(ws_actor_time_ready()) {
		s_actor.reconnect_delay_ms = 0;
		(void)ws_actor_connect();
		return;
	}

	if(ws_actor_tick_reached(xTaskGetTickCount(), s_actor.time_sync_deadline_tick)) {
		LOG("ws wait time sync timeout");
		ws_actor_schedule_reconnect();
	}
}

static esp_err_t ws_actor_send_intercom_audio_batch(const uint8_t *packet, size_t packet_bytes)
{
	if(packet == NULL || packet_bytes == 0) {
		return ESP_OK;
	}

	if(!ws_actor_ready_for_intercom_audio()) {
		return ESP_ERR_INVALID_STATE;
	}

	esp_err_t err = ws_client_send_binary_timeout(
		packet,
		(int)packet_bytes,
		pdMS_TO_TICKS(WS_AUDIO_SEND_TIMEOUT_MS)
	);
	if(err == ESP_OK) {
		s_actor.audio_send_consecutive_failures = 0;
		ws_actor_schedule_heartbeat();
		return ESP_OK;
	}

	s_actor.audio_send_consecutive_failures++;
	LOG("ws intercom audio send failed: err=%s bytes=%u consecutive=%u/%u",
		esp_err_to_name(err),
		(unsigned)packet_bytes,
		(unsigned)s_actor.audio_send_consecutive_failures,
		(unsigned)WS_AUDIO_SEND_MAX_CONSECUTIVE_FAILS);
	s_intercom_audio_tx_batch_count = 0;
	if(s_actor.audio_send_consecutive_failures >= WS_AUDIO_SEND_MAX_CONSECUTIVE_FAILS) {
		(void)msg_send_sys_value(MSG_SRC_WS, MSG_EVT_SYS_WS_DISCONNECTED, 0, 0);
	}
	return err;
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
			ws_actor_stop_intercom_rx_audio("wifi_stopping");
			s_actor.wifi_connected = false;
			s_actor.reconnect_delay_ms = 0;
			s_actor.next_heartbeat_tick = 0;
			s_actor.time_sync_deadline_tick = 0;
			s_intercom_audio_tx_batch_count = 0;
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
			ws_actor_stop_intercom_rx_audio("wifi_disconnected");
			s_actor.wifi_connected = false;
			s_actor.next_heartbeat_tick = 0;
			s_actor.time_sync_deadline_tick = 0;
			s_intercom_audio_tx_batch_count = 0;
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
				ws_actor_stop_intercom_rx_audio("ws_disabled");
				s_actor.next_heartbeat_tick = 0;
				s_actor.time_sync_deadline_tick = 0;
				s_intercom_audio_tx_batch_count = 0;
				(void)ws_client_stop();
				ws_actor_set_state(WS_ACTOR_STATE_IDLE);
			}
			break;

		case MSG_EVT_CMD_WS_RECONNECT:
			ws_actor_stop_intercom_rx_audio("ws_reconnect");
			s_actor.reconnect_delay_ms = 0;
			s_actor.next_heartbeat_tick = 0;
			s_actor.time_sync_deadline_tick = 0;
			s_actor.audio_send_consecutive_failures = 0;
			s_intercom_audio_tx_batch_count = 0;
			(void)ws_client_stop();
			(void)ws_actor_connect();
			break;

		case MSG_EVT_CMD_WS_SEND_CW:
			ws_actor_send_cw();
			break;

		case MSG_EVT_CMD_WS_ROOM_LIST_REQ:
#if INTERCOM_ROOM_SYNC_ENABLE
			if(s_actor.intercom_tx_active) {
				break;
			}
			ws_actor_send_room_list_req();
#else
			LOG("ws room list request ignored: room sync disabled");
#endif
			break;

		case MSG_EVT_CMD_WS_ROOM_USERS_REQ:
#if INTERCOM_ROOM_SYNC_ENABLE
			if(s_actor.intercom_tx_active) {
				break;
			}
			ws_actor_send_room_users_req();
#else
			LOG("ws room users request ignored: room sync disabled");
#endif
			break;

		case MSG_EVT_CMD_WS_INTERCOM_ROOM_JOIN:
			s_actor.intercom_room_joined = true;
			LOG("ws intercom room joined locally: room=%s",
				app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default");
#if INTERCOM_ROOM_SYNC_ENABLE
			ws_actor_send_intercom_room_presence("intercom_room_join");
#else
			// LOG("ws room join presence skipped: room sync disabled");
#endif
			break;

		case MSG_EVT_CMD_WS_INTERCOM_ROOM_LEAVE:
			s_actor.intercom_room_joined = false;
			ws_actor_stop_intercom_rx_audio("room_leave");
			LOG("ws intercom room left locally: room=%s",
				app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default");
#if INTERCOM_ROOM_SYNC_ENABLE
			ws_actor_send_intercom_room_presence("intercom_room_leave");
#else
			// LOG("ws room leave presence skipped: room sync disabled");
#endif
			break;

		case MSG_EVT_CMD_WS_INTERCOM_TALK_START:
			if(!s_actor.intercom_room_joined) {
				// LOG("ws intercom talk start ignored: not in intercom room");
				break;
			}
			ws_actor_stop_intercom_rx_audio("local_talk_start");
			s_actor.intercom_tx_active = true;
			ws_actor_send_intercom_signal("intercom_talk_start");
			break;

		case MSG_EVT_CMD_WS_INTERCOM_TALK_STOP:
			s_actor.intercom_tx_active = false;
			ws_actor_send_intercom_signal("intercom_talk_stop");
			break;

		case MSG_EVT_SYS_WS_CONNECTED:
			s_actor.reconnect_delay_ms = 0;
			s_actor.audio_send_consecutive_failures = 0;
			ws_actor_schedule_heartbeat();
			ws_actor_set_state(WS_ACTOR_STATE_CONNECTED);
			break;

		case MSG_EVT_SYS_WS_DISCONNECTED:
			ws_actor_stop_intercom_rx_audio("ws_disconnected");
			s_actor.next_heartbeat_tick = 0;
			s_actor.audio_send_consecutive_failures = 0;
			s_actor.intercom_tx_active = false;
			s_intercom_audio_tx_batch_count = 0;
			if(s_actor.state == WS_ACTOR_STATE_BACKOFF || s_actor.state == WS_ACTOR_STATE_WAIT_WIFI) {
				break;
			}
			(void)ws_client_deinit();
			s_actor.active_url[0] = '\0';
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

		ws_actor_poll_time_sync();
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
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
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
		vQueueDelete(s_actor.queue);
		s_actor.queue = NULL;
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

esp_err_t ws_actor_send_intercom_audio_frame(const int16_t *samples,
											 size_t sample_count,
											 uint16_t seq,
											 uint32_t sample_rate)
{
	if(samples == NULL || sample_count != INTERCOM_AUDIO_FRAME_SAMPLES) {
		return ESP_ERR_INVALID_ARG;
	}

	if(!ws_actor_ready_for_intercom_audio()) {
		s_intercom_audio_tx_batch_count = 0;
		return ESP_ERR_INVALID_STATE;
	}

	if(seq == 0) {
		s_intercom_audio_tx_batch_count = 0;
	}

	uint8_t *packet = s_intercom_audio_tx_batch +
					  (s_intercom_audio_tx_batch_count * WS_INTERCOM_AUDIO_FRAME_PACKET_BYTES);
	ws_intercom_audio_header_t *header = (ws_intercom_audio_header_t *)packet;
	header->magic[0] = WS_INTERCOM_AUDIO_MAGIC_0;
	header->magic[1] = WS_INTERCOM_AUDIO_MAGIC_1;
	header->version = WS_INTERCOM_AUDIO_VERSION;
#if INTERCOM_AUDIO_CODEC == INTERCOM_AUDIO_CODEC_G711_ULAW
	header->codec = WS_INTERCOM_AUDIO_CODEC_G711_ULAW;
#else
	header->codec = WS_INTERCOM_AUDIO_CODEC_PCM16;
#endif
	header->seq = seq;
	header->samples = (uint16_t)sample_count;
	header->sample_rate = sample_rate;

	(void)ws_actor_encode_intercom_payload(packet + sizeof(ws_intercom_audio_header_t), samples, sample_count);
	s_intercom_audio_tx_batch_count++;
	if(s_intercom_audio_tx_batch_count < WS_INTERCOM_AUDIO_BATCH_FRAMES) {
		return ESP_OK;
	}

	return ws_actor_flush_intercom_audio();
}

esp_err_t ws_actor_flush_intercom_audio(void)
{
	if(s_intercom_audio_tx_batch_count == 0) {
		return ESP_OK;
	}

	if(!ws_actor_ready_for_intercom_audio()) {
		s_intercom_audio_tx_batch_count = 0;
		return ESP_ERR_INVALID_STATE;
	}

	size_t packet_bytes = s_intercom_audio_tx_batch_count * WS_INTERCOM_AUDIO_FRAME_PACKET_BYTES;
	esp_err_t err = ws_actor_send_intercom_audio_batch(s_intercom_audio_tx_batch, packet_bytes);
	s_intercom_audio_tx_batch_count = 0;
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
