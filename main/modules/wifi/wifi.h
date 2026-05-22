#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char password[65];
    bool auto_reconnect;
} wifi_module_config_t;

typedef enum {
    WIFI_MOD_EVT_STA_START,
    WIFI_MOD_EVT_STA_CONNECTED,
    WIFI_MOD_EVT_STA_DISCONNECTED,
    WIFI_MOD_EVT_GOT_IP,
    WIFI_MOD_EVT_SCAN_AP_FOUND,
    WIFI_MOD_EVT_SCAN_DONE,
    WIFI_MOD_EVT_SCAN_FAILED,
} wifi_module_event_id_t;

typedef struct {
    wifi_module_event_id_t id;
    int reason;		// 事件原因
    int rssi;		// 信号强度
    uint32_t ip;	// 获取的IP
    uint16_t ap_count;	// 获取到的AP数量
    uint8_t authmode;	// WiFi认证类型
    uint8_t channel;	// WiFi信道
    char ssid[33];	// SSID名称
} wifi_module_event_t;

typedef void (*wifi_module_event_cb_t)(const wifi_module_event_t *event, void *user_ctx);

wifi_module_config_t wifi_module_default_config(void);

esp_err_t wifi_module_init(const wifi_module_config_t *cfg);
esp_err_t wifi_module_deinit(void);

esp_err_t wifi_module_start(void);
esp_err_t wifi_module_stop(void);
esp_err_t wifi_module_connect(void);
esp_err_t wifi_module_disconnect(void);
esp_err_t wifi_module_scan(void);

esp_err_t wifi_module_set_credentials(const char *ssid, const char *password);

bool wifi_module_is_ready(void);
bool wifi_module_is_connected(void);
esp_err_t wifi_module_get_rssi(int *out_rssi);

esp_err_t wifi_module_register_event_callback(wifi_module_event_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
