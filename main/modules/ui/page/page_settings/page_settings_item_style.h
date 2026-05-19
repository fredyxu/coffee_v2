#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void page_settings_item_style_init(void);

void page_settings_item_apply_style_page_title_body(lv_obj_t *obj);
void page_settings_item_apply_style_page_title_label(lv_obj_t *obj);
void page_settings_item_apply_style_page_item_body(lv_obj_t *obj);
void page_settings_item_apply_style_page_item_in_body(lv_obj_t *obj);

void page_settings_item_apply_style_page_item_text(lv_obj_t *obj_body);
void page_settings_item_apply_style_page_item_bool(
	lv_obj_t *obj_body, 
	lv_obj_t *obj_title_body,
	lv_obj_t *obj_title_label, 
	lv_obj_t *obj_switch
);
void page_settings_item_apply_style_page_item_list(
	lv_obj_t *obj_body,
	lv_obj_t *obj_title_label
);

void page_settings_item_apply_style_page_item_int(
	lv_obj_t *obj_body,
	lv_obj_t *obj_title_body,
	lv_obj_t *obj_title_label,
	lv_obj_t *obj_slider_body,
	lv_obj_t *obj_slider
);

#ifdef __cplusplus
}
#endif