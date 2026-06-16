#include "modules/ui/page/page_settings_item/internal/page_settings_binding.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"
#include "modules/ui/ui.h"

static void binding_format_int_value(const settings_sub_item_t *item, int value, char *buffer, size_t buffer_size)
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

static void binding_preview_int_value(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->item == NULL || focus->item->on_preview_change == NULL) {
		return;
	}

	focus->item->on_preview_change(focus->item, focus->value_int);
}

static void binding_refresh_values_if_page_active(void)
{
	if(ui_get_current_page() != PAGE_SETTINGS_ITEM) {
		return;
	}

	page_settings_focus_refresh_values();
}

static int binding_read_int(const settings_sub_item_t *item)
{
	if(item == NULL || item->value == NULL) {
		return 0;
	}

	return (int)*(int32_t *)item->value;
}

static bool binding_read_bool(const settings_sub_item_t *item)
{
	if(item == NULL || item->value == NULL) {
		return false;
	}

	return *(bool *)item->value;
}

static esp_err_t binding_write_bool(const settings_sub_item_t *item, bool value)
{
	if(item == NULL || item->value == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	if(item->has_change_cmd_event) {
		return msg_send_cmd_value(MSG_SRC_LVGL, item->change_cmd_event, value ? 1 : 0, 0);
	}

	if(item->has_setting_id) {
		return app_settings_update(&(app_settings_update_t) {
			.id = item->setting_id,
			.value.b = value,
		});
	}

	*(bool *)item->value = value;
	return ESP_OK;
}

static esp_err_t binding_write_int(const settings_sub_item_t *item, int value)
{
	if(item == NULL || item->value == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	if(item->has_setting_id) {
		return app_settings_update(&(app_settings_update_t) {
			.id = item->setting_id,
			.value.i32 = value,
		});
	}

	*(int32_t *)item->value = value;
	return ESP_OK;
}

static void binding_adjust_int(page_settings_item_focus_item_t *focus, int step)
{
	if(focus == NULL || focus->item == NULL || focus->item->value == NULL) {
		return;
	}

	int delta = focus->item->step;
	if(delta <= 0) {
		delta = 1;
	}

	focus->value_int += step * delta;

	if(focus->value_int < focus->item->min_value) {
		focus->value_int = focus->item->min_value;
	} else if(focus->value_int > focus->item->max_value) {
		focus->value_int = focus->item->max_value;
	}

	if(focus->control != NULL) {
		lv_slider_set_value(focus->control, focus->value_int, LV_ANIM_OFF);
	}

	if(focus->value_label != NULL) {
		char text[16];
		binding_format_int_value(focus->item, focus->value_int, text, sizeof(text));
		lv_label_set_text(focus->value_label, text);
	}

	binding_preview_int_value(focus);
}

static void binding_toggle_bool(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->item == NULL || focus->item->value == NULL || focus->control == NULL) {
		return;
	}

	const bool current_value = binding_read_bool(focus->item);
	const bool new_value = !current_value;
	esp_err_t err = binding_write_bool(focus->item, new_value);
	if(err != ESP_OK) {
		LOG("settings bool update failed: id=%d err=%d", focus->item->id, err);
		return;
	}

	focus->value_bool = new_value;

	if(new_value) {
		lv_obj_add_state(focus->control, LV_STATE_CHECKED);
	} else {
		lv_obj_remove_state(focus->control, LV_STATE_CHECKED);
	}
}

static void binding_begin_edit(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->item == NULL) {
		return;
	}

	if(focus->value_type == SETTINGS_VALUE_TYPE_INT && focus->item->value != NULL) {
		focus->value_int = binding_read_int(focus->item);
	}

	page_settings_focus_set_status(OP_SELECTED);

	if(focus->obj != NULL) {
		lv_obj_add_state(focus->obj, LV_STATE_CHECKED);
	}
}

static void binding_apply_edit(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->item == NULL) {
		return;
	}

	if(focus->value_type == SETTINGS_VALUE_TYPE_INT && focus->item->value != NULL) {
		esp_err_t err = binding_write_int(focus->item, focus->value_int);
		if(err != ESP_OK) {
			LOG("settings int update failed: id=%d err=%d", focus->item->id, err);
		} else if(focus->item->on_change != NULL) {
			focus->item->on_change(focus->item);
		}
	}

	page_settings_focus_set_status(OP_MENU);

	if(focus->obj != NULL) {
		lv_obj_remove_state(focus->obj, LV_STATE_CHECKED);
	}
}

