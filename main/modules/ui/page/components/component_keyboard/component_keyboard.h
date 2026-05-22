#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_KEYBOARD_EVT_CHANGED = 0,
    UI_KEYBOARD_EVT_SUBMIT,
    UI_KEYBOARD_EVT_CANCEL,
} ui_keyboard_event_t;

typedef void (*ui_keyboard_event_cb_t)(ui_keyboard_event_t event,
                                       const char *text,
                                       void *user_data);

typedef struct {
    lv_obj_t *parent;
    const char *title;
    const char *placeholder;
    char *buffer;
    size_t buffer_size;
    bool password_mode;
    ui_keyboard_event_cb_t on_event;
    void *user_data;
} ui_keyboard_config_t;

lv_obj_t *ui_keyboard_modal_create(const ui_keyboard_config_t *cfg);

void ui_keyboard_modal_close(lv_obj_t *modal);

const char *ui_keyboard_get_text(lv_obj_t *modal);

void ui_keyboard_set_text(lv_obj_t *modal, const char *text);

void ui_keyboard_handle_input(lv_obj_t *modal, const msg_t *msg);

#ifdef __cplusplus
}
#endif
