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
    int dout_io;
    uint32_t sample_rate;
    uint8_t volume_percent;
} audio_config_t;

audio_config_t audio_default_config(void);
esp_err_t audio_init(const audio_config_t *cfg);
esp_err_t audio_deinit(void);

bool audio_is_ready(void);
uint32_t audio_get_sample_rate(void);
uint8_t audio_get_volume(void);

esp_err_t audio_set_sample_rate(uint32_t sample_rate);
esp_err_t audio_set_volume(uint8_t volume_percent);

esp_err_t audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);
esp_err_t audio_play_pcm16(const int16_t *samples, size_t sample_count, TickType_t timeout_ticks);
esp_err_t audio_play_stream_chunk(const int16_t *samples, size_t sample_count, TickType_t timeout_ticks);
esp_err_t audio_play_file(const char *path);
esp_err_t audio_stop(void);

#ifdef __cplusplus
}
#endif
