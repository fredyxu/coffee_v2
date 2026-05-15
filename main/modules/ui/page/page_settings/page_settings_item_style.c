#include "modules/ui/page/page_settings/page_settings_item_style.h"

#include "config/config_sys.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/theme/color.h"
#include "modules/ui/theme/font.h"

#define SETTINGS_ITEM_HEIGHT 25

static bool style_init_done = false;

// 页面样式
static lv_style_t style_page_title_body;
static lv_style_t style_page_title_label;
// 选项外框
static lv_style_t style_page_item_body;
// 选项内框
static lv_style_t style_page_item_in_body;

// 字符串项目样式
static lv_style_t style_text_body;




// **************************************************
// 列表样式及样式设置
// **************************************************
static lv_style_t style_list_body;
static lv_style_t style_list_title_label;

// 列表样式初始化
static void style_init_item_value_list() {
	ui_style_init_row(&style_list_body);
	lv_style_set_size(&style_list_body, LV_PCT(100), SETTINGS_ITEM_HEIGHT );
	lv_style_set_bg_color(&style_list_body, UI_COLOR_PANEL_2);

	lv_style_init(&style_list_title_label);
	lv_style_set_text_font(&style_list_title_label, UI_FONT_12);
	lv_style_set_text_color(&style_list_title_label, UI_COLOR_TEXT);

}

void page_settings_item_apply_style_page_item_list(
	lv_obj_t *obj_body,
	lv_obj_t *obj_title_label) 
{
	lv_obj_add_style(obj_body, &style_list_body, 0);
	lv_obj_add_style(obj_title_label, &style_list_title_label, 0);
}



// **************************************************
// 开关样式及应用
// **************************************************
static lv_style_t style_bool_body;
static lv_style_t style_bool_title_body;
static lv_style_t style_bool_title_label;
static lv_style_t style_bool_switch;

