#include "modules/key/key.h"

#include <stddef.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_timer.h"

typedef struct {
    bool configured;
    int gpio;
    bool stable_pressed;
    bool raw_pressed;
    int64_t raw_changed_at_ms;
} key_input_state_t;

typedef struct {
    bool inited;
    int active_level;
    uint32_t debounce_ms;
    key_input_state_t inputs[KEY_INPUT_COUNT];
} key_ctx_t;

static key_ctx_t s_key = {0};

static int64_t key_now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static bool key_gpio_pressed(int gpio)
{
    return gpio_get_level((gpio_num_t)gpio) == s_key.active_level;
}

static esp_err_t key_config_input(key_input_t input, int gpio)
{
    if(gpio < 0) {
        s_key.inputs[input].configured = false;
        s_key.inputs[input].gpio = gpio;
        return ESP_OK;
    }

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << (uint32_t)gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t err = gpio_config(&cfg);
    if(err != ESP_OK) {
        return err;
    }

    bool pressed = key_gpio_pressed(gpio);
    s_key.inputs[input] = (key_input_state_t) {
        .configured = true,
        .gpio = gpio,
        .stable_pressed = pressed,
        .raw_pressed = pressed,
        .raw_changed_at_ms = key_now_ms(),
    };
    return ESP_OK;
}

esp_err_t key_init(const key_config_t *cfg)
{
    if(cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)key_deinit();
    memset(&s_key, 0, sizeof(s_key));
    s_key.active_level = cfg->active_level ? 1 : 0;
    s_key.debounce_ms = cfg->debounce_ms;

    esp_err_t err = key_config_input(KEY_INPUT_A, cfg->gpio_a);
    if(err != ESP_OK) {
        return err;
    }
    err = key_config_input(KEY_INPUT_B, cfg->gpio_b);
    if(err != ESP_OK) {
        return err;
    }

    s_key.inited = true;
    return ESP_OK;
}

esp_err_t key_deinit(void)
{
    if(!s_key.inited) {
        return ESP_OK;
    }

    for(size_t i = 0; i < KEY_INPUT_COUNT; i++) {
        if(s_key.inputs[i].configured && s_key.inputs[i].gpio >= 0) {
            (void)gpio_reset_pin((gpio_num_t)s_key.inputs[i].gpio);
        }
    }

    memset(&s_key, 0, sizeof(s_key));
    return ESP_OK;
}

esp_err_t key_poll(key_input_event_t *events, size_t max_events, size_t *out_count)
{
    if(events == NULL || out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_count = 0;
    if(!s_key.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    int64_t now = key_now_ms();
    for(size_t i = 0; i < KEY_INPUT_COUNT && *out_count < max_events; i++) {
        key_input_state_t *state = &s_key.inputs[i];
        if(!state->configured) {
            continue;
        }

        bool raw = key_gpio_pressed(state->gpio);
        if(raw != state->raw_pressed) {
            state->raw_pressed = raw;
            state->raw_changed_at_ms = now;
            continue;
        }

        if(raw != state->stable_pressed &&
           (uint32_t)(now - state->raw_changed_at_ms) >= s_key.debounce_ms) {
            state->stable_pressed = raw;
            events[*out_count] = (key_input_event_t) {
                .changed = true,
                .input = (key_input_t)i,
                .pressed = raw,
                .timestamp_ms = (uint32_t)now,
            };
            (*out_count)++;
        }
    }

    return ESP_OK;
}

bool key_is_pressed(key_input_t input)
{
    if(input < 0 || input >= KEY_INPUT_COUNT) {
        return false;
    }

    return s_key.inputs[input].stable_pressed;
}
