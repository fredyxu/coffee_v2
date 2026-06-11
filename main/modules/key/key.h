#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_INPUT_A = 0,
    KEY_INPUT_B,
    KEY_INPUT_COUNT,
} key_input_t;

typedef struct {
    int gpio_a;
    int gpio_b;
    int active_level;
    uint32_t debounce_ms;
} key_config_t;

typedef struct {
    bool changed;
    key_input_t input;
    bool pressed;
    uint32_t timestamp_ms;
} key_input_event_t;

esp_err_t key_init(const key_config_t *cfg);
esp_err_t key_deinit(void);
esp_err_t key_poll(key_input_event_t *events, size_t max_events, size_t *out_count);
bool key_is_pressed(key_input_t input);

#ifdef __cplusplus
}
#endif
