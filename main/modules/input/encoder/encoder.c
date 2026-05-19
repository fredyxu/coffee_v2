#include "encoder.h"

#include <stdbool.h>
#include "driver/gpio.h"
#include "config/config_pin.h"

static encoder_config_t s_cfg = {0};
static bool s_ready = false;

encoder_config_t encoder_default_config(void)
{
    encoder_config_t cfg = {
        .pin_a = PIN_ENCODER_A,
        .pin_b = PIN_ENCODER_B,
        .pin_sw = PIN_ENCODER_SW,
        .sw_debounce_ms = 25,
        .sw_long_press_ms = 800,
    };
    return cfg;
}

esp_err_t encoder_init(const encoder_config_t *cfg)
{
    if(s_ready) {
        return ESP_OK;
    }

    if(cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(cfg->pin_a < 0 || cfg->pin_b < 0 || cfg->pin_sw < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_cfg = *cfg;

    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << (uint32_t)s_cfg.pin_a) |
                        (1ULL << (uint32_t)s_cfg.pin_b) |
                        (1ULL << (uint32_t)s_cfg.pin_sw),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&io_cfg);
    if(err != ESP_OK) {
        return err;
    }

    s_ready = true;
    return ESP_OK;
}

esp_err_t encoder_deinit(void)
{
    s_ready = false;
    return ESP_OK;
}

bool encoder_is_ready(void)
{
    return s_ready;
}

int encoder_read_a(void)
{
    return gpio_get_level(s_cfg.pin_a);
}

int encoder_read_b(void)
{
    return gpio_get_level(s_cfg.pin_b);
}

int encoder_read_sw(void)
{
    return gpio_get_level(s_cfg.pin_sw);
}
