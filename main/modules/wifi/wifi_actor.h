#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char password[65];
    bool auto_reconnect;
    int weak_rssi_threshold;
} wifi_actor_config_t;

wifi_actor_config_t wifi_actor_default_config(void);

esp_err_t wifi_actor_init(void);
esp_err_t wifi_actor_deinit(void);

esp_err_t wifi_actor_start(TickType_t timeout_ticks);
esp_err_t wifi_actor_stop(TickType_t timeout_ticks);
esp_err_t wifi_actor_connect(TickType_t timeout_ticks);
esp_err_t wifi_actor_disconnect(TickType_t timeout_ticks);
esp_err_t wifi_actor_scan(TickType_t timeout_ticks);

esp_err_t wifi_actor_set_credentials(const char *ssid, const char *password, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
