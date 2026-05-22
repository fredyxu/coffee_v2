#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	OP_MENU = 0,
	OP_SELECTED,
} op_status_type_t;

typedef struct {
	settings_sub_item_id_t sub_item_id;
	settings_value_type_t value_type;
	lv_obj_t *obj;
	lv_obj_t *control;
	lv_obj_t *value_label;
	int value_int;
	char value_str[64];
	bool value_bool;
	const settings_sub_item_t *item;
	void (*on_enter)(const settings_sub_item_t *item);
	void (*on_change)(const settings_sub_item_t *item, int step);
	bool disabled;
} page_settings_item_focus_item_t;

typedef void (*page_settings_focus_activate_cb_t)(page_settings_item_focus_item_t *focus, void *user_data);

void page_settings_focus_reset(void);
void page_settings_focus_set_activate_cb(page_settings_focus_activate_cb_t cb, void *user_data);

size_t page_settings_focus_count(void);
int page_settings_focus_index(void);
op_status_type_t page_settings_focus_status(void);
void page_settings_focus_set_status(op_status_type_t status);

page_settings_item_focus_item_t *page_settings_focus_current(void);
page_settings_item_focus_item_t *page_settings_focus_get_by_index(int index);

int page_settings_focus_find_index_by_control(lv_obj_t *control);
int page_settings_focus_find_index_by_sub_item(settings_sub_item_id_t sub_item_id);

bool page_settings_focus_add(const settings_sub_item_t *item,
							 lv_obj_t *row,
							 lv_obj_t *control,
							 lv_obj_t *value_label,
							 bool disabled);
bool page_settings_focus_add_at(const settings_sub_item_t *item,
								lv_obj_t *row,
								lv_obj_t *control,
								lv_obj_t *value_label,
								bool disabled,
								size_t index);
void page_settings_focus_remove_sub_item(settings_sub_item_id_t sub_item_id);
void page_settings_focus_set_index(int index);
void page_settings_focus_move(int step);

#ifdef __cplusplus
}
#endif
