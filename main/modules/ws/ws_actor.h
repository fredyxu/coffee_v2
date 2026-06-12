#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws_actor_init(void);
esp_err_t ws_actor_deinit(void);
esp_err_t ws_actor_prepare_wifi_stop(TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
