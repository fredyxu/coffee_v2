#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIC_ACTOR_FRAME_MAX_SAMPLES 320

typedef struct {
    uint32_t sample_rate;
    size_t sample_count;
    uint32_t timestamp;
    int16_t samples[MIC_ACTOR_FRAME_MAX_SAMPLES];
} mic_frame_t;

esp_err_t mic_actor_init(void);
esp_err_t mic_actor_deinit(void);

esp_err_t mic_actor_start(TickType_t timeout_ticks);
esp_err_t mic_actor_stop(TickType_t timeout_ticks);
esp_err_t mic_actor_set_sample_rate(uint32_t sample_rate, TickType_t timeout_ticks);

esp_err_t mic_actor_pop_frame(mic_frame_t *out_frame, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