static void binding_sync_int_from_slider(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->control == NULL || focus->item == NULL || focus->item->value == NULL) {
		return;
	}

	focus->value_int = (int)lv_slider_get_value(focus->control);

	if(focus->value_label != NULL) {
		char text[16];
		binding_format_int_value(focus->item, focus->value_int, text, sizeof(text));
		lv_label_set_text(focus->value_label, text);
	}

	binding_preview_int_value(focus);
}

page_settings_binding_result_t page_settings_binding_rotate_current(int step)
{
	page_settings_item_focus_item_t *focus = page_settings_focus_current();
	if(focus == NULL || focus->disabled) {
		return PAGE_SETTINGS_BINDING_RESULT_NONE;
	}

	if(page_settings_focus_status() == OP_MENU) {
		page_settings_focus_move(step);
		return PAGE_SETTINGS_BINDING_RESULT_NONE;
	}

	switch(focus->value_type) {
		case SETTINGS_VALUE_TYPE_INT:
			binding_adjust_int(focus, step);
			return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;

		default:
			return PAGE_SETTINGS_BINDING_RESULT_UNHANDLED;
	}
}

page_settings_binding_result_t page_settings_binding_press(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL) {
		return PAGE_SETTINGS_BINDING_RESULT_NONE;
	}

	switch(focus->value_type) {
		case SETTINGS_VALUE_TYPE_TEXT:
			if(focus->item != NULL && focus->item->on_action != NULL) {
				focus->item->on_action(focus->item);
				binding_refresh_values_if_page_active();
				return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;
			}
			return PAGE_SETTINGS_BINDING_RESULT_UNHANDLED;

		case SETTINGS_VALUE_TYPE_BOOL:
			binding_toggle_bool(focus);
			return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;

		case SETTINGS_VALUE_TYPE_INT:
			if(page_settings_focus_status() == OP_MENU) {
				binding_begin_edit(focus);
				return PAGE_SETTINGS_BINDING_RESULT_EDIT_BEGIN;
			}
			binding_apply_edit(focus);
			return PAGE_SETTINGS_BINDING_RESULT_EDIT_APPLY;

		case SETTINGS_VALUE_TYPE_ACTION:
			if(focus->item != NULL && focus->item->has_cmd_event) {
				esp_err_t err = msg_send_cmd_value(MSG_SRC_LVGL, focus->item->cmd_event, focus->item->cmd_value, 0);
				if(err != ESP_OK) {
					LOG("send settings action cmd failed: event=%d err=%d", focus->item->cmd_event, err);
					return PAGE_SETTINGS_BINDING_RESULT_CMD_FAILED;
				}
				return PAGE_SETTINGS_BINDING_RESULT_CMD_SENT;
			}

			if(focus->item != NULL && focus->item->on_action != NULL) {
				focus->item->on_action(focus->item);
				binding_refresh_values_if_page_active();
				return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;
			}

			return PAGE_SETTINGS_BINDING_RESULT_UNHANDLED;

		case SETTINGS_VALUE_TYPE_LIST:
			if(focus->list_item != NULL && focus->list_item->on_action != NULL) {
				focus->list_item->on_action(focus->list_item, focus->list_item->user_data);
				return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;
			}

			return PAGE_SETTINGS_BINDING_RESULT_UNHANDLED;

		default:
			return PAGE_SETTINGS_BINDING_RESULT_UNHANDLED;
	}
}

page_settings_binding_result_t page_settings_binding_handle_slider_event(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	int index = page_settings_focus_find_index_by_control(lv_event_get_current_target(e));
	page_settings_item_focus_item_t *focus = page_settings_focus_get_by_index(index);
	if(focus == NULL || focus->value_type != SETTINGS_VALUE_TYPE_INT) {
		return PAGE_SETTINGS_BINDING_RESULT_NONE;
	}

	if(code == LV_EVENT_PRESSED) {
		page_settings_focus_set_index(index);
		binding_begin_edit(focus);
		return PAGE_SETTINGS_BINDING_RESULT_EDIT_BEGIN;
	}

	if(code == LV_EVENT_VALUE_CHANGED) {
		binding_sync_int_from_slider(focus);
		return PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED;
	}

	if(code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		binding_sync_int_from_slider(focus);
		binding_apply_edit(focus);
		return PAGE_SETTINGS_BINDING_RESULT_EDIT_APPLY;
	}

	return PAGE_SETTINGS_BINDING_RESULT_NONE;
}
