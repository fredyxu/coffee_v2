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
    APP_SETTING_ID_WS_ENABLE,
    APP_SETTING_ID_WS_URL,
    APP_SETTING_ID_WS_ROOM,
    APP_SETTING_ID_WS_CALLSIGN,
    APP_SETTING_ID_WS_AUTO_RECONNECT,
    APP_SETTING_ID_KEY_ENABLE,
    APP_SETTING_ID_KEY_MODE,
    APP_SETTING_ID_KEY_ACTIVE_LEVEL,
    APP_SETTING_ID_KEY_SWAP_AB,
    APP_SETTING_ID_KEY_DEBOUNCE_MS,
    APP_SETTING_ID_KEY_MANUAL_DI_MS,
    APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
    APP_SETTING_ID_KEY_AUTO_DI_MS,
    APP_SETTING_ID_KEY_AUTO_DA_RATIO,
    APP_SETTING_ID_KEY_AUTO_GAP_RATIO,
    APP_SETTING_ID_KEY_TONE_HZ,
    APP_SETTING_ID_AUDIO_VOLUME,
    APP_SETTING_ID_DISPLAY_BRIGHTNESS,
    APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
    APP_SETTING_ID_MORSE_AUTO_SEND_ENABLE,
    APP_SETTING_ID_MORSE_AUTO_SEND_DELAY_MS,
	APP_SETTING_ID_USER_CALLSIGN,
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
    bool ws_enable;
    char ws_url[129];
    char ws_room[33];
    char ws_callsign[17];
    bool ws_auto_reconnect;
    bool key_enable;
    int32_t key_mode;
    int32_t key_active_level;
    bool key_swap_ab;
    int32_t key_debounce_ms;
    int32_t key_manual_di_ms;
    bool key_manual_adaptive_enable;
    int32_t key_auto_di_ms;
    char key_auto_da_ratio[8];
    char key_auto_gap_ratio[8];
    int32_t key_tone_hz;
    int32_t audio_volume;
    int32_t display_brightness;
    bool cw_decode_display_enable;
    bool morse_auto_send_enable;
    int32_t morse_auto_send_delay_ms;
	char user_callsign[65];
} app_settings_t;

extern app_settings_t app_settings;

esp_err_t app_settings_init(void);
esp_err_t app_settings_update(const app_settings_update_t *update);
esp_err_t app_settings_flush(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
