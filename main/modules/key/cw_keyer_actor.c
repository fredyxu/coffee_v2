#include "modules/key/cw_keyer_actor.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app/app_settings.h"
#include "config/config_sys.h"
#include "config/config_sys_key.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"

typedef enum {
    CW_SYMBOL_DI = 0,
    CW_SYMBOL_DA,
} cw_symbol_t;

#define CW_KEYER_GROUP_MAX 24
#define CW_KEYER_DELETE_HISTORY_MAX 8

typedef struct {
    const char *code;
    const char *text;
} cw_decode_item_t;

typedef struct {
    char text[CW_KEYER_GROUP_MAX];
    bool had_trailing_space;
} cw_keyer_deleted_group_t;

typedef struct {
    bool initialized;
    QueueHandle_t queue;
    msg_sub_handle_t sub_handle;
    TaskHandle_t task;
    bool di_down;
    bool da_down;
    cw_symbol_t preferred;
    char *raw_text;
    size_t raw_len;
    size_t raw_cap;
    char *display_text;
    size_t display_len;
    size_t display_cap;
    char last_group[CW_KEYER_GROUP_MAX];
    size_t last_group_len;
    bool last_group_overflow;
    cw_keyer_deleted_group_t deleted_groups[CW_KEYER_DELETE_HISTORY_MAX];
    size_t deleted_group_count;
    bool auto_send_pending;
    TickType_t auto_send_deadline;
} cw_keyer_ctx_t;

static cw_keyer_ctx_t s_keyer = {
    .sub_handle = MSG_SUB_HANDLE_INVALID,
};

static const cw_decode_item_t s_cw_decode_table[] = {
    {"·-", "A"},
    {"-···", "B"},
    {"-·-·", "C"},
    {"-··", "D"},
    {"·", "E"},
    {"··-·", "F"},
    {"--·", "G"},
    {"····", "H"},
    {"··", "I"},
    {"·---", "J"},
    {"-·-", "K"},
    {"·-··", "L"},
    {"--", "M"},
    {"-·", "N"},
    {"---", "O"},
    {"·--·", "P"},
    {"--·-", "Q"},
    {"·-·", "R"},
    {"···", "S"},
    {"-", "T"},
    {"··-", "U"},
    {"···-", "V"},
    {"·--", "W"},
    {"-··-", "X"},
    {"-·--", "Y"},
    {"--··", "Z"},
    {"·----", "1"},
    {"··---", "2"},
    {"···--", "3"},
    {"····-", "4"},
    {"·····", "5"},
    {"-····", "6"},
    {"--···", "7"},
    {"---··", "8"},
    {"----·", "9"},
    {"-----", "0"},
};

static uint32_t cw_keyer_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static uint32_t cw_keyer_auto_di_ms(void)
{
    if(app_settings.key_auto_di_ms > 0) {
        return (uint32_t)app_settings.key_auto_di_ms;
    }

    uint32_t manual_di_ms = app_settings.key_manual_di_ms > 0 ?
        (uint32_t)app_settings.key_manual_di_ms : KEY_DEFAULT_MANUAL_DI_MS;
    uint32_t auto_di_ms = (uint32_t)((float)manual_di_ms * KEY_AUTO_DI_FALLBACK_RATIO);
    return auto_di_ms > 0 ? auto_di_ms : 1;
}

static float cw_keyer_auto_da_ratio(void)
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

static float cw_keyer_auto_gap_ratio(void)
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

static uint32_t cw_keyer_symbol_duration_ms(cw_symbol_t symbol)
{
    uint32_t di_ms = cw_keyer_auto_di_ms();
    if(symbol == CW_SYMBOL_DI) {
        return di_ms;
    }

    uint32_t da_ms = (uint32_t)((float)di_ms * cw_keyer_auto_da_ratio());
    return da_ms > 0 ? da_ms : 1;
}

static void cw_keyer_send_audio_on(void)
{
    int freq = app_settings.key_tone_hz > 0 ? app_settings.key_tone_hz : KEY_DEFAULT_TONE_HZ;
    msg_t msg = msg_make_cmd_tone(MSG_SRC_CW_KEYER, freq, 0, cw_keyer_now_ms());
    msg.event = MSG_EVT_CMD_AUDIO_TONE_ON;
    (void)msg_send_cmd(&msg, 0);
}

