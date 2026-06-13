#include "page_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "esp_err.h"
#include "config/config_ui.h"
#include "config/config_sys.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/theme/color.h"
#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/ui/theme/font.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/ui/page/components/component_note/component_note.h"
#include "modules/ui/ui.h"
#include "modules/ui/ui_actor.h"


// #define SETTINGS_PAGE_WIDTH (DISPLAY_H_RES - CONFIG_UI_MARGIN * 2)
#define SETTINGS_PAGE_WIDTH DISPLAY_H_RES
#define SETTINGS_PAGE_BG_COLOR UI_COLOR_PANEL_2
#define SETTINGS_MARGIN 10
#define SETTINGS_ITEM_HEIGHT 15
#define SETTINGS_ITEM_MAX 16


// static settings_item_id_t current_item_id = SETTINGS_ITEM_ID_BACK;
static lv_obj_t *page_body;
static lv_obj_t *settings_body;
static lv_obj_t *s_item_objs[SETTINGS_ITEM_MAX];
static const settings_item_t *s_items;
static size_t s_item_count;
static int s_focus_index;


// static lv_style_t style_page_body;
static lv_style_t style_item_body;
static lv_style_t style_item_title_body;
static lv_style_t style_item_value_body;
static lv_style_t style_item_title_label;
static lv_style_t style_item_value_label;
static lv_style_t style_item_n_body;


static bool style_init_done = false;

static void settings_focus_set_index(int index);
static void settings_focus_move(int step);
static void settings_focus_press(void);
static void settings_item_activate(const settings_item_t *item);

static void page_settings_input_handler(const msg_t *msg)
{
	if(msg == NULL || msg->type != MSG_TYPE_INPUT) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_INPUT_ENCODER_CW:
			settings_focus_move(+1);
			break;

		case MSG_EVT_INPUT_ENCODER_CCW:
			settings_focus_move(-1);
			break;

		case MSG_EVT_INPUT_ENCODER_PRESS:
			settings_focus_press();
			break;

		case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
		default:
			break;
	}
}

static const ui_page_ops_t page_settings_ops = {
	.on_input = page_settings_input_handler,
};

