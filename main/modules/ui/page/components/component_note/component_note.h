#pragma once

#include "lvgl.h"

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

ui_note_ctx_t ui_note_ctx_default(void);


#ifdef __cplusplus
}
#endif