static void cw_keyer_send_audio_off(void)
{
    (void)msg_send_cmd_value(MSG_SRC_CW_KEYER, MSG_EVT_CMD_AUDIO_TONE_OFF, 0, 0);
}

static void cw_keyer_publish_symbol(cw_symbol_t symbol)
{
    msg_event_t event = (symbol == CW_SYMBOL_DI) ? MSG_EVT_INPUT_KEY_DI : MSG_EVT_INPUT_KEY_DA;
    (void)msg_send_input_value(MSG_SRC_CW_KEYER, event, 1, 0);
}

static void cw_keyer_publish_display_symbol(const char *symbol)
{
    if(symbol == NULL || symbol[0] == '\0') {
        return;
    }

    msg_t msg = msg_make(MSG_SRC_CW_KEYER, MSG_TYPE_INPUT, MSG_EVT_INPUT_CW_DISPLAY_SYMBOL, cw_keyer_now_ms());
    (void)snprintf(msg.data.text, sizeof(msg.data.text), "%s", symbol);
    (void)msg_send_input(&msg, 0);
}

static void cw_keyer_publish_content_state(void)
{
    (void)msg_send_input_value(
        MSG_SRC_CW_KEYER,
        MSG_EVT_INPUT_CW_CONTENT_STATE,
        s_keyer.raw_len > 0 ? 1 : 0,
        0
    );
}

static void cw_keyer_publish_text_changed(void)
{
    (void)msg_send_input_value(MSG_SRC_CW_KEYER, MSG_EVT_INPUT_CW_TEXT_CHANGED, 1, 0);
    cw_keyer_publish_content_state();
}

static void cw_keyer_publish_cleared(void)
{
    (void)msg_send_input_value(MSG_SRC_CW_KEYER, MSG_EVT_INPUT_CW_CLEARED, 1, 0);
    cw_keyer_publish_content_state();
}

static bool cw_keyer_tick_reached(TickType_t now, TickType_t target)
{
    return (int32_t)(now - target) >= 0;
}

static void cw_keyer_cancel_auto_send(void)
{
    s_keyer.auto_send_pending = false;
}

static void cw_keyer_clear_delete_history(void)
{
    s_keyer.deleted_group_count = 0;
}

static void cw_keyer_push_deleted_group(const char *text, size_t len, bool had_trailing_space)
{
    if(text == NULL || len == 0 || len >= CW_KEYER_GROUP_MAX) {
        return;
    }

    if(s_keyer.deleted_group_count == CW_KEYER_DELETE_HISTORY_MAX) {
        memmove(
            &s_keyer.deleted_groups[0],
            &s_keyer.deleted_groups[1],
            sizeof(s_keyer.deleted_groups[0]) * (CW_KEYER_DELETE_HISTORY_MAX - 1)
        );
        s_keyer.deleted_group_count--;
    }

    cw_keyer_deleted_group_t *group = &s_keyer.deleted_groups[s_keyer.deleted_group_count++];
    memcpy(group->text, text, len);
    group->text[len] = '\0';
    group->had_trailing_space = had_trailing_space;
}

static bool cw_keyer_pop_deleted_group(cw_keyer_deleted_group_t *out_group)
{
    if(out_group == NULL || s_keyer.deleted_group_count == 0) {
        return false;
    }

    *out_group = s_keyer.deleted_groups[--s_keyer.deleted_group_count];
    return true;
}

static void cw_keyer_schedule_auto_send(void)
{
    if(!app_settings.morse_auto_send_enable || s_keyer.raw_len == 0) {
        cw_keyer_cancel_auto_send();
        return;
    }

    int32_t delay_ms = app_settings.morse_auto_send_delay_ms;
    if(delay_ms < 500) {
        delay_ms = 500;
    }
    s_keyer.auto_send_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(delay_ms);
    s_keyer.auto_send_pending = true;
}

static void cw_keyer_request_send(void)
{
    if(s_keyer.raw_len == 0) {
        return;
    }

    (void)msg_send_cmd_value(MSG_SRC_CW_KEYER, MSG_EVT_CMD_WS_SEND_CW, 1, 0);
}

