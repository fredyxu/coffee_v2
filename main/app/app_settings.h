#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#else
typedef uint32_t TickType_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_SETTING_ID_WIFI_ENABLE = 0,
    APP_SETTING_ID_WIFI_SSID,
    APP_SETTING_ID_WIFI_PASSWORD,
    APP_SETTING_ID_WIFI_AUTO_RECONNECT,
    APP_SETTING_ID_WIFI_WEAK_RSSI_THRESHOLD,
    APP_SETTING_ID_AUDIO_VOLUME,
    APP_SETTING_ID_DISPLAY_BRIGHTNESS,
    APP_SETTING_ID_MAX,
} app_setting_id_t;

typedef union {
    bool b;
    int32_t i32;
    const char *str;
} app_setting_value_t;

typedef struct {
    app_setting_id_t id;
    app_setting_value_t value;
} app_settings_update_t;

typedef struct {
    bool wifi_enable;
    char wifi_ssid[33];
    char wifi_password[65];
    bool wifi_auto_reconnect;
    int32_t wifi_weak_rssi_threshold;
    int32_t audio_volume;
    int32_t display_brightness;
} app_settings_t;

extern app_settings_t app_settings;

esp_err_t app_settings_init(void);
esp_err_t app_settings_update(const app_settings_update_t *update);
esp_err_t app_settings_flush(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
