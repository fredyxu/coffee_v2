#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	NOTE = 0,
	MSG,
	ASK,
	ERR,
} ui_note_type_t;

typedef void (*ui_note_cb_t)(void *user_data);

typedef struct {
    ui_note_type_t type;

    const char *title;
    const char *message;

    const char *confirm_text;
    const char *cancel_text;

    ui_note_cb_t on_confirm;
    ui_note_cb_t on_cancel;
    void *user_data;
} ui_note_ctx_t;


// void com_note_show(lv_obj_t *p);
void ui_note_show(ui_note_ctx_t *ctx);
void ui_note_hide(void);
bool ui_note_is_visible(void);
void ui_note_handle_input(const msg_t *msg);

ui_note_ctx_t ui_note_ctx_default(void);


#ifdef __cplusplus
}
#endif