static bool cw_keyer_buffer_reserve(char **buffer, size_t *cap, size_t needed)
{
    if(buffer == NULL || cap == NULL) {
        return false;
    }
    if(needed <= *cap) {
        return true;
    }

    size_t next_cap = *cap > 0 ? *cap : 128;
    while(next_cap < needed) {
        next_cap *= 2;
    }

    char *next = (char *)realloc(*buffer, next_cap);
    if(next == NULL) {
        return false;
    }

    *buffer = next;
    *cap = next_cap;
    return true;
}

static bool cw_keyer_append_to_buffer(char **buffer,
                                      size_t *len,
                                      size_t *cap,
                                      const char *text)
{
    if(buffer == NULL || len == NULL || cap == NULL || text == NULL || text[0] == '\0') {
        return false;
    }

    size_t text_len = strlen(text);
    size_t needed = *len + text_len + 1;
    if(!cw_keyer_buffer_reserve(buffer, cap, needed)) {
        return false;
    }

    memcpy(*buffer + *len, text, text_len);
    *len += text_len;
    (*buffer)[*len] = '\0';
    return true;
}

static bool cw_keyer_should_accept_symbol(const char *symbol)
{
    if(symbol == NULL || symbol[0] == '\0') {
        return false;
    }

    if(strcmp(symbol, " ") == 0 &&
       (s_keyer.raw_len == 0 || s_keyer.raw_text[s_keyer.raw_len - 1] == ' ')) {
        return false;
    }

    return true;
}

static size_t cw_keyer_code_len(const char *text)
{
    size_t len = 0;
    const char *p = text;
    while(p != NULL && *p != '\0') {
        if(strncmp(p, "·", strlen("·")) == 0) {
            len++;
            p += strlen("·");
        } else if(*p == '-' || *p == ' ') {
            len++;
            p++;
        } else {
            p++;
        }
    }

    return len;
}

static bool cw_keyer_can_append_code(const char *text)
{
    if(text == NULL || text[0] == '\0') {
        return true;
    }

    return cw_keyer_code_len(s_keyer.raw_text) + cw_keyer_code_len(text) <= KEY_CODE_MAX_LEN;
}

static bool cw_keyer_raw_ends_with_space(void)
{
    return s_keyer.raw_len > 0 &&
           s_keyer.raw_text != NULL &&
           s_keyer.raw_text[s_keyer.raw_len - 1] == ' ';
}

static const char *cw_keyer_decode_group(void)
{
    if(s_keyer.last_group_len == 0 || s_keyer.last_group_overflow) {
        return NULL;
    }

    for(size_t i = 0; i < sizeof(s_cw_decode_table) / sizeof(s_cw_decode_table[0]); i++) {
        if(strcmp(s_keyer.last_group, s_cw_decode_table[i].code) == 0) {
            return s_cw_decode_table[i].text;
        }
    }

    return NULL;
}

static const char *cw_keyer_decode_code(const char *code)
{
    if(code == NULL || code[0] == '\0') {
        return NULL;
    }

    for(size_t i = 0; i < sizeof(s_cw_decode_table) / sizeof(s_cw_decode_table[0]); i++) {
        if(strcmp(code, s_cw_decode_table[i].code) == 0) {
            return s_cw_decode_table[i].text;
        }
    }

    return NULL;
}

static void cw_keyer_clear_last_group(void)
{
    s_keyer.last_group[0] = '\0';
    s_keyer.last_group_len = 0;
    s_keyer.last_group_overflow = false;
}

static void cw_keyer_append_last_group(const char *symbol)
{
    if(symbol == NULL || symbol[0] == '\0') {
        return;
    }

    size_t symbol_len = strlen(symbol);
    if(s_keyer.last_group_len + symbol_len >= sizeof(s_keyer.last_group)) {
        s_keyer.last_group_overflow = true;
        return;
    }

    memcpy(s_keyer.last_group + s_keyer.last_group_len, symbol, symbol_len);
    s_keyer.last_group_len += symbol_len;
    s_keyer.last_group[s_keyer.last_group_len] = '\0';
}

