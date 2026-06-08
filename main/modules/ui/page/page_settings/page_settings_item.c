#include "page_settings_item.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "modules/ui/page/components/component_keyboard/component_keyboard.h"
#include "modules/ui/page/page_settings/page_settings_binding.h"
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/ui/page/page_settings/page_settings_focus.h"
#include "modules/ui/page/page_settings/page_settings_item_style.h"
#include "modules/ui/page/page_settings/page_settings_renderer.h"
#include "modules/ui/style/ui_style.h"
#include "core/utils/log.h"
#include "modules/ui/ui_actor.h"
#include "core/msg/msg.h"
#include "modules/ui/page/components/component_note/component_note.h"


static lv_obj_t *page_body;
static lv_obj_t *s_keyboard;
static char s_selected_wifi_ssid[33];
static char s_wifi_password[65];
static settings_item_id_t s_current_item_id;

#define PAGE_SETTINGS_LIST_VIEW_MAX 8

typedef struct {
	const settings_sub_item_t *item;
	lv_obj_t *body;
	lv_obj_t *line;
	size_t focus_insert_index;
} page_settings_list_view_t;

static page_settings_list_view_t s_list_views[PAGE_SETTINGS_LIST_VIEW_MAX];
static size_t s_list_view_count;

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

static void focus_rotate(int step);
static void focus_press(void);
static void page_msg_handler(const msg_t *msg);
static void focus_activate_from_touch(page_settings_item_focus_item_t *focus, void *user_data);



static void input_handler(const msg_t *msg)
{
	switch(msg->type) {
		case MSG_TYPE_INPUT:
			if(ui_note_is_visible()) {
				ui_note_handle_input(msg);
				return;
			}

			if(s_keyboard != NULL) {
				ui_keyboard_handle_input(s_keyboard, msg);
				return;
			}

			switch(msg->event) {
				case MSG_EVT_INPUT_ENCODER_CW:
					focus_rotate(+1);
					break;
				case MSG_EVT_INPUT_ENCODER_CCW:
					focus_rotate(-1);
					break;
				case MSG_EVT_INPUT_ENCODER_PRESS:
					focus_press();
					break;
				case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
					LOG("输入事件: 编码器长按");
					break;
				default:
					LOG("输入事件: 未处理的事件类型 %d", msg->event);
					break;
			}

			break;
		case MSG_TYPE_CMD:
			LOG("命令事件: 事件 %d", msg->event);
			break;

		case MSG_TYPE_SYS:
			LOG("系统事件: 事件 %d", msg->event);
			break;
	}
}

static const ui_page_ops_t page_settings_item_ops = {
	.on_input = input_handler,
	.on_msg = page_msg_handler,
};




static void wifi_password_keyboard_event_cb(ui_keyboard_event_t event, const char *text, void *user_data)
{
	const char *ssid = (const char *)user_data;

	switch(event) {
		case UI_KEYBOARD_EVT_SUBMIT: {
			msg_t msg = msg_make(MSG_SRC_LVGL, MSG_TYPE_CMD, MSG_EVT_CMD_WIFI_SET_CREDENTIALS, 0);
			(void)snprintf(msg.data.wifi_credentials.ssid, sizeof(msg.data.wifi_credentials.ssid), "%s", ssid ? ssid : "");
			(void)snprintf(msg.data.wifi_credentials.password, sizeof(msg.data.wifi_credentials.password), "%s", text ? text : "");

			esp_err_t err = msg_send_cmd(&msg, 0);
			if(err != ESP_OK) {
				LOG("wifi credentials cmd failed: err=%d", err);
			}

			s_keyboard = NULL;
			break;
		}

		case UI_KEYBOARD_EVT_CANCEL:
			LOG("wifi password input cancel");
			s_keyboard = NULL;
			break;

		case UI_KEYBOARD_EVT_CHANGED:
		default:
			break;
	}
}

static void focus_activate_from_touch(page_settings_item_focus_item_t *focus, void *user_data)
{
	if(focus != NULL && (focus->value_type == SETTINGS_VALUE_TYPE_LIST || focus->value_type == SETTINGS_VALUE_TYPE_ACTION)) {
		focus_press();
	}
}

static void focus_rotate(int step)
{
	(void)page_settings_binding_rotate_current(step);
}

static void focus_slider_event_cb(lv_event_t *e)
{
	(void)page_settings_binding_handle_slider_event(e);
}

static void focus_open_wifi_password_keyboard(page_settings_item_focus_item_t *focus)
{
	if(focus == NULL || focus->value_label == NULL || s_keyboard != NULL) {
		return;
	}

	const char *ssid = focus->value_str[0] != '\0' ? focus->value_str : lv_label_get_text(focus->value_label);
	(void)strncpy(s_selected_wifi_ssid, ssid ? ssid : "", sizeof(s_selected_wifi_ssid) - 1);
	s_selected_wifi_ssid[sizeof(s_selected_wifi_ssid) - 1] = '\0';
	s_wifi_password[0] = '\0';

	s_keyboard = ui_keyboard_modal_create(&(ui_keyboard_config_t) {
		.parent = page_body ? page_body : lv_scr_act(),
		.title = s_selected_wifi_ssid,
		.placeholder = "请输入 WiFi 密码",
		.buffer = s_wifi_password,
		.buffer_size = sizeof(s_wifi_password),
		.password_mode = true,
		.on_event = wifi_password_keyboard_event_cb,
		.user_data = s_selected_wifi_ssid,
	});
}