// 开关
static void style_init_item_value_bool() {
    // 外框
    ui_style_init_row(&style_bool_body);
    lv_style_set_size(&style_bool_body, LV_PCT(100), SETTINGS_ITEM_HEIGHT);
    // lv_style_set_bg_color(&style_bool_body, UI_COLOR_ACCENT);
	lv_style_set_bg_color(&style_bool_body, UI_COLOR_PANEL_2);
    lv_style_set_flex_main_place(&style_bool_body, LV_FLEX_ALIGN_SPACE_BETWEEN);
    lv_style_set_flex_cross_place(&style_bool_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_bool_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_margin_top(&style_bool_body, 5);

    // 标题外框
    ui_style_init_row(&style_bool_title_body);
    lv_style_set_size(&style_bool_title_body, LV_PCT(70), SETTINGS_ITEM_HEIGHT);
    lv_style_set_bg_color(&style_bool_title_body, UI_COLOR_PANEL_2);
    lv_style_set_flex_main_place(&style_bool_title_body, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_bool_title_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_bool_title_body, LV_FLEX_ALIGN_CENTER);

    // 标题LABEL
    lv_style_init(&style_bool_title_label);
    lv_style_set_text_align(&style_bool_title_label, LV_TEXT_ALIGN_LEFT);
    lv_style_set_text_font(&style_bool_title_label, UI_FONT_12);
    lv_style_set_text_color(&style_bool_title_label, UI_COLOR_TEXT);
    // lv_style_set_pad_top(&style_bool_title_label, 7);

    // 开关
    lv_style_init(&style_bool_switch);
    lv_style_set_size(&style_bool_switch, 40, 20);
}

void page_settings_item_apply_style_page_item_bool(
	lv_obj_t *obj_body, 
	lv_obj_t *obj_title_body,
	lv_obj_t *obj_title_label,
	lv_obj_t *obj_switch
) {
    lv_obj_add_style(obj_body, &style_bool_body, 0);
    lv_obj_add_style(obj_title_body, &style_bool_title_body, 0);
    lv_obj_add_style(obj_title_label, &style_bool_title_label, 0);
    lv_obj_add_style(obj_switch, &style_bool_switch, 0);
}

// 字符串项目初始化
static void style_init_item_value_char() {
    ui_style_init_row(&style_text_body);
    lv_style_set_size(&style_text_body, LV_PCT(100), SETTINGS_ITEM_HEIGHT * 2);
    lv_style_set_bg_color(&style_text_body, UI_COLOR_PANEL_2);
}

void page_settings_item_style_init() {
    if (style_init_done) {
        return;
    }

    // 标题
    ui_style_init_row(&style_page_title_body);
    lv_style_set_size(&style_page_title_body, DISPLAY_H_RES, SETTINGS_ITEM_HEIGHT);
    // lv_style_set_bg_color(&style_page_title_body, UI_COLOR_PANEL_2);
    lv_style_set_flex_main_place(&style_page_title_body, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_page_title_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_page_title_body, LV_FLEX_ALIGN_CENTER);

    // 标题LABEL
    ui_style_init_row(&style_page_title_label);
    lv_style_set_size(&style_page_title_label, LV_PCT(100), LV_PCT(100));
    lv_style_set_text_font(&style_page_title_label, UI_FONT_14);
    lv_style_set_text_color(&style_page_title_label, UI_COLOR_TEXT);
    lv_style_set_pad_left(&style_page_title_label, CONFIG_UI_MARGIN * 2);
    lv_style_set_flex_main_place(&style_page_title_label, LV_FLEX_ALIGN_START);
    lv_style_set_flex_cross_place(&style_page_title_label, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_page_title_label, LV_FLEX_ALIGN_CENTER);

    // 选项背景外框
    ui_style_init_column(&style_page_item_body);
    lv_style_set_radius(&style_page_item_body, CONFIG_UI_RADIUS);
    lv_style_set_bg_color(&style_page_item_body, UI_COLOR_PANEL_2);
    // lv_style_set_size(&style_page_item_body,
    // 	DISPLAY_H_RES,
    // 	DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT - SETTINGS_ITEM_TITLE_HEIGHT - 1 -
    // CONFIG_UI_MARGIN * 6
    // );
    lv_style_set_flex_grow(&style_page_item_body, 1);
    lv_style_set_width(&style_page_item_body, LV_PCT(100));
    lv_style_set_flex_main_place(&style_page_item_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_cross_place(&style_page_item_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&style_page_item_body, LV_FLEX_ALIGN_CENTER);
    lv_style_set_radius(&style_page_item_body, 0);

    // 选项内框
    ui_style_init_column(&style_page_item_in_body);
    lv_style_set_bg_color(&style_page_item_in_body, UI_COLOR_PANEL_2);
	// lv_style_set_bg_color(&style_page_item_in_body, UI_COLOR_ACCENT);
    lv_style_set_size(&style_page_item_in_body, DISPLAY_H_RES, LV_PCT(100));
    lv_style_set_pad_left(&style_page_item_in_body, CONFIG_UI_MARGIN*3);
    lv_style_set_pad_right(&style_page_item_in_body, CONFIG_UI_MARGIN * 4);
    lv_style_set_radius(&style_page_item_in_body, 0);

    style_init_item_value_char();
    style_init_item_value_bool();
	style_init_item_value_list();

    style_init_done = true;
}

void page_settings_item_apply_style_page_title_body(lv_obj_t *obj) {
    lv_obj_add_style(obj, &style_page_title_body, 0);
}

void page_settings_item_apply_style_page_title_label(lv_obj_t *obj) {
    lv_obj_add_style(obj, &style_page_title_label, 0);
}

void page_settings_item_apply_style_page_item_body(lv_obj_t *obj) {
    lv_obj_add_style(obj, &style_page_item_body, 0);
}

void page_settings_item_apply_style_page_item_in_body(lv_obj_t *obj) {
    lv_obj_add_style(obj, &style_page_item_in_body, 0);
}

// 文字项目
void page_settings_item_apply_style_page_item_text(lv_obj_t *obj_body) {
    lv_obj_add_style(obj_body, &style_text_body, 0);
}
