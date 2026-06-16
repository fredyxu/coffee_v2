#include "modules/ui/page/page_settings_item/internal/page_settings_focus.h"

#include <stdint.h>
#include <stdio.h>

#include "core/utils/log.h"

#define PAGE_SETTINGS_FOCUS_MAX 32

static void focus_format_int_value(const settings_sub_item_t *item, int value, char *buffer, size_t buffer_size)
{
	if(buffer == NULL || buffer_size == 0) {
		return;
	}

	if(item != NULL && item->format_value != NULL) {
		int32_t temp_value = value;
		settings_sub_item_t temp_item = *item;
		temp_item.value = &temp_value;
		item->format_value(&temp_item, buffer, buffer_size);
		return;
	}

	(void)snprintf(buffer, buffer_size, "%d", value);
}

static page_settings_item_focus_item_t s_focus[PAGE_SETTINGS_FOCUS_MAX];
static size_t s_focus_count;
static int s_focus_index = -1;
static op_status_type_t s_focus_status = OP_MENU;
static page_settings_focus_activate_cb_t s_activate_cb;
static void *s_activate_user_data;

static bool focus_item_can_focus(settings_value_type_t value_type)
{
	switch(value_type) {
		case SETTINGS_VALUE_TYPE_TEXT:
		case SETTINGS_VALUE_TYPE_PASSWORD:
		case SETTINGS_VALUE_TYPE_INPUT:
		case SETTINGS_VALUE_TYPE_BOOL:
		case SETTINGS_VALUE_TYPE_INT:
		case SETTINGS_VALUE_TYPE_LIST:
		case SETTINGS_VALUE_TYPE_ACTION:
			return true;

		default:
			return false;
	}
}

static int focus_find_index_by_obj(lv_obj_t *obj)
{
	if(obj == NULL) {
		return -1;
	}

	for(size_t i = 0; i < s_focus_count; i++) {
		if(s_focus[i].obj == obj) {
			return (int)i;
		}
	}

	return -1;
}

static void focus_disable_lvgl_click_focus(lv_obj_t *obj)
{
	if(obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
}

static void focus_touch_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	if(code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED) {
		return;
	}

	int index = focus_find_index_by_obj(lv_event_get_current_target(e));
	if(index < 0) {
		return;
	}

	page_settings_focus_set_index(index);

	if(code == LV_EVENT_PRESSED) {
		return;
	}

	page_settings_item_focus_item_t *focus = page_settings_focus_current();
	if(focus != NULL && s_activate_cb != NULL) {
		s_activate_cb(focus, s_activate_user_data);
		lv_indev_t *indev = lv_indev_active();
		if(indev != NULL) {
			lv_indev_wait_release(indev);
		}
	}
}

void page_settings_focus_reset(void)
{
	s_focus_count = 0;
	s_focus_index = -1;
	s_focus_status = OP_MENU;
	s_activate_cb = NULL;
	s_activate_user_data = NULL;
}

void page_settings_focus_set_activate_cb(page_settings_focus_activate_cb_t cb, void *user_data)
{
	s_activate_cb = cb;
	s_activate_user_data = user_data;
}

size_t page_settings_focus_count(void)
{
	return s_focus_count;
}

int page_settings_focus_index(void)
{
	return s_focus_index;
}

op_status_type_t page_settings_focus_status(void)
{
	return s_focus_status;
}

void page_settings_focus_set_status(op_status_type_t status)
{
	s_focus_status = status;
}

page_settings_item_focus_item_t *page_settings_focus_current(void)
{
	if(s_focus_count == 0 || s_focus_index < 0 || s_focus_index >= (int)s_focus_count) {
		return NULL;
	}

	if(s_focus[s_focus_index].disabled) {
		return NULL;
	}

	return &s_focus[s_focus_index];
}

page_settings_item_focus_item_t *page_settings_focus_get_by_index(int index)
{
	if(index < 0 || index >= (int)s_focus_count) {
		return NULL;
	}

	return &s_focus[index];
}

int page_settings_focus_find_index_by_control(lv_obj_t *control)
{
	if(control == NULL) {
		return -1;
	}

	for(size_t i = 0; i < s_focus_count; i++) {
		if(s_focus[i].control == control) {
			return (int)i;
		}
	}

	return -1;
}

int page_settings_focus_find_index_by_sub_item(settings_sub_item_id_t sub_item_id)
{
	for(size_t i = 0; i < s_focus_count; i++) {
		if(s_focus[i].sub_item_id == sub_item_id) {
			return (int)i;
		}
	}

	return -1;
}

