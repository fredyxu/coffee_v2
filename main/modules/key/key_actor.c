#include "modules/key/key_actor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app/app_settings.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "config/config_sys_key.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "modules/key/key.h"

typedef void (*key_input_handler_t)(const key_input_event_t *event);

static void key_actor_handle_manual_input_event(const key_input_event_t *event);
static void key_actor_handle_auto_input_event(const key_input_event_t *event);

typedef enum {
    KEY_AUTO_SYMBOL_DI = 0,
    KEY_AUTO_SYMBOL_DA,
} key_auto_symbol_t;

typedef struct {
    bool initialized;
    QueueHandle_t queue;
    msg_sub_handle_t sub_handle;
    TaskHandle_t task;
    bool key_down;
    bool auto_di_down;
    bool auto_da_down;
    bool auto_tone_on;
    bool auto_gap_active;
    key_auto_symbol_t auto_current_symbol;
    key_auto_symbol_t auto_preferred_symbol;
    uint32_t auto_deadline_ms;
    uint32_t key_down_at_ms;
    uint32_t last_symbol_at_ms;
    bool group_gap_pending;
    uint32_t manual_di_estimate_ms;
} key_actor_ctx_t;

static key_actor_ctx_t s_actor = {
    .sub_handle = MSG_SUB_HANDLE_INVALID,
};
static key_input_handler_t s_key_input_handler = key_actor_handle_manual_input_event;

static uint32_t key_actor_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static uint32_t key_actor_manual_di_ms(void)
{
    if(s_actor.manual_di_estimate_ms > 0) {
        return s_actor.manual_di_estimate_ms;
    }
    if(app_settings.key_manual_di_ms > 0) {
        return (uint32_t)app_settings.key_manual_di_ms;
    }
    return KEY_DEFAULT_MANUAL_DI_MS;
}

static uint32_t key_actor_auto_di_ms(void)
{
    if(app_settings.key_auto_di_ms > 0) {
        return (uint32_t)app_settings.key_auto_di_ms;
    }

    uint32_t manual_di_ms = key_actor_manual_di_ms();
    uint32_t auto_di_ms = (uint32_t)((float)manual_di_ms * KEY_AUTO_DI_FALLBACK_RATIO);
    return auto_di_ms > 0 ? auto_di_ms : 1;
}

static float key_actor_auto_da_ratio(void)
{
    float ratio = strtof(app_settings.key_auto_da_ratio, NULL);
    if(ratio <= 0.0f) {
        ratio = strtof(KEY_DEFAULT_AUTO_DA_RATIO, NULL);
    }
    if(ratio <= 0.0f) {
        ratio = 2.2f;
    }
    return ratio;
}

static float key_actor_auto_gap_ratio(void)
{
    float ratio = strtof(app_settings.key_auto_gap_ratio, NULL);
    if(ratio <= 0.0f) {
        ratio = strtof(KEY_DEFAULT_AUTO_GAP_RATIO, NULL);
    }
    if(ratio <= 0.0f) {
        ratio = 0.5f;
    }
    if(ratio < 0.1f) {
        ratio = 0.1f;
    }
    return ratio;
}

static uint32_t key_actor_auto_symbol_duration_ms(key_auto_symbol_t symbol)
{
    uint32_t di_ms = key_actor_auto_di_ms();
    if(symbol == KEY_AUTO_SYMBOL_DI) {
        return di_ms;
    }

    uint32_t da_ms = (uint32_t)((float)di_ms * key_actor_auto_da_ratio());
    return da_ms > 0 ? da_ms : 1;
}

static uint32_t key_actor_auto_gap_ms(void)
{
    uint32_t gap_ms = (uint32_t)((float)key_actor_auto_di_ms() * key_actor_auto_gap_ratio());
    return gap_ms > 0 ? gap_ms : 1;
}

static void key_actor_send_audio_tone_on(void)
{
    int freq = app_settings.key_tone_hz > 0 ? app_settings.key_tone_hz : KEY_DEFAULT_TONE_HZ;
    msg_t msg = msg_make_cmd_tone(MSG_SRC_KEY, freq, 0, key_actor_now_ms());
    msg.event = MSG_EVT_CMD_AUDIO_TONE_ON;
    (void)msg_send_cmd(&msg, 0);
}

static void key_actor_send_audio_tone_off(void)
{
    (void)msg_send_cmd_value(MSG_SRC_KEY, MSG_EVT_CMD_AUDIO_TONE_OFF, 0, 0);
}

