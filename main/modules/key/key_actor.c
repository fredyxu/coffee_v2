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

typedef struct {
    bool initialized;
    QueueHandle_t queue;
    msg_sub_handle_t sub_handle;
    TaskHandle_t task;
    bool key_down;
    uint32_t key_down_at_ms;
    uint32_t last_symbol_at_ms;
    bool group_gap_pending;
    uint32_t manual_di_estimate_ms;
    char code[KEY_CODE_MAX_LEN];
    size_t code_len;
} key_actor_ctx_t;

static key_actor_ctx_t s_actor = {
    .sub_handle = MSG_SUB_HANDLE_INVALID,
};

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

static bool key_actor_append_char(char ch)
{
    if(s_actor.code_len + 1 >= sizeof(s_actor.code)) {
        return false;
    }

    if(ch == ' ' && (s_actor.code_len == 0 || s_actor.code[s_actor.code_len - 1] == ' ')) {
        return false;
    }

    s_actor.code[s_actor.code_len++] = ch;
    s_actor.code[s_actor.code_len] = '\0';
    return true;
}

static void key_actor_publish_code_changed(void)
{
    msg_t msg = msg_make(MSG_SRC_KEY, MSG_TYPE_INPUT, MSG_EVT_INPUT_KEY_CODE_CHANGED, key_actor_now_ms());
    (void)snprintf(msg.data.text, sizeof(msg.data.text), "%s", s_actor.code);
    (void)msg_send_input(&msg, 0);
}

static void key_actor_append_symbol(char symbol, uint32_t now_ms)
{
    if(key_actor_append_char(symbol)) {
        s_actor.last_symbol_at_ms = now_ms;
        s_actor.group_gap_pending = true;
        key_actor_publish_code_changed();
    }
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
        if(key_actor_append_char(' ')) {
            key_actor_publish_code_changed();
        }
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

static msg_event_t key_actor_auto_event(key_input_t input, bool pressed)
{
    bool input_is_di = (input == KEY_INPUT_A);
    if(app_settings.key_swap_ab) {
        input_is_di = !input_is_di;
    }

    if(input_is_di) {
        return pressed ? MSG_EVT_CMD_CW_KEYER_DI_START : MSG_EVT_CMD_CW_KEYER_DI_STOP;
    }
    return pressed ? MSG_EVT_CMD_CW_KEYER_DA_START : MSG_EVT_CMD_CW_KEYER_DA_STOP;
}

static void key_actor_handle_auto_input(const key_input_event_t *event)
{
    if(event == NULL) {
        return;
    }

    msg_event_t cmd_event = key_actor_auto_event(event->input, event->pressed);
    (void)msg_send_cmd_value(MSG_SRC_KEY, cmd_event, event->pressed ? 1 : 0, 0);
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

static void key_actor_handle_manual_input(const key_input_event_t *event)
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
    uint32_t manual_di_ms = key_actor_manual_di_ms();
    if(duration_ms <= manual_di_ms) {
        key_actor_append_symbol('.', event->timestamp_ms);
        key_actor_update_manual_adaptive(duration_ms);
    } else {
        key_actor_append_symbol('-', event->timestamp_ms);
    }
}

static void key_actor_handle_input_event(const key_input_event_t *event)
{
    if(event == NULL || !event->changed) {
        return;
    }

    key_actor_publish_physical(event->input, event->pressed, event->timestamp_ms);

    if(!app_settings.key_enable) {
        return;
    }

    if(app_settings.key_mode == KEY_MODE_AUTO) {
        key_actor_handle_auto_input(event);
    } else {
        key_actor_handle_manual_input(event);
    }
}

static void key_actor_handle_msg(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_INPUT) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_INPUT_KEY_DI:
            key_actor_append_symbol('.', key_actor_now_ms());
            break;
        case MSG_EVT_INPUT_KEY_DA:
            key_actor_append_symbol('-', key_actor_now_ms());
            break;
        default:
            break;
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

    err = msg_actor_queue_create_with_len(KEY_ACTOR_QUEUE_LEN, &s_actor.queue);
    if(err != ESP_OK) {
        (void)key_deinit();
        return err;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_KEY_INPUT,
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
