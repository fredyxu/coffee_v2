#include "encoder_actor.h"

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"
#include "modules/input/encoder/encoder.h"

typedef struct {
    int pin_a;
    int pin_b;
    int pin_sw;
    uint16_t sw_debounce_ms;
    uint16_t sw_long_press_ms;
    uint8_t prev_state;
    int8_t acc;
    TickType_t last_sw_edge_tick;
    TickType_t sw_press_tick;
    bool sw_pressed;
    bool sw_long_sent;
} encoder_actor_ctx_t;

static TaskHandle_t s_task = NULL;
static QueueHandle_t s_evt_q = NULL;
static encoder_actor_ctx_t s_ctx = {0};

static void IRAM_ATTR encoder_gpio_isr(void *arg)
{
    const uint32_t gpio_num = (uint32_t)(uintptr_t)arg;
    BaseType_t hp_task_woken = pdFALSE;
    if(s_evt_q) {
        (void)xQueueSendFromISR(s_evt_q, &gpio_num, &hp_task_woken);
    }
    if(hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void encoder_post_input_event(msg_event_t event)
{
    msg_t msg = msg_make(MSG_SRC_ENCODER, MSG_TYPE_INPUT, event, (uint32_t)xTaskGetTickCount());
    (void)msg_send_input(&msg, 0);
}

static void encoder_process_ab(void)
{
    static const int8_t trans_lut[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0,
    };

    uint8_t a = (uint8_t)(encoder_read_a() & 0x1);
    uint8_t b = (uint8_t)(encoder_read_b() & 0x1);
    uint8_t cur = (uint8_t)((a << 1) | b);

    uint8_t idx = (uint8_t)((s_ctx.prev_state << 2) | cur);
    s_ctx.prev_state = cur;
    s_ctx.acc += trans_lut[idx];

    if(s_ctx.acc >= 4) {
        s_ctx.acc = 0;
        encoder_post_input_event(MSG_EVT_INPUT_ENCODER_CW);
    } else if(s_ctx.acc <= -4) {
        s_ctx.acc = 0;
        encoder_post_input_event(MSG_EVT_INPUT_ENCODER_CCW);
    }
}

static void encoder_process_sw(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t debounce_ticks = pdMS_TO_TICKS(s_ctx.sw_debounce_ms);

    if((now - s_ctx.last_sw_edge_tick) < debounce_ticks) {
        return;
    }
    s_ctx.last_sw_edge_tick = now;

    if(encoder_read_sw() == 0) {
        if(!s_ctx.sw_pressed) {
            s_ctx.sw_pressed = true;
            s_ctx.sw_long_sent = false;
            s_ctx.sw_press_tick = now;
        }
        return;
    }

    if(s_ctx.sw_pressed) {
        if(!s_ctx.sw_long_sent) {
            encoder_post_input_event(MSG_EVT_INPUT_ENCODER_PRESS);
        } else {
            encoder_post_input_event(MSG_EVT_INPUT_ENCODER_RELEASE);
        }
        s_ctx.sw_pressed = false;
        s_ctx.sw_long_sent = false;
    }
}

static TickType_t encoder_sw_wait_ticks(void)
{
    if(s_ctx.sw_pressed && !s_ctx.sw_long_sent) {
        TickType_t now = xTaskGetTickCount();
        TickType_t long_press_ticks = pdMS_TO_TICKS(s_ctx.sw_long_press_ms);
        TickType_t elapsed = now - s_ctx.sw_press_tick;
        if(elapsed >= long_press_ticks) {
            return 0;
        }

        return long_press_ticks - elapsed;
    }

    return portMAX_DELAY;
}

static void encoder_process_sw_long_press(void)
{
    if(!s_ctx.sw_pressed || s_ctx.sw_long_sent) {
        return;
    }

    TickType_t now = xTaskGetTickCount();
    TickType_t long_press_ticks = pdMS_TO_TICKS(s_ctx.sw_long_press_ms);
    if((now - s_ctx.sw_press_tick) < long_press_ticks) {
        return;
    }

    if(encoder_read_sw() != 0) {
        s_ctx.sw_pressed = false;
        return;
    }

    s_ctx.sw_long_sent = true;
    encoder_post_input_event(MSG_EVT_INPUT_ENCODER_LONG_PRESS);
}

static void encoder_actor_task(void *arg)
{
    (void)arg;

    uint32_t gpio_num = 0;
    while(1) {
        if(xQueueReceive(s_evt_q, &gpio_num, encoder_sw_wait_ticks()) != pdTRUE) {
            encoder_process_sw_long_press();
            continue;
        }

        if((int)gpio_num == s_ctx.pin_a || (int)gpio_num == s_ctx.pin_b) {
            encoder_process_ab();
        } else if((int)gpio_num == s_ctx.pin_sw) {
            encoder_process_sw();
        }
    }
}

esp_err_t encoder_actor_init(void)
{
    if(s_task != NULL) {
        return ESP_OK;
    }

    encoder_config_t cfg = encoder_default_config();
    esp_err_t err = encoder_init(&cfg);
    if(err != ESP_OK) {
        LOG("encoder_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_ctx.pin_a = cfg.pin_a;
    s_ctx.pin_b = cfg.pin_b;
    s_ctx.pin_sw = cfg.pin_sw;
    s_ctx.sw_debounce_ms = cfg.sw_debounce_ms;
    s_ctx.sw_long_press_ms = cfg.sw_long_press_ms;
    s_ctx.prev_state = (uint8_t)(((encoder_read_a() & 0x1) << 1) | (encoder_read_b() & 0x1));
    s_ctx.acc = 0;
    s_ctx.last_sw_edge_tick = 0;
    s_ctx.sw_press_tick = 0;
    s_ctx.sw_pressed = false;
    s_ctx.sw_long_sent = false;

    s_evt_q = xQueueCreate(32, sizeof(uint32_t));
    if(s_evt_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = gpio_install_isr_service(0);
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG("gpio_install_isr_service failed: %s", esp_err_to_name(err));
        goto fail;
    }

    err = gpio_set_intr_type(s_ctx.pin_a, GPIO_INTR_ANYEDGE);
    if(err != ESP_OK) goto fail;
    err = gpio_set_intr_type(s_ctx.pin_b, GPIO_INTR_ANYEDGE);
    if(err != ESP_OK) goto fail;
    err = gpio_set_intr_type(s_ctx.pin_sw, GPIO_INTR_ANYEDGE);
    if(err != ESP_OK) goto fail;

    err = gpio_isr_handler_add(s_ctx.pin_a, encoder_gpio_isr, (void *)(uintptr_t)s_ctx.pin_a);
    if(err != ESP_OK) goto fail;
    err = gpio_isr_handler_add(s_ctx.pin_b, encoder_gpio_isr, (void *)(uintptr_t)s_ctx.pin_b);
    if(err != ESP_OK) goto fail;
    err = gpio_isr_handler_add(s_ctx.pin_sw, encoder_gpio_isr, (void *)(uintptr_t)s_ctx.pin_sw);
    if(err != ESP_OK) goto fail;

    BaseType_t ok = xTaskCreatePinnedToCore(
        encoder_actor_task,
        "encoder_actor",
        4096,
        NULL,
        TASK_PRIO_ENCODER,
        &s_task,
        TASK_CORE_ENCODER
    );
    if(ok != pdPASS) {
        err = ESP_FAIL;
        goto fail;
    }

    // LOG("encoder actor init done (A=%d B=%d SW=%d)", s_ctx.pin_a, s_ctx.pin_b, s_ctx.pin_sw);
    return ESP_OK;

fail:
    gpio_isr_handler_remove(s_ctx.pin_a);
    gpio_isr_handler_remove(s_ctx.pin_b);
    gpio_isr_handler_remove(s_ctx.pin_sw);
    if(s_evt_q) {
        vQueueDelete(s_evt_q);
        s_evt_q = NULL;
    }
    (void)encoder_deinit();
    return err;
}

esp_err_t encoder_actor_deinit(void)
{
    if(s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }

    gpio_isr_handler_remove(s_ctx.pin_a);
    gpio_isr_handler_remove(s_ctx.pin_b);
    gpio_isr_handler_remove(s_ctx.pin_sw);

    if(s_evt_q) {
        vQueueDelete(s_evt_q);
        s_evt_q = NULL;
    }

    return encoder_deinit();
}
