#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_style_t style_page_body;

void ui_style_init(void);
void ui_style_init_row(lv_style_t *s);
void ui_style_init_column(lv_style_t *s);

void ui_style_insert_line_1(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif