#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include "modules/store/store.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t store_actor_init(void);
esp_err_t store_actor_deinit(void);

esp_err_t store_actor_load_wifi(store_wifi_settings_t *out_settings);
esp_err_t store_actor_save_wifi(const store_wifi_settings_t *settings, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