static bool focus_add_at(const settings_sub_item_t *item,
						 const settings_value_list_t *list_item,
						 lv_obj_t *row,
						 lv_obj_t *control,
						 lv_obj_t *value_label,
						 const char *value_str,
						 bool disabled,
						 size_t index)
{
	if(item == NULL || row == NULL) {
		return false;
	}

	if(disabled) {
		return false;
	}

	if(!focus_item_can_focus(item->value_type)) {
		return false;
	}

	if(s_focus_count >= PAGE_SETTINGS_FOCUS_MAX) {
		LOG("settings focus list full");
		return false;
	}

	if(index > s_focus_count) {
		index = s_focus_count;
	}

	page_settings_item_focus_item_t focus_item = {
		.sub_item_id = item->id,
		.value_type = item->value_type,
		.obj = row,
		.control = control,
		.value_label = value_label,
		.item = item,
		.list_item = list_item,
		.disabled = disabled,
	};
	if(value_str != NULL) {
		(void)snprintf(focus_item.value_str, sizeof(focus_item.value_str), "%s", value_str);
	}

	if(item->value != NULL) {
		switch(item->value_type) {
			case SETTINGS_VALUE_TYPE_BOOL:
				focus_item.value_bool = *(bool *)item->value;
				break;

			case SETTINGS_VALUE_TYPE_INT:
				focus_item.value_int = *(int *)item->value;
				break;

			case SETTINGS_VALUE_TYPE_TEXT:
			case SETTINGS_VALUE_TYPE_PASSWORD:
			case SETTINGS_VALUE_TYPE_INPUT:
				(void)snprintf(focus_item.value_str, sizeof(focus_item.value_str), "%s", (const char *)item->value);
				break;

			default:
				break;
		}
	}

	for(size_t i = s_focus_count; i > index; i--) {
		s_focus[i] = s_focus[i - 1];
	}

	s_focus[index] = focus_item;

	lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
	focus_disable_lvgl_click_focus(row);
	lv_obj_add_event_cb(row, focus_touch_event_cb, LV_EVENT_PRESSED, NULL);
	lv_obj_add_event_cb(row, focus_touch_event_cb, LV_EVENT_CLICKED, NULL);

	s_focus_count++;

	if(s_focus_index >= (int)index) {
		s_focus_index++;
	}

	if(s_focus_index < 0) {
		page_settings_focus_set_index((int)index);
	}

	return true;
}

bool page_settings_focus_add(const settings_sub_item_t *item,
							 lv_obj_t *row,
							 lv_obj_t *control,
							 lv_obj_t *value_label,
							 bool disabled)
{
	return focus_add_at(item, NULL, row, control, value_label, NULL, disabled, s_focus_count);
}

bool page_settings_focus_add_at(const settings_sub_item_t *item,
								lv_obj_t *row,
								lv_obj_t *control,
								lv_obj_t *value_label,
								const char *value_str,
								bool disabled,
								size_t index)
{
	return focus_add_at(item, NULL, row, control, value_label, value_str, disabled, index);
}

bool page_settings_focus_add_list_at(const settings_sub_item_t *item,
									 const settings_value_list_t *list_item,
									 lv_obj_t *row,
									 lv_obj_t *value_label,
									 bool disabled,
									 size_t index)
{
	const char *value_str = list_item != NULL ? list_item->value_str : NULL;
	return focus_add_at(item, list_item, row, NULL, value_label, value_str, disabled, index);
}

void page_settings_focus_remove_sub_item(settings_sub_item_id_t sub_item_id)
{
	size_t write_index = 0;
	bool removed_current = false;
	size_t removed_before_current = 0;

	for(size_t read_index = 0; read_index < s_focus_count; read_index++) {
		if(s_focus[read_index].sub_item_id == sub_item_id) {
			if((int)read_index == s_focus_index) {
				removed_current = true;
			} else if(s_focus_index >= 0 && read_index < (size_t)s_focus_index) {
				removed_before_current++;
			}
			continue;
		}

		if(write_index != read_index) {
			s_focus[write_index] = s_focus[read_index];
		}
		write_index++;
	}

	s_focus_count = write_index;

	if(s_focus_count == 0) {
		s_focus_index = -1;
		s_focus_status = OP_MENU;
		return;
	}

	if(!removed_current && removed_before_current > 0) {
		s_focus_index -= (int)removed_before_current;
	}

	if(removed_current || s_focus_index < 0 || s_focus_index >= (int)s_focus_count) {
		s_focus_index = -1;
		page_settings_focus_set_index(0);
	}
}

void page_settings_focus_set_index(int index)
{
	if(index < 0 || index >= (int)s_focus_count) {
		return;
	}

	for(size_t i = 0; i < s_focus_count; i++) {
		if(s_focus[i].obj != NULL) {
			lv_obj_remove_state(s_focus[i].obj, LV_STATE_FOCUSED);
		}
	}

	if(s_focus_index >= 0 && s_focus_index < (int)s_focus_count) {
		lv_obj_remove_state(s_focus[s_focus_index].obj, LV_STATE_CHECKED);
	}

	s_focus_status = OP_MENU;
	s_focus_index = index;
	lv_obj_add_state(s_focus[s_focus_index].obj, LV_STATE_FOCUSED);
	lv_obj_scroll_to_view_recursive(s_focus[s_focus_index].obj, LV_ANIM_OFF);
}

void page_settings_focus_move(int step)
{
	if(s_focus_count == 0) {
		return;
	}

	int new_index = s_focus_index + step;

	if(new_index < 0) {
		new_index = (int)s_focus_count - 1;
	} else if(new_index >= (int)s_focus_count) {
		new_index = 0;
	}

	page_settings_focus_set_index(new_index);
}

void page_settings_focus_refresh_values(void)
{
	for(size_t i = 0; i < s_focus_count; i++) {
		page_settings_item_focus_item_t *focus = &s_focus[i];
		if(focus->item == NULL || focus->item->value == NULL || focus->value_label == NULL) {
			continue;
		}

		switch(focus->value_type) {
			case SETTINGS_VALUE_TYPE_TEXT:
			case SETTINGS_VALUE_TYPE_PASSWORD:
			case SETTINGS_VALUE_TYPE_INPUT:
				lv_label_set_text(focus->value_label, (const char *)focus->item->value);
				(void)snprintf(focus->value_str, sizeof(focus->value_str), "%s", (const char *)focus->item->value);
				break;

			case SETTINGS_VALUE_TYPE_INT:
				focus->value_int = *(int32_t *)focus->item->value;
				char text[16];
				focus_format_int_value(focus->item, focus->value_int, text, sizeof(text));
				lv_label_set_text(focus->value_label, text);
				if(focus->control != NULL) {
					lv_slider_set_value(focus->control, focus->value_int, LV_ANIM_OFF);
				}
				break;

			default:
				break;
		}
	}
}
