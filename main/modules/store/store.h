#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char password[65];
    bool auto_reconnect;
    int weak_rssi_threshold;
} store_wifi_settings_t;

typedef struct {
    store_wifi_settings_t wifi;
} store_settings_t;

store_wifi_settings_t store_default_wifi_settings(void);

esp_err_t store_init(void);
esp_err_t store_deinit(void);

esp_err_t store_load_wifi_settings(store_wifi_settings_t *out_settings);
esp_err_t store_save_wifi_settings(const store_wifi_settings_t *settings);

esp_err_t store_load_settings(store_settings_t *out_settings);
esp_err_t store_save_settings(const store_settings_t *settings);

#ifdef __cplusplus
}
#endif