static void key_actor_publish_raw_symbol(const char *symbol, uint32_t now_ms)
{
    if(symbol == NULL || symbol[0] == '\0') {
        return;
    }

    msg_t msg = msg_make(MSG_SRC_KEY, MSG_TYPE_INPUT, MSG_EVT_INPUT_CW_RAW_SYMBOL, now_ms);
    (void)snprintf(msg.data.text, sizeof(msg.data.text), "%s", symbol);
    (void)msg_send_input(&msg, 0);
    s_actor.last_symbol_at_ms = now_ms;
    s_actor.group_gap_pending = true;
}

static void key_actor_maybe_append_group_gap(uint32_t now_ms)
{
    if(!s_actor.group_gap_pending || s_actor.key_down) {
        return;
    }

    uint32_t base_ms = (app_settings.key_mode == KEY_MODE_AUTO) ? key_actor_auto_di_ms() : key_actor_manual_di_ms();
    uint32_t threshold_ms = (uint32_t)((float)base_ms * KEY_GROUP_GAP_RATIO);
    if(threshold_ms == 0) {
        threshold_ms = 1;
    }

    if((uint32_t)(now_ms - s_actor.last_symbol_at_ms) > threshold_ms) {
        key_actor_publish_raw_symbol(" ", now_ms);
        s_actor.group_gap_pending = false;
    }
}

static void key_actor_publish_physical(key_input_t input, bool pressed, uint32_t now_ms)
{
    msg_event_t event = MSG_EVT_INPUT_KEY_A_DOWN;
    if(input == KEY_INPUT_A) {
        event = pressed ? MSG_EVT_INPUT_KEY_A_DOWN : MSG_EVT_INPUT_KEY_A_UP;
    } else {
        event = pressed ? MSG_EVT_INPUT_KEY_B_DOWN : MSG_EVT_INPUT_KEY_B_UP;
    }

    (void)msg_send_input_value(MSG_SRC_KEY, event, pressed ? 1 : 0, 0);
    (void)now_ms;
}

static key_auto_symbol_t key_actor_auto_symbol_for_input(key_input_t input)
{
    bool input_is_di = (input == KEY_INPUT_A);
    if(app_settings.key_swap_ab) {
        input_is_di = !input_is_di;
    }

    return input_is_di ? KEY_AUTO_SYMBOL_DI : KEY_AUTO_SYMBOL_DA;
}

static bool key_actor_auto_has_request(void)
{
    return s_actor.auto_di_down || s_actor.auto_da_down;
}

static key_auto_symbol_t key_actor_auto_next_symbol(void)
{
    if(s_actor.auto_di_down && s_actor.auto_da_down) {
        return s_actor.auto_preferred_symbol;
    }
    if(s_actor.auto_di_down) {
        return KEY_AUTO_SYMBOL_DI;
    }
    return KEY_AUTO_SYMBOL_DA;
}

static void key_actor_auto_advance_after_symbol(key_auto_symbol_t symbol)
{
    if(s_actor.auto_di_down && s_actor.auto_da_down) {
        s_actor.auto_preferred_symbol =
            symbol == KEY_AUTO_SYMBOL_DI ? KEY_AUTO_SYMBOL_DA : KEY_AUTO_SYMBOL_DI;
    } else if(s_actor.auto_di_down) {
        s_actor.auto_preferred_symbol = KEY_AUTO_SYMBOL_DI;
    } else if(s_actor.auto_da_down) {
        s_actor.auto_preferred_symbol = KEY_AUTO_SYMBOL_DA;
    }
}

static void key_actor_auto_update_key_down(void)
{
    s_actor.key_down = key_actor_auto_has_request() || s_actor.auto_tone_on;
}

static void key_actor_auto_start_symbol(key_auto_symbol_t symbol, uint32_t now_ms)
{
    s_actor.auto_current_symbol = symbol;
    s_actor.auto_tone_on = true;
    s_actor.auto_gap_active = false;
    s_actor.auto_deadline_ms = now_ms + key_actor_auto_symbol_duration_ms(symbol);
    s_actor.key_down = true;
    key_actor_send_audio_tone_on();
}

static void key_actor_auto_finish_symbol(uint32_t now_ms)
{
    key_actor_send_audio_tone_off();
    s_actor.auto_tone_on = false;
    key_actor_publish_raw_symbol(
        s_actor.auto_current_symbol == KEY_AUTO_SYMBOL_DI ? "·" : "-",
        now_ms
    );
    key_actor_auto_advance_after_symbol(s_actor.auto_current_symbol);
    s_actor.auto_gap_active = true;
    s_actor.auto_deadline_ms = now_ms + key_actor_auto_gap_ms();
    key_actor_auto_update_key_down();
}

