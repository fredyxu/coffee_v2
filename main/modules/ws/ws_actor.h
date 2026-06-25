#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ws_actor_init(void);
esp_err_t ws_actor_deinit(void);
esp_err_t ws_actor_prepare_wifi_stop(TickType_t timeout_ticks);
esp_err_t ws_actor_send_intercom_audio_frame(const int16_t *samples,
											 size_t sample_count,
											 uint16_t seq,
											 uint32_t sample_rate);
esp_err_t ws_actor_flush_intercom_audio(void);

#ifdef __cplusplus
}
#endif