static void cw_keyer_publish_display_text(const char *text)
{
    if(text == NULL || text[0] == '\0') {
        return;
    }

    if(cw_keyer_append_to_buffer(
        &s_keyer.display_text,
        &s_keyer.display_len,
        &s_keyer.display_cap,
        text
    )) {
        cw_keyer_publish_display_symbol(text);
    }
}

static void cw_keyer_append_display_text(const char *text)
{
    (void)cw_keyer_append_to_buffer(
        &s_keyer.display_text,
        &s_keyer.display_len,
        &s_keyer.display_cap,
        text
    );
}

static void cw_keyer_render_group_end(bool publish)
{
    if(app_settings.cw_decode_display_enable && s_keyer.last_group_len > 0) {
        const char *decoded = cw_keyer_decode_group();
        const char *text = decoded != NULL ? decoded : "*";
        if(publish) {
            cw_keyer_publish_display_text(text);
        } else {
            cw_keyer_append_display_text(text);
        }
    }

    if(publish) {
        cw_keyer_publish_display_text(" ");
    } else {
        cw_keyer_append_display_text(" ");
    }
    cw_keyer_clear_last_group();
}

static void cw_keyer_publish_group_end(void)
{
    cw_keyer_render_group_end(true);
}

static void cw_keyer_reset_display_state(void)
{
    s_keyer.display_len = 0;
    if(s_keyer.display_text != NULL) {
        s_keyer.display_text[0] = '\0';
    }
    cw_keyer_clear_last_group();
}

static void cw_keyer_replay_display_symbol(const char *symbol)
{
    if(symbol == NULL || symbol[0] == '\0') {
        return;
    }

    if(strcmp(symbol, " ") == 0) {
        cw_keyer_render_group_end(false);
        return;
    }

    cw_keyer_append_last_group(symbol);
    (void)cw_keyer_append_to_buffer(
        &s_keyer.display_text,
        &s_keyer.display_len,
        &s_keyer.display_cap,
        symbol
    );
}

static void cw_keyer_rebuild_display_from_raw(void)
{
    cw_keyer_reset_display_state();

    const char *p = s_keyer.raw_text;
    while(p != NULL && *p != '\0') {
        if(strncmp(p, "·", strlen("·")) == 0) {
            cw_keyer_replay_display_symbol("·");
            p += strlen("·");
        } else if(*p == '-') {
            cw_keyer_replay_display_symbol("-");
            p++;
        } else if(*p == ' ') {
            cw_keyer_replay_display_symbol(" ");
            p++;
        } else {
            p++;
        }
    }
}

static void cw_keyer_clear_text(void)
{
    s_keyer.raw_len = 0;
    if(s_keyer.raw_text != NULL) {
        s_keyer.raw_text[0] = '\0';
    }
    s_keyer.display_len = 0;
    if(s_keyer.display_text != NULL) {
        s_keyer.display_text[0] = '\0';
    }
    cw_keyer_clear_last_group();
    cw_keyer_clear_delete_history();
    cw_keyer_cancel_auto_send();
    cw_keyer_publish_cleared();
}

static void cw_keyer_delete_last_group(void)
{
    if(s_keyer.raw_len == 0 || s_keyer.raw_text == NULL) {
        return;
    }

    size_t original_len = s_keyer.raw_len;
    size_t end = s_keyer.raw_len;
    while(end > 0 && s_keyer.raw_text[end - 1] == ' ') {
        end--;
    }

    if(end == 0) {
        cw_keyer_clear_text();
        return;
    }

    size_t start = end;
    while(start > 0 && s_keyer.raw_text[start - 1] != ' ') {
        start--;
    }

    cw_keyer_push_deleted_group(
        s_keyer.raw_text + start,
        end - start,
        original_len > end
    );

    s_keyer.raw_len = start;
    while(s_keyer.raw_len > 0 && s_keyer.raw_text[s_keyer.raw_len - 1] == ' ') {
        s_keyer.raw_len--;
    }
    if(s_keyer.raw_len > 0) {
        s_keyer.raw_text[s_keyer.raw_len++] = ' ';
    }
    s_keyer.raw_text[s_keyer.raw_len] = '\0';

    cw_keyer_cancel_auto_send();
    cw_keyer_rebuild_display_from_raw();
    cw_keyer_publish_text_changed();
}

