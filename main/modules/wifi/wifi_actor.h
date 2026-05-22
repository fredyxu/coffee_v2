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
} wifi_actor_config_t;

wifi_actor_config_t wifi_actor_default_config(void);

esp_err_t wifi_actor_init(void);
esp_err_t wifi_actor_deinit(void);

#ifdef __cplusplus
}
#endif
