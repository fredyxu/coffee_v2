#include "modules/ws/ws_client.h"

#include <string.h>

#include "config/config_sys.h"
#include "core/utils/log.h"
#include "esp_crt_bundle.h"
#include "esp_websocket_client.h"

static esp_websocket_client_handle_t s_client;
static ws_client_event_cb_t s_event_cb;
static void *s_event_user_ctx;
static bool s_connected;

static void ws_client_emit(ws_client_event_id_t id, const char *data, int data_len, bool binary)
{
	if(s_event_cb == NULL) {
		return;
	}

	ws_client_event_t event = {
		.id = id,
		.data = data,
		.data_len = data_len,
		.binary = binary,
	};
	s_event_cb(&event, s_event_user_ctx);
}

static void ws_client_event_handler(void *handler_args,
									esp_event_base_t base,
									int32_t event_id,
									void *event_data)
{
	(void)handler_args;
	(void)base;

	esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

	switch(event_id) {
		case WEBSOCKET_EVENT_CONNECTED:
			s_connected = true;
			ws_client_emit(WS_CLIENT_EVT_CONNECTED, NULL, 0, false);
			break;

		case WEBSOCKET_EVENT_DISCONNECTED:
			s_connected = false;
			ws_client_emit(WS_CLIENT_EVT_DISCONNECTED, NULL, 0, false);
			break;

		case WEBSOCKET_EVENT_DATA:
			if(data != NULL) {
				ws_client_emit(
					WS_CLIENT_EVT_DATA,
					data->data_ptr,
					data->data_len,
					data->op_code == WS_TRANSPORT_OPCODES_BINARY
				);
			}
			break;

		case WEBSOCKET_EVENT_ERROR:
			s_connected = false;
			ws_client_emit(WS_CLIENT_EVT_ERROR, NULL, 0, false);
			break;

		default:
			break;
	}
}

esp_err_t ws_client_init(const ws_client_config_t *cfg)
{
	if(cfg == NULL || cfg->url == NULL || cfg->url[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	(void)ws_client_deinit();

	esp_websocket_client_config_t websocket_cfg = {
		.uri = cfg->url,
		.buffer_size = 1024,
		.crt_bundle_attach = esp_crt_bundle_attach,
		.disable_auto_reconnect = true,
		.network_timeout_ms = 10000,
	};

	s_client = esp_websocket_client_init(&websocket_cfg);
	if(s_client == NULL) {
		return ESP_FAIL;
	}

	s_event_cb = cfg->event_cb;
	s_event_user_ctx = cfg->event_user_ctx;
	s_connected = false;

	esp_err_t err = esp_websocket_register_events(
		s_client,
		WEBSOCKET_EVENT_ANY,
		ws_client_event_handler,
		NULL
	);
	if(err != ESP_OK) {
		LOG("ws register events failed: %s", esp_err_to_name(err));
		(void)ws_client_deinit();
		return err;
	}

	return ESP_OK;
}

esp_err_t ws_client_deinit(void)
{
	if(s_client == NULL) {
		return ESP_OK;
	}

	if(esp_websocket_client_is_connected(s_client)) {
		(void)esp_websocket_client_stop(s_client);
	}

	esp_err_t err = esp_websocket_client_destroy(s_client);
	s_client = NULL;
	s_event_cb = NULL;
	s_event_user_ctx = NULL;
	s_connected = false;
	return err;
}

esp_err_t ws_client_start(void)
{
	if(s_client == NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	return esp_websocket_client_start(s_client);
}

esp_err_t ws_client_stop(void)
{
	if(s_client == NULL) {
		return ESP_OK;
	}

	s_connected = false;
	return esp_websocket_client_stop(s_client);
}

bool ws_client_is_connected(void)
{
	return s_client != NULL && s_connected && esp_websocket_client_is_connected(s_client);
}

esp_err_t ws_client_send_text(const char *text)
{
	if(text == NULL || !ws_client_is_connected()) {
		return ESP_ERR_INVALID_STATE;
	}

	int written = esp_websocket_client_send_text(
		s_client,
		text,
		(int)strlen(text),
		pdMS_TO_TICKS(WS_SEND_TIMEOUT_MS)
	);
	return written >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t ws_client_send_binary(const void *data, int len)
{
	if(data == NULL || len <= 0 || !ws_client_is_connected()) {
		return ESP_ERR_INVALID_STATE;
	}

	int written = esp_websocket_client_send_bin(
		s_client,
		data,
		len,
		pdMS_TO_TICKS(WS_SEND_TIMEOUT_MS)
	);
	return written >= 0 ? ESP_OK : ESP_FAIL;
}