static void cw_keyer_restore_last_group(void)
{
    if(s_keyer.deleted_group_count == 0) {
        return;
    }

    cw_keyer_deleted_group_t group = s_keyer.deleted_groups[s_keyer.deleted_group_count - 1];
    if(group.text[0] == '\0') {
        return;
    }

    size_t restore_len = cw_keyer_code_len(group.text);
    if(s_keyer.raw_len > 0 && !cw_keyer_raw_ends_with_space()) {
        restore_len++;
    }
    if(group.had_trailing_space) {
        restore_len++;
    }
    if(cw_keyer_code_len(s_keyer.raw_text) + restore_len > KEY_CODE_MAX_LEN) {
        return;
    }

    (void)cw_keyer_pop_deleted_group(&group);

    if(s_keyer.raw_len > 0 && !cw_keyer_raw_ends_with_space()) {
        if(!cw_keyer_append_to_buffer(&s_keyer.raw_text, &s_keyer.raw_len, &s_keyer.raw_cap, " ")) {
            return;
        }
    }

    if(!cw_keyer_append_to_buffer(&s_keyer.raw_text, &s_keyer.raw_len, &s_keyer.raw_cap, group.text)) {
        return;
    }

    if(group.had_trailing_space) {
        if(!cw_keyer_append_to_buffer(&s_keyer.raw_text, &s_keyer.raw_len, &s_keyer.raw_cap, " ")) {
            return;
        }
    }

    cw_keyer_cancel_auto_send();
    cw_keyer_rebuild_display_from_raw();
    cw_keyer_publish_text_changed();
}

static void cw_keyer_append_raw_symbol(const char *symbol)
{
    if(!cw_keyer_should_accept_symbol(symbol)) {
        return;
    }

    if(!cw_keyer_can_append_code(symbol)) {
        return;
    }

    if(!cw_keyer_append_to_buffer(
        &s_keyer.raw_text,
        &s_keyer.raw_len,
        &s_keyer.raw_cap,
        symbol
    )) {
        return;
    }

    if(strcmp(symbol, " ") == 0) {
        cw_keyer_publish_group_end();
        cw_keyer_schedule_auto_send();
        cw_keyer_publish_content_state();
        return;
    }

    cw_keyer_cancel_auto_send();
    cw_keyer_append_last_group(symbol);
    cw_keyer_publish_display_text(symbol);
    cw_keyer_publish_content_state();
}

static void cw_keyer_apply_msg(const msg_t *msg)
{
    if(msg == NULL) {
        return;
    }

    if(msg->type == MSG_TYPE_INPUT) {
        if(msg->event == MSG_EVT_INPUT_CW_RAW_SYMBOL) {
            cw_keyer_append_raw_symbol(msg->data.text);
        }
        return;
    }

    if(msg->type != MSG_TYPE_CMD) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_CMD_CW_KEYER_DI_START:
            s_keyer.di_down = true;
            s_keyer.preferred = CW_SYMBOL_DI;
            break;
        case MSG_EVT_CMD_CW_KEYER_DI_STOP:
            s_keyer.di_down = false;
            if(s_keyer.da_down) {
                s_keyer.preferred = CW_SYMBOL_DA;
            }
            break;
        case MSG_EVT_CMD_CW_KEYER_DA_START:
            s_keyer.da_down = true;
            s_keyer.preferred = CW_SYMBOL_DA;
            break;
        case MSG_EVT_CMD_CW_KEYER_DA_STOP:
            s_keyer.da_down = false;
            if(s_keyer.di_down) {
                s_keyer.preferred = CW_SYMBOL_DI;
            }
            break;
        case MSG_EVT_CMD_CW_SEND:
            cw_keyer_request_send();
            break;
        case MSG_EVT_CMD_CW_DELETE_LAST_GROUP:
            cw_keyer_delete_last_group();
            break;
        case MSG_EVT_CMD_CW_RESTORE_LAST_GROUP:
            cw_keyer_restore_last_group();
            break;
        case MSG_EVT_CMD_CW_CLEAR_DELETE_HISTORY:
            cw_keyer_clear_delete_history();
            break;
        case MSG_EVT_CMD_CW_CLEAR:
            cw_keyer_clear_text();
            break;
        default:
            break;
    }
}