static void key_actor_auto_tick(uint32_t now_ms)
{
    if(s_actor.auto_tone_on) {
        if((int32_t)(now_ms - s_actor.auto_deadline_ms) >= 0) {
            key_actor_auto_finish_symbol(now_ms);
        }
        return;
    }

    if(s_actor.auto_gap_active) {
        if((int32_t)(now_ms - s_actor.auto_deadline_ms) < 0) {
            return;
        }
        s_actor.auto_gap_active = false;
    }

    if(key_actor_auto_has_request()) {
        key_actor_auto_start_symbol(key_actor_auto_next_symbol(), now_ms);
    } else {
        key_actor_auto_update_key_down();
    }
}

static void key_actor_set_auto_down(key_input_t input, bool pressed)
{
    key_auto_symbol_t symbol = key_actor_auto_symbol_for_input(input);
    if(symbol == KEY_AUTO_SYMBOL_DI) {
        s_actor.auto_di_down = pressed;
    } else {
        s_actor.auto_da_down = pressed;
    }

    if(pressed) {
        s_actor.auto_preferred_symbol = symbol;
    }
    key_actor_auto_update_key_down();
    key_actor_auto_tick(key_actor_now_ms());
}

static void key_actor_refresh_input_handler(void)
{
    key_actor_send_audio_tone_off();
    s_actor.auto_di_down = false;
    s_actor.auto_da_down = false;
    s_actor.auto_tone_on = false;
    s_actor.auto_gap_active = false;
    s_actor.auto_current_symbol = KEY_AUTO_SYMBOL_DI;
    s_actor.auto_preferred_symbol = KEY_AUTO_SYMBOL_DI;
    s_actor.auto_deadline_ms = 0;
    s_actor.key_down = false;

    s_key_input_handler = app_settings.key_mode == KEY_MODE_AUTO ?
        key_actor_handle_auto_input_event :
        key_actor_handle_manual_input_event;
}

static void key_actor_handle_auto_input_event(const key_input_event_t *event)
{
    if(event == NULL) {
        return;
    }

    key_actor_set_auto_down(event->input, event->pressed);
}

static void key_actor_update_manual_adaptive(uint32_t duration_ms)
{
    if(!app_settings.key_manual_adaptive_enable || duration_ms == 0) {
        return;
    }

    uint32_t current = key_actor_manual_di_ms();
    s_actor.manual_di_estimate_ms = (uint32_t)((float)current * 0.8f + (float)duration_ms * 0.2f);
    if(s_actor.manual_di_estimate_ms == 0) {
        s_actor.manual_di_estimate_ms = duration_ms;
    }
}

static void key_actor_handle_manual_input_event(const key_input_event_t *event)
{
    if(event == NULL || event->input != KEY_INPUT_A) {
        return;
    }

    if(event->pressed) {
        s_actor.key_down = true;
        s_actor.key_down_at_ms = event->timestamp_ms;
        key_actor_send_audio_tone_on();
        return;
    }

    if(!s_actor.key_down) {
        key_actor_send_audio_tone_off();
        return;
    }

    key_actor_send_audio_tone_off();
    s_actor.key_down = false;

    uint32_t duration_ms = event->timestamp_ms - s_actor.key_down_at_ms;
    uint32_t now_ms = key_actor_now_ms();
    uint32_t manual_di_ms = key_actor_manual_di_ms();
    if(duration_ms <= manual_di_ms) {
        key_actor_publish_raw_symbol("·", now_ms);
        key_actor_update_manual_adaptive(duration_ms);
    } else {
        key_actor_publish_raw_symbol("-", now_ms);
    }
}

static void key_actor_handle_input_event(const key_input_event_t *event)
{
    if(event == NULL || !event->changed) {
        return;
    }

    // LOG("key input %c %s",
    //     event->input == KEY_INPUT_A ? 'A' : 'B',
    //     event->pressed ? "down" : "up");
    key_actor_publish_physical(event->input, event->pressed, event->timestamp_ms);

    if(!app_settings.key_enable) {
        return;
    }

    s_key_input_handler(event);
}

