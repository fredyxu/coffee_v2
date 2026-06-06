#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *page_settings_renderer_create_list_container(lv_obj_t *parent);

lv_obj_t *page_settings_renderer_insert_list_row(lv_obj_t *parent,
												 const settings_sub_item_t *item,
												 const settings_value_list_t *list_item,
												 const char *title,
												 const char *value_str,
												 bool disabled,
												 bool selected,
												 size_t focus_index);

void page_settings_renderer_insert_static_list(lv_obj_t *parent, const settings_sub_item_t *item);
void page_settings_renderer_insert_action(lv_obj_t *parent, const settings_sub_item_t *item);
void page_settings_renderer_insert_text(lv_obj_t *parent, const settings_sub_item_t *item);
void page_settings_renderer_insert_bool(lv_obj_t *parent, const settings_sub_item_t *item);
void page_settings_renderer_insert_int(lv_obj_t *parent,
									   const settings_sub_item_t *item,
									   lv_event_cb_t slider_event_cb);

#ifdef __cplusplus
}
#endif