static void cw_keyer_maybe_auto_send(void)
{
    if(!s_keyer.auto_send_pending) {
        return;
    }

    if(!cw_keyer_tick_reached(xTaskGetTickCount(), s_keyer.auto_send_deadline)) {
        return;
    }

    s_keyer.auto_send_pending = false;
    cw_keyer_request_send();
}

static void cw_keyer_drain_queue(void)
{
    msg_t msg;
    while(xQueueReceive(s_keyer.queue, &msg, 0) == pdTRUE) {
        cw_keyer_apply_msg(&msg);
    }
}

static bool cw_keyer_has_active_request(void)
{
    return s_keyer.di_down || s_keyer.da_down;
}

static cw_symbol_t cw_keyer_next_symbol(void)
{
    if(s_keyer.di_down && s_keyer.da_down) {
        return s_keyer.preferred;
    }
    if(s_keyer.di_down) {
        return CW_SYMBOL_DI;
    }
    return CW_SYMBOL_DA;
}

static void cw_keyer_advance_after_symbol(cw_symbol_t symbol)
{
    if(s_keyer.di_down && s_keyer.da_down) {
        s_keyer.preferred = symbol == CW_SYMBOL_DI ? CW_SYMBOL_DA : CW_SYMBOL_DI;
    } else if(s_keyer.di_down) {
        s_keyer.preferred = CW_SYMBOL_DI;
    } else if(s_keyer.da_down) {
        s_keyer.preferred = CW_SYMBOL_DA;
    }
}

static void cw_keyer_wait_ms(uint32_t duration_ms)
{
    uint32_t remaining = duration_ms;
    while(remaining > 0) {
        uint32_t step = remaining > 5 ? 5 : remaining;
        vTaskDelay(pdMS_TO_TICKS(step));
        cw_keyer_drain_queue();
        remaining -= step;
    }
}

static void cw_keyer_task(void *arg)
{
    (void)arg;

    while(1) {
        if(!cw_keyer_has_active_request()) {
            msg_t msg;
            TickType_t wait_ticks = s_keyer.auto_send_pending ? pdMS_TO_TICKS(50) : portMAX_DELAY;
            if(xQueueReceive(s_keyer.queue, &msg, wait_ticks) == pdTRUE) {
                cw_keyer_apply_msg(&msg);
            }
            cw_keyer_maybe_auto_send();
            continue;
        }

        cw_keyer_drain_queue();
        cw_keyer_maybe_auto_send();
        if(!cw_keyer_has_active_request()) {
            continue;
        }

        cw_symbol_t symbol = cw_keyer_next_symbol();
        uint32_t duration_ms = cw_keyer_symbol_duration_ms(symbol);
        uint32_t gap_ms = (uint32_t)((float)cw_keyer_auto_di_ms() * cw_keyer_auto_gap_ratio());
        if(gap_ms == 0) {
            gap_ms = 1;
        }

        cw_keyer_send_audio_on();
        cw_keyer_wait_ms(duration_ms);
        cw_keyer_send_audio_off();
        cw_keyer_publish_symbol(symbol);
        cw_keyer_advance_after_symbol(symbol);
        cw_keyer_wait_ms(gap_ms);
    }
}

esp_err_t cw_keyer_actor_init(void)
{
    if(s_keyer.initialized) {
        return ESP_OK;
    }

    esp_err_t err = msg_actor_queue_create_with_len(CW_KEYER_QUEUE_LEN, &s_keyer.queue);
    if(err != ESP_OK) {
        return err;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_CW_KEYER_CMD,
        MSG_TOPIC_CW_INPUT,
    };
    err = msg_sub(s_keyer.queue, topics, sizeof(topics) / sizeof(topics[0]), &s_keyer.sub_handle);
    if(err != ESP_OK) {
        vQueueDelete(s_keyer.queue);
        s_keyer.queue = NULL;
        return err;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        cw_keyer_task,
        "cw_keyer",
        CW_KEYER_TASK_STACK,
        NULL,
        TASK_PRIO_KEY,
        &s_keyer.task,
        TASK_CORE_KEY
    );
    if(ok != pdPASS) {
        (void)msg_unsub(s_keyer.sub_handle, NULL, 0);
        vQueueDelete(s_keyer.queue);
        s_keyer.sub_handle = MSG_SUB_HANDLE_INVALID;
        s_keyer.queue = NULL;
        return ESP_FAIL;
    }

    s_keyer.initialized = true;
    return ESP_OK;
}