static void focus_press(void)
{
	page_settings_item_focus_item_t *focus = page_settings_focus_current();
	if(focus == NULL) {
		return;
	}

	switch(focus->value_type) {
		case SETTINGS_VALUE_TYPE_LIST:
			if(focus->list_item != NULL && focus->list_item->on_action != NULL) {
				(void)page_settings_binding_press(focus);
				break;
			}

			if(focus->sub_item_id == SETTINGS_SUB_ITEM_ID_WIFI_SSID_LIST) {
				focus_open_wifi_password_keyboard(focus);
			}
			break;

		default:
			(void)page_settings_binding_press(focus);
			break;
	}
}

static lv_obj_t *page_item_body;
// 选项内框
static lv_obj_t *page_item_in_body;

static const settings_item_t *setting_item;

static void list_views_reset(void)
{
	memset(s_list_views, 0, sizeof(s_list_views));
	s_list_view_count = 0;
}

static void list_view_render(page_settings_list_view_t *view)
{
	if(view == NULL || view->item == NULL || view->body == NULL) {
		return;
	}

	page_settings_focus_remove_sub_item(view->item->id);
	lv_obj_clean(view->body);

	settings_value_list_t *value_list = settings_list_items(view->item);
	size_t *value_count = settings_list_count(view->item);
	if(value_list == NULL || value_count == NULL || *value_count == 0) {
		lv_obj_add_flag(view->body, LV_OBJ_FLAG_HIDDEN);
		if(view->line != NULL) {
			lv_obj_add_flag(view->line, LV_OBJ_FLAG_HIDDEN);
		}
		return;
	}

	lv_obj_clear_flag(view->body, LV_OBJ_FLAG_HIDDEN);
	if(view->line != NULL) {
		lv_obj_clear_flag(view->line, LV_OBJ_FLAG_HIDDEN);
	}

	size_t focus_index = view->focus_insert_index;
	for(size_t i = 0; i < *value_count; i++) {
		settings_value_list_t *value_info = value_list + i;
		(void)page_settings_renderer_insert_list_row(
			view->body,
			view->item,
			value_info,
			value_info->title,
			value_info->value_str,
			value_info->disabled,
			value_info->selected,
			focus_index
		);

		if(!value_info->disabled) {
			focus_index++;
		}
	}
}

static void list_views_refresh_all(void)
{
	for(size_t i = 0; i < s_list_view_count; i++) {
		list_view_render(&s_list_views[i]);
	}
}

static void insert_settings_item_list(const settings_sub_item_t *item) {
	if(item == NULL || s_list_view_count >= PAGE_SETTINGS_LIST_VIEW_MAX) {
		return;
	}

	page_settings_list_view_t *view = &s_list_views[s_list_view_count++];
	view->item = item;
	view->body = page_settings_renderer_create_list_container(page_item_in_body);
	view->line = ui_style_insert_line_1(page_item_in_body);
	view->focus_insert_index = page_settings_focus_count();
	list_view_render(view);
}

// **************************************************
// 插入项目
// **************************************************
static void insert_settings_items() {
	size_t sub_items_count = 0;
    const settings_sub_item_t *sub_items = page_settings_get_sub_items(s_current_item_id, &sub_items_count);
    for(size_t i = 0; i < sub_items_count; i++) {
		switch (sub_items[i].value_type) {
			// 插入文本项
			case SETTINGS_VALUE_TYPE_TEXT:
			case SETTINGS_VALUE_TYPE_PASSWORD:
				page_settings_renderer_insert_text(page_item_in_body, &sub_items[i]);
				break;
			// 插入布尔项
			case SETTINGS_VALUE_TYPE_BOOL:
				page_settings_renderer_insert_bool(page_item_in_body, &sub_items[i]);
				break;
			// 插入列表项
			case SETTINGS_VALUE_TYPE_LIST:
				insert_settings_item_list(&sub_items[i]);
				break;
			case SETTINGS_VALUE_TYPE_INT:
				page_settings_renderer_insert_int(page_item_in_body, &sub_items[i], focus_slider_event_cb);
				break;
			case SETTINGS_VALUE_TYPE_ACTION:
				page_settings_renderer_insert_action(page_item_in_body, &sub_items[i]);
				break;
			default:
				LOG("未知的项目类型: %d", sub_items[i].value_type);
				break;
		}
	}
		
}

static void page_msg_handler(const msg_t *msg)
{
	if(msg == NULL) {
		return;
	}

	if(msg->type == MSG_TYPE_SYS) {
		list_views_refresh_all();
	}
}

static void create_page() {
	// 标题
    lv_obj_t *title_body = lv_obj_create(page_body);
    page_settings_item_apply_style_page_title_body(title_body);

    lv_obj_t *title_label = lv_label_create(title_body);
    page_settings_item_apply_style_page_title_label(title_label);
    lv_label_set_text(title_label, setting_item->title);


    page_item_body = lv_obj_create(page_body);
    page_settings_item_apply_style_page_item_body(page_item_body);

    // 选项内框
    page_item_in_body = lv_obj_create(page_item_body);
    page_settings_item_apply_style_page_item_in_body(page_item_in_body);


    // 插入选项
    insert_settings_items();
}

void page_settings_item_show(lv_obj_t *p, settings_item_id_t id) {
    if (p == NULL) {
        return;
    }
    setting_item = page_settings_find_item(id);
    if (setting_item == NULL) {
        return;
    }
	page_settings_focus_reset();
	page_settings_focus_set_activate_cb(focus_activate_from_touch, NULL);
	s_current_item_id = id;
	s_keyboard = NULL;
	list_views_reset();
	page_settings_item_style_init();
	page_body = lv_obj_create(p);
	lv_obj_add_style(page_body, &style_page_body, LV_STATE_DEFAULT);
	ui_add_top_status(page_body);
	create_page();

	ui_actor_set_ops(&page_settings_item_ops);
}