static void key_actor_handle_msg(const msg_t *msg)
{
    if(msg == NULL) {
        return;
    }

    if(msg->type == MSG_TYPE_INPUT) {
        switch(msg->event) {
            case MSG_EVT_INPUT_KEY_DI:
                key_actor_publish_raw_symbol("·", key_actor_now_ms());
                break;
            case MSG_EVT_INPUT_KEY_DA:
                key_actor_publish_raw_symbol("-", key_actor_now_ms());
                break;
            default:
                break;
        }
        return;
    }

    if(msg->type == MSG_TYPE_CMD) {
        switch(msg->event) {
            case MSG_EVT_CMD_KEY_REFRESH_MODE:
                key_actor_refresh_input_handler();
                break;
            default:
                break;
        }
    }
}

static esp_err_t key_actor_init_key_driver(void)
{
    key_config_t cfg = {
        .gpio_a = PIN_KEY_A,
        .gpio_b = PIN_KEY_B,
        .active_level = app_settings.key_active_level,
        .debounce_ms = app_settings.key_debounce_ms >= 0 ? (uint32_t)app_settings.key_debounce_ms : 0,
    };
    return key_init(&cfg);
}

static void key_actor_task(void *arg)
{
    (void)arg;

    key_input_event_t events[KEY_INPUT_COUNT];
    while(1) {
        msg_t msg;
        if(xQueueReceive(s_actor.queue, &msg, pdMS_TO_TICKS(KEY_ACTOR_POLL_MS)) == pdTRUE) {
            key_actor_handle_msg(&msg);
            while(xQueueReceive(s_actor.queue, &msg, 0) == pdTRUE) {
                key_actor_handle_msg(&msg);
            }
        }

        size_t count = 0;
        if(key_poll(events, KEY_INPUT_COUNT, &count) == ESP_OK) {
            for(size_t i = 0; i < count; i++) {
                key_actor_handle_input_event(&events[i]);
            }
        }

        if(app_settings.key_mode == KEY_MODE_AUTO) {
            key_actor_auto_tick(key_actor_now_ms());
        }
        key_actor_maybe_append_group_gap(key_actor_now_ms());
    }
}

esp_err_t key_actor_init(void)
{
    if(s_actor.initialized) {
        return ESP_OK;
    }

    esp_err_t err = key_actor_init_key_driver();
    if(err != ESP_OK) {
        return err;
    }

    if(!app_settings.key_enable) {
        (void)app_settings_update(&(app_settings_update_t) {
            .id = APP_SETTING_ID_KEY_ENABLE,
            .value.b = true,
        });
    }

    err = msg_actor_queue_create_with_len(KEY_ACTOR_QUEUE_LEN, &s_actor.queue);
    if(err != ESP_OK) {
        (void)key_deinit();
        return err;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_KEY_INPUT,
        MSG_TOPIC_KEY_CMD,
    };
    err = msg_sub(s_actor.queue, topics, sizeof(topics) / sizeof(topics[0]), &s_actor.sub_handle);
    if(err != ESP_OK) {
        vQueueDelete(s_actor.queue);
        s_actor.queue = NULL;
        (void)key_deinit();
        return err;
    }

    s_actor.manual_di_estimate_ms = app_settings.key_manual_di_ms > 0 ?
        (uint32_t)app_settings.key_manual_di_ms : KEY_DEFAULT_MANUAL_DI_MS;
    key_actor_refresh_input_handler();

    BaseType_t ok = xTaskCreatePinnedToCore(
        key_actor_task,
        "key_actor",
        KEY_ACTOR_TASK_STACK,
        NULL,
        TASK_PRIO_KEY,
        &s_actor.task,
        TASK_CORE_KEY
    );
    if(ok != pdPASS) {
        (void)msg_unsub(s_actor.sub_handle, NULL, 0);
        vQueueDelete(s_actor.queue);
        s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
        s_actor.queue = NULL;
        (void)key_deinit();
        return ESP_FAIL;
    }

    s_actor.initialized = true;
    (void)msg_send_sys_value(MSG_SRC_KEY, MSG_EVT_SYS_INIT_DONE_KEY, 1, 0);
    return ESP_OK;
}

esp_err_t key_actor_deinit(void)
{
    if(!s_actor.initialized) {
        return ESP_OK;
    }

    if(s_actor.task != NULL) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }
    if(s_actor.sub_handle != MSG_SUB_HANDLE_INVALID) {
        (void)msg_unsub(s_actor.sub_handle, NULL, 0);
        s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
    }
    if(s_actor.queue != NULL) {
        vQueueDelete(s_actor.queue);
        s_actor.queue = NULL;
    }

    (void)key_deinit();
    memset(&s_actor, 0, sizeof(s_actor));
    s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
    return ESP_OK;
}
