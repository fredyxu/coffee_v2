#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_STREAM_CHUNK_MAX_SAMPLES 512

typedef struct {
    uint32_t sample_rate;
    size_t sample_count;
    int16_t samples[AUDIO_STREAM_CHUNK_MAX_SAMPLES];
} audio_stream_chunk_t;

esp_err_t audio_actor_init(void);
esp_err_t audio_actor_deinit(void);

esp_err_t audio_actor_play_tone(uint32_t freq_hz, uint32_t duration_ms, TickType_t timeout_ticks);
esp_err_t audio_actor_tone_on(uint32_t freq_hz, TickType_t timeout_ticks);
esp_err_t audio_actor_tone_off(TickType_t timeout_ticks);
esp_err_t audio_actor_play_file(const char *path, TickType_t timeout_ticks);
esp_err_t audio_actor_stop(TickType_t timeout_ticks);

esp_err_t audio_actor_stream_start(uint32_t sample_rate, TickType_t timeout_ticks);
esp_err_t audio_actor_stream_stop(TickType_t timeout_ticks);
esp_err_t audio_actor_stream_push(const int16_t *samples, size_t sample_count, uint32_t sample_rate, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