esp_err_t cw_keyer_actor_deinit(void)
{
    if(!s_keyer.initialized) {
        return ESP_OK;
    }

    if(s_keyer.task != NULL) {
        vTaskDelete(s_keyer.task);
        s_keyer.task = NULL;
    }
    if(s_keyer.sub_handle != MSG_SUB_HANDLE_INVALID) {
        (void)msg_unsub(s_keyer.sub_handle, NULL, 0);
        s_keyer.sub_handle = MSG_SUB_HANDLE_INVALID;
    }
    if(s_keyer.queue != NULL) {
        vQueueDelete(s_keyer.queue);
        s_keyer.queue = NULL;
    }
    free(s_keyer.raw_text);
    free(s_keyer.display_text);

    memset(&s_keyer, 0, sizeof(s_keyer));
    s_keyer.sub_handle = MSG_SUB_HANDLE_INVALID;
    return ESP_OK;
}

const char *cw_keyer_actor_get_raw_text(void)
{
    return s_keyer.raw_text != NULL ? s_keyer.raw_text : "";
}

const char *cw_keyer_actor_get_display_text(void)
{
    return s_keyer.display_text != NULL ? s_keyer.display_text : "";
}

size_t cw_keyer_actor_render_display_text(const char *raw_text, char *out, size_t out_size)
{
    if(out == NULL || out_size == 0) {
        return 0;
    }

    out[0] = '\0';
    if(raw_text == NULL || raw_text[0] == '\0') {
        return 0;
    }

    if(!app_settings.cw_decode_display_enable) {
        (void)snprintf(out, out_size, "%s", raw_text);
        return strlen(out);
    }

    size_t out_len = 0;
    char group[CW_KEYER_GROUP_MAX] = {0};
    size_t group_len = 0;
    bool group_overflow = false;

    for(const char *p = raw_text; *p != '\0';) {
        const char *symbol = NULL;
        size_t symbol_len = 0;

        if(strncmp(p, "·", strlen("·")) == 0) {
            symbol = "·";
            symbol_len = strlen("·");
        } else if(*p == '-') {
            symbol = "-";
            symbol_len = 1;
        } else if(*p == ' ') {
            if(group_len > 0) {
                const char *decoded = group_overflow ? NULL : cw_keyer_decode_code(group);
                const char *text = decoded != NULL ? decoded : "*";
                size_t text_len = strlen(text);
                if(out_len + text_len < out_size) {
                    memcpy(out + out_len, text, text_len);
                    out_len += text_len;
                    out[out_len] = '\0';
                }
                group[0] = '\0';
                group_len = 0;
                group_overflow = false;
            }
            if(out_len + 1 < out_size) {
                out[out_len++] = ' ';
                out[out_len] = '\0';
            }
            p++;
            continue;
        } else {
            p++;
            continue;
        }

        if(out_len + symbol_len < out_size) {
            memcpy(out + out_len, symbol, symbol_len);
            out_len += symbol_len;
            out[out_len] = '\0';
        }

        if(group_len + symbol_len < sizeof(group)) {
            memcpy(group + group_len, symbol, symbol_len);
            group_len += symbol_len;
            group[group_len] = '\0';
        } else {
            group_overflow = true;
        }

        p += symbol_len;
    }

    if(group_len > 0) {
        const char *decoded = group_overflow ? NULL : cw_keyer_decode_code(group);
        const char *text = decoded != NULL ? decoded : "*";
        size_t text_len = strlen(text);
        if(out_len + text_len < out_size) {
            memcpy(out + out_len, text, text_len);
            out_len += text_len;
            out[out_len] = '\0';
        }
    }

    return out_len;
}
