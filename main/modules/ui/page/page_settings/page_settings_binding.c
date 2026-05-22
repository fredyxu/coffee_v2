#include "modules/ui/page/page_settings/page_settings_binding.h"

#include "esp_err.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"

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
		lv_label_set_text_fmt(focus->value_label, "%d", focus->value_int);
	}
}

static void binding_toggle_bool(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->item == NULL || focus->item->value == NULL || focus->control == NULL) {
		return;
	}

	bool *value = (bool *)focus->item->value;
	*value = !(*value);
	focus->value_bool = *value;

	if(*value) {
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
		focus->value_int = *(int *)focus->item->value;
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
		*(int *)focus->item->value = focus->value_int;
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
	*(int *)focus->item->value = focus->value_int;

	if(focus->value_label != NULL) {
		lv_label_set_text_fmt(focus->value_label, "%d", focus->value_int);
	}
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
