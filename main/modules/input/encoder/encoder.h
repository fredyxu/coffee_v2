#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pin_a;
    int pin_b;
    int pin_sw;
    uint16_t sw_debounce_ms;
} encoder_config_t;

encoder_config_t encoder_default_config(void);
esp_err_t encoder_init(const encoder_config_t *cfg);
esp_err_t encoder_deinit(void);

bool encoder_is_ready(void);
int encoder_read_a(void);
int encoder_read_b(void);
int encoder_read_sw(void);

#ifdef __cplusplus
}
#endif