static void settings_make_display_only(lv_obj_t *obj)
{
	if(obj == NULL) {
		return;
	}

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
	lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static int settings_find_item_index(const settings_item_t *item)
{
	if(item == NULL || s_items == NULL) {
		return -1;
	}

	for(size_t i = 0; i < s_item_count; i++) {
		if(&s_items[i] == item || s_items[i].id == item->id) {
			return (int)i;
		}
	}

	return -1;
}

static void settings_item_activate(const settings_item_t *item)
{
	if(item == NULL) {
		return;
	}

	if(item->id == SETTINGS_ITEM_ID_BACK) {
		ui_nav_back();
		return;
	}

	ui_nav_go((ui_page_nav_param_t) {
		.page_id = PAGE_SETTINGS_ITEM,
		.settings_item_id = item->id,
	});
}

static void settings_focus_set_index(int index)
{
	if(s_item_count == 0 || index < 0 || index >= (int)s_item_count) {
		return;
	}

	for(size_t i = 0; i < s_item_count; i++) {
		if(s_item_objs[i] != NULL) {
			lv_obj_remove_state(s_item_objs[i], LV_STATE_FOCUSED);
		}
	}

	s_focus_index = index;

	if(s_item_objs[s_focus_index] != NULL) {
		lv_obj_add_state(s_item_objs[s_focus_index], LV_STATE_FOCUSED);
		lv_obj_scroll_to_view_recursive(s_item_objs[s_focus_index], LV_ANIM_OFF);
	}
}

static void settings_focus_move(int step)
{
	if(s_item_count == 0) {
		return;
	}

	int index = s_focus_index + step;
	if(index < 0) {
		index = (int)s_item_count - 1;
	} else if(index >= (int)s_item_count) {
		index = 0;
	}

	settings_focus_set_index(index);
}

static void settings_focus_press(void)
{
	if(s_items == NULL || s_focus_index < 0 || s_focus_index >= (int)s_item_count) {
		return;
	}

	settings_item_activate(&s_items[s_focus_index]);
}

static void settings_item_clicked_cb(lv_event_t *e)
{
	if(lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	const settings_item_t *item = (const settings_item_t *)lv_event_get_user_data(e);
	if(item == NULL) {
		return;
	}

	int index = settings_find_item_index(item);
	if(index >= 0) {
		settings_focus_set_index(index);
	}

	settings_item_activate(item);

	lv_indev_t *indev = lv_indev_active();
	if(indev != NULL) {
		lv_indev_wait_release(indev);
	}
}

void insert_settings_items() {
	if(page_body == NULL) {
		return;
	}

	s_items = page_settings_get_items(&s_item_count);
	if(s_items == NULL) {
		return;
	}
	if(s_item_count > SETTINGS_ITEM_MAX) {
		s_item_count = SETTINGS_ITEM_MAX;
	}

	for(size_t i = 0; i < s_item_count; i++) {

		// 外框
		lv_obj_t *item_body = lv_obj_create(settings_body);
		
		lv_obj_add_style(item_body, &style_item_body, 0);
		// lv_obj_set_style_bg_color(item_body, UI_COLOR_FOCUS_1, LV_STATE_FOCUSED);
		
		lv_obj_remove_flag(item_body, LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_add_flag(item_body, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_clear_flag(item_body, LV_OBJ_FLAG_CLICK_FOCUSABLE);
		lv_obj_add_event_cb(item_body, settings_item_clicked_cb, LV_EVENT_CLICKED, (void *)&s_items[i]);

		// 内框
		lv_obj_t *item_n_body = lv_obj_create(item_body);
		lv_obj_add_style(item_n_body, &style_item_n_body, LV_STATE_DEFAULT);
		lv_obj_set_style_border_color(item_n_body, UI_COLOR_ACCENT, LV_STATE_FOCUSED);
		settings_make_display_only(item_n_body);
		s_item_objs[i] = item_n_body;

		// 内框标题
		lv_obj_t *item_title_body = lv_obj_create(item_n_body);
		lv_obj_add_style(item_title_body, &style_item_title_body, 0);
		settings_make_display_only(item_title_body);

		lv_obj_t *label_item_title = lv_label_create(item_title_body);
		lv_obj_add_style(label_item_title, &style_item_title_label, 0);
		lv_label_set_text(label_item_title, s_items[i].title);
		settings_make_display_only(label_item_title);
		lv_obj_center(label_item_title);

		// lv_obj_t *item_value_body = lv_obj_create(item_body);
		// lv_obj_add_style(item_value_body, &style_item_value_body, 0);
		// settings_make_display_only(item_value_body);
		
		// lv_obj_t *item_value_label = lv_label_create(item_value_body);
		// lv_obj_add_style(item_value_label, &style_item_value_label, 0);
		// lv_label_set_text(item_value_label, s_items[i].subtitle);
		// settings_make_display_only(item_value_label);
		
	}

	if(s_item_count > 0) {
		settings_focus_set_index(0);
	}
}

static void create_page() {
	ui_add_top_status(page_body);

	settings_body = lv_obj_create(page_body);
	lv_obj_set_style_border_width(settings_body, 0, 0);
	
	lv_obj_set_style_radius(settings_body, CONFIG_UI_RADIUS, 0);
	lv_obj_set_style_size(
		settings_body, 
		SETTINGS_PAGE_WIDTH, 
		DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT,
		// DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT - CONFIG_UI_MARGIN * 3, 
	LV_STATE_DEFAULT);
	lv_obj_set_style_bg_color(settings_body, SETTINGS_PAGE_BG_COLOR, 0);
	lv_obj_set_style_pad_row(settings_body, CONFIG_UI_MARGIN, 0);
	lv_obj_set_style_margin_all(settings_body, 0, 0);
	lv_obj_set_style_pad_all(settings_body, 0, 0);
	// lv_obj_remove_flag(settings_body, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
	lv_obj_set_layout(settings_body, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(settings_body, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(settings_body, 
		LV_FLEX_ALIGN_START,
		LV_FLEX_ALIGN_CENTER,
		LV_FLEX_ALIGN_CENTER
	);
	
	insert_settings_items();
}

void style_init(void) {
	if(style_init_done) {
		return;
	}

	// 项目背景
	// lv_style_init(&style_item_body);
	ui_style_init_column(&style_item_body);
	lv_style_set_size(&style_item_body, SETTINGS_PAGE_WIDTH, SETTINGS_ITEM_HEIGHT + 25);
	lv_style_set_radius(&style_item_body, 0);
	lv_style_set_border_width(&style_item_body, 1);
	lv_style_set_border_side(&style_item_body, LV_BORDER_SIDE_BOTTOM);
	// lv_style_set_border_bottom(&style_item_body, 1);
	lv_style_set_border_color(&style_item_body, UI_COLOR_BORDER);
	lv_style_set_bg_color(&style_item_body, SETTINGS_PAGE_BG_COLOR);
	lv_style_set_pad_all(&style_item_body, 5);
	lv_style_set_margin_all(&style_item_body, 0);
	lv_style_set_radius(&style_item_body, CONFIG_UI_RADIUS);

	// lv_style_set_layout(&style_item_body, LV_LAYOUT_FLEX);
	
	// lv_style_set_flex_flow(&style_item_body, LV_FLEX_FLOW_COLUMN);
	lv_style_set_flex_main_place(&style_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&style_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&style_item_body, LV_FLEX_ALIGN_CENTER);


	// 项目内框
	// lv_style_init(&style_item_n_body);
	ui_style_init_row(&style_item_n_body);
	lv_style_set_size(
		&style_item_n_body, 
		LV_PCT(90), 
		LV_PCT(100));
	lv_style_set_border_width(&style_item_n_body, 1);
	lv_style_set_radius(&style_item_n_body, CONFIG_UI_RADIUS);
	lv_style_set_bg_opa(&style_item_n_body, LV_OPA_0);
	lv_style_set_pad_hor(&style_item_n_body, 10);
	lv_style_set_border_color(&style_item_n_body, SETTINGS_PAGE_BG_COLOR);


	// 标题样式
	// lv_style_init(&style_item_title_body);
	ui_style_init_row(&style_item_title_body);
	lv_style_set_size(&style_item_title_body, 
		LV_PCT(95), 
		// SETTINGS_ITEM_HEIGHT
		LV_PCT(90)
	);
	lv_style_set_radius(&style_item_title_body, 0);
	lv_style_set_bg_color(&style_item_title_body, SETTINGS_PAGE_BG_COLOR);
	lv_style_set_border_width(&style_item_title_body, 0);
	// lv_style_set_layout(&style_item_title_body, LV_LAYOUT_FLEX);
	// lv_style_set_flex_flow(&style_item_title_body, LV_FLEX_FLOW_ROW);
	lv_style_set_flex_main_place(&style_item_title_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_cross_place(&style_item_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&style_item_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_margin_all(&style_item_title_body, 0);
	lv_style_set_pad_all(&style_item_title_body, 0);

	// 标题LABEL
	lv_style_init(&style_item_title_label);
	lv_style_set_border_width(&style_item_title_label, 0);
	lv_style_set_text_color(&style_item_title_label, UI_COLOR_TEXT);
	lv_style_set_margin_all(&style_item_title_label, 0);
	lv_style_set_pad_all(&style_item_title_label, 0);
	lv_style_set_text_font(&style_item_title_label, UI_FONT_14);

	// 参数数值BODY
	ui_style_init_row(&style_item_value_body);
	lv_style_set_size(&style_item_value_body, 
		SETTINGS_PAGE_WIDTH - SETTINGS_MARGIN * 2,
		SETTINGS_ITEM_HEIGHT
	);
	lv_style_set_bg_color(&style_item_value_body, SETTINGS_PAGE_BG_COLOR);

	ui_style_init_row(&style_item_value_label);
	lv_style_set_text_font(&style_item_value_label, UI_FONT_12);
	lv_style_set_text_color(&style_item_value_label, UI_COLOR_TEXT_MUTED);

	

	// 数值样式
	// lv_style_init(&style_item_value_body);
	// lv_style_set_size(&style_item_value_body, SETTINGS_PAGE_WIDTH - SETTINGS_MARGIN * 2, 15);
}


esp_err_t page_settings_show(lv_obj_t *p) {
	if(p == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

	s_items = NULL;
	s_item_count = 0;
	s_focus_index = -1;
	for(size_t i = 0; i < SETTINGS_ITEM_MAX; i++) {
		s_item_objs[i] = NULL;
	}

	page_body = lv_obj_create(p);
	lv_obj_add_style(page_body, &style_page_body, 0);

	style_init();

	create_page();

	ui_actor_set_ops(&page_settings_ops);
	

	return ESP_OK;
}
