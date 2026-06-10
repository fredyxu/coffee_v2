#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	WS_CLIENT_EVT_CONNECTED = 0,
	WS_CLIENT_EVT_DISCONNECTED,
	WS_CLIENT_EVT_ERROR,
	WS_CLIENT_EVT_DATA,
} ws_client_event_id_t;

typedef struct {
	ws_client_event_id_t id;
	const char *data;
	int data_len;
	bool binary;
} ws_client_event_t;

typedef void (*ws_client_event_cb_t)(const ws_client_event_t *event, void *user_ctx);

typedef struct {
	const char *url;
	ws_client_event_cb_t event_cb;
	void *event_user_ctx;
} ws_client_config_t;

esp_err_t ws_client_init(const ws_client_config_t *cfg);
esp_err_t ws_client_deinit(void);
esp_err_t ws_client_start(void);
esp_err_t ws_client_stop(void);
bool ws_client_is_connected(void);
esp_err_t ws_client_send_text(const char *text);
esp_err_t ws_client_send_binary(const void *data, int len);

#ifdef __cplusplus
}
#endif
