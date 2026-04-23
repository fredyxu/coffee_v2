#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int bclk_io;
    int ws_io;
    int din_io;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
} mic_config_t;

mic_config_t mic_default_config(void);
esp_err_t mic_init(const mic_config_t *cfg);
esp_err_t mic_deinit(void);

bool mic_is_ready(void);
uint32_t mic_get_sample_rate(void);

esp_err_t mic_set_sample_rate(uint32_t sample_rate);
esp_err_t mic_read_raw(void *buf, size_t bytes, size_t *out_bytes, TickType_t timeout_ticks);
esp_err_t mic_read_pcm16(int16_t *samples, size_t sample_count, TickType_t timeout_ticks);
esp_err_t mic_read_pcm16_some(int16_t *samples, size_t max_samples, size_t *out_samples);

#ifdef __cplusplus
}
#endif
