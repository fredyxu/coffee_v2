#include "modules/ui/page/page_settings/page_settings_renderer.h"

#include "modules/ui/page/page_settings/page_settings_focus.h"
#include "modules/ui/page/page_settings/page_settings_item_style.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/theme/color.h"
#include "modules/ui/theme/font.h"

static settings_value_list_t *settings_list_items(const settings_sub_item_t *item)
{
	if(item == NULL) {
		return NULL;
	}

	if(item->value_source != NULL) {
		return item->value_source->list;
	}

	return NULL;
}

static size_t *settings_list_count(const settings_sub_item_t *item)
{
	if(item == NULL) {
		return NULL;
	}

	if(item->value_source != NULL) {
		return item->value_source->count;
	}

	return NULL;
}

lv_obj_t *page_settings_renderer_create_list_container(lv_obj_t *parent)
{
	lv_obj_t *container = lv_obj_create(parent);
	lv_obj_set_width(container, LV_PCT(100));
	lv_obj_set_height(container, LV_SIZE_CONTENT);
	lv_obj_set_layout(container, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_bg_opa(container, LV_OPA_0, 0);
	lv_obj_set_style_border_width(container, 0, 0);
	lv_obj_set_style_pad_all(container, 0, 0);
	lv_obj_set_style_pad_row(container, 0, 0);
	lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
	return container;
}

lv_obj_t *page_settings_renderer_insert_list_row(lv_obj_t *parent,
												 const settings_sub_item_t *item,
												 const settings_value_list_t *list_item,
												 const char *title,
												 const char *value_str,
												 bool disabled,
												 bool selected,
												 size_t focus_index)
{
	lv_obj_t *obj_body = lv_obj_create(parent);
	lv_obj_t *obj_title_label = lv_label_create(obj_body);
	lv_label_set_text(obj_title_label, title ? title : "");

	page_settings_item_apply_style_page_item_list(
		obj_body,
		obj_title_label,
		selected
	);
	if(list_item != NULL) {
		page_settings_focus_add_list_at(item, list_item, obj_body, obj_title_label, disabled, focus_index);
	} else {
		page_settings_focus_add_at(item, obj_body, NULL, obj_title_label, value_str, disabled, focus_index);
	}

	if(disabled) {
		lv_obj_clear_flag(obj_body, LV_OBJ_FLAG_CLICKABLE);
	}

	return obj_title_label;
}

void page_settings_renderer_insert_static_list(lv_obj_t *parent, const settings_sub_item_t *item)
{
	settings_value_list_t *value_list = settings_list_items(item);
	size_t *value_count = settings_list_count(item);
	if(item == NULL || value_list == NULL || value_count == NULL) {
		return;
	}

	for(size_t i = 0; i < *value_count; i++) {
		settings_value_list_t *value_info = value_list + i;
		(void)page_settings_renderer_insert_list_row(
			parent,
			item,
			value_info,
			value_info->title,
			value_info->value_str,
			value_info->disabled,
			value_info->selected,
			page_settings_focus_count()
		);
	}
	ui_style_insert_line_1(parent);
}

void page_settings_renderer_insert_action(lv_obj_t *parent, const settings_sub_item_t *item)
{
	if(item == NULL) {
		return;
	}

	(void)page_settings_renderer_insert_list_row(
		parent,
		item,
		NULL,
		item->title,
		NULL,
		false,
		false,
		page_settings_focus_count()
	);
	ui_style_insert_line_1(parent);
}

void page_settings_renderer_insert_text(lv_obj_t *parent, const settings_sub_item_t *item)
{
	if(item == NULL) {
		return;
	}

	lv_obj_t *obj_body = lv_obj_create(parent);
	lv_obj_t *obj_title_body = lv_obj_create(obj_body);
	lv_obj_t *obj_title_label = lv_label_create(obj_title_body);
	lv_obj_t *obj_value_label = lv_label_create(obj_title_body);
	lv_label_set_text(obj_title_label, item->title ? item->title : "");
	lv_label_set_text(obj_value_label, item->value != NULL ? (const char *)item->value : "");

	page_settings_item_apply_style_page_item_text(obj_body);
	lv_obj_set_style_text_font(obj_title_label, UI_FONT_12, 0);
	lv_obj_set_style_text_color(obj_title_label, UI_COLOR_TEXT, 0);
	lv_obj_set_style_text_font(obj_value_label, UI_FONT_12, 0);
	lv_obj_set_style_text_color(obj_value_label, UI_COLOR_TEXT_MUTED, 0);
	page_settings_focus_add(item, obj_body, NULL, obj_value_label, false);
	ui_style_insert_line_1(parent);
}

void page_settings_renderer_insert_bool(lv_obj_t *parent, const settings_sub_item_t *item)
{
	if(item == NULL) {
		return;
	}

	lv_obj_t *obj_body = lv_obj_create(parent);
	lv_obj_t *obj_title_body = lv_obj_create(obj_body);
	lv_obj_t *obj_title_label = lv_label_create(obj_title_body);
	lv_label_set_text(obj_title_label, item->title);
	lv_obj_t *obj_switch = lv_switch_create(obj_body);
	lv_obj_clear_flag(obj_switch, LV_OBJ_FLAG_CLICKABLE);

	if(item->value != NULL && *(bool *)item->value) {
		lv_obj_add_state(obj_switch, LV_STATE_CHECKED);
	} else {
		lv_obj_remove_state(obj_switch, LV_STATE_CHECKED);
	}

	page_settings_item_apply_style_page_item_bool(obj_body, obj_title_body, obj_title_label, obj_switch);
	page_settings_focus_add(item, obj_body, obj_switch, NULL, false);
	ui_style_insert_line_1(parent);
}

void page_settings_renderer_insert_int(lv_obj_t *parent,
									   const settings_sub_item_t *item,
									   lv_event_cb_t slider_event_cb)
{
	if(item == NULL || item->value == NULL) {
		return;
	}

	lv_obj_t *obj_body = lv_obj_create(parent);
	lv_obj_t *obj_title_body = lv_obj_create(obj_body);
	lv_obj_t *obj_title_label = lv_label_create(obj_title_body);
	lv_obj_t *obj_value_label = lv_label_create(obj_title_body);
	lv_obj_t *obj_slider_body = lv_obj_create(obj_body);
	lv_obj_t *obj_slider = lv_slider_create(obj_slider_body);
	lv_slider_set_range(obj_slider, item->min_value, item->max_value);
	lv_slider_set_value(obj_slider, *(int *)item->value, LV_ANIM_OFF);

	lv_label_set_text(obj_title_label, item->title);
	lv_label_set_text_fmt(obj_value_label, "%d", *(int *)item->value);

	page_settings_item_apply_style_page_item_int(
		obj_body,
		obj_title_body,
		obj_title_label,
		obj_value_label,
		obj_slider_body,
		obj_slider
	);

	size_t focus_index = page_settings_focus_count();
	page_settings_focus_add(item, obj_body, obj_slider, obj_value_label, false);
	if(page_settings_focus_count() > focus_index && slider_event_cb != NULL) {
		lv_obj_add_event_cb(obj_slider, slider_event_cb, LV_EVENT_PRESSED, NULL);
		lv_obj_add_event_cb(obj_slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
		lv_obj_add_event_cb(obj_slider, slider_event_cb, LV_EVENT_RELEASED, NULL);
		lv_obj_add_event_cb(obj_slider, slider_event_cb, LV_EVENT_PRESS_LOST, NULL);
	}
}
