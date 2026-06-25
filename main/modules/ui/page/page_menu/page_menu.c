#include "page_menu.h"

#include "config/config_sys_display.h"
#include "lvgl.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "config/config_ui.h"
#include "modules/ui/theme/color.h"
#include "modules/ui/theme/font.h"
#include "ui/ui.h"

#define PAGE_MENU_ITEM_WIDTH 80
#define PAGE_MENU_ITEM_HEIGHT 80
#define PAGE_MENU_ICON_WIDTH 50
#define PAGE_MENU_ICON_HEIGHT PAGE_MENU_ICON_WIDTH

static bool style_init_done = false;
static lv_style_t s_style_page_body;
static lv_style_t s_style_main_body;
static lv_style_t s_style_item_body;
static lv_style_t s_style_icon_body;
static lv_style_t s_style_icon_label;
static lv_style_t s_style_title_body;
static lv_style_t s_style_title_label;


static menu_item_t menu_item[] = {
	{
		.title = "首页",
		.icon = ICON_MENU_SHOUYE,
		.page_id = PAGE_HOME,
	},
	{
		.title = "对讲",
		.icon = ICON_MENU_DUIJIANG,
		.page_id = PAGE_TALK,
	},
	// {
	// 	.title =  "练习",
	// 	.icon = ICON_MENU_LIANXI,
	// 	.page_id = PAGE_NONE,
	// },
	{
		.title = "设置",
		.icon = ICON_MENU_SHEZHI,
		.page_id = PAGE_SETTINGS,
	},
	// {
	// 	.title = "关于",
	// 	.icon = ICON_MENU_GUANYU,
	// 	.page_id = PAGE_NONE,
	// }
};

#define MENU_ITEM_QTY (sizeof(menu_item) / sizeof(menu_item[0]))

static void style_init(void) {
	if(style_init_done) {
		return;
	}
	// 主体样式
	ui_style_init_column(&s_style_page_body);
	lv_style_set_size(&s_style_page_body, DISPLAY_H_RES, DISPLAY_V_RES);
	lv_style_set_bg_color(&s_style_page_body, UI_COLOR_PANEL_2);
	lv_style_set_flex_main_place(&s_style_page_body, LV_FLEX_ALIGN_START);
	// lv_style_set_flex_cross_place(&s_style_page_body, LV_FLEX_ALIGN_START);
	// lv_style_set_flex_track_place(&s_style_page_body, LV_FLEX_ALIGN_START);

	// 主体内容
	ui_style_init_row(&s_style_main_body);
	lv_style_set_flex_flow(&s_style_main_body, LV_FLEX_FLOW_ROW_WRAP);
	lv_style_set_bg_opa(&s_style_main_body, LV_OPA_TRANSP);
	lv_style_set_size(&s_style_main_body, DISPLAY_H_RES - 10, DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT - 10);
	lv_style_set_margin_all(&s_style_main_body, 5);
	lv_style_set_flex_main_place(&s_style_main_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_cross_place(&s_style_main_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_track_place(&s_style_main_body, LV_FLEX_ALIGN_START);
	lv_style_set_pad_all(&s_style_main_body, 1);


	// 菜单项目外框
	ui_style_init_column(&s_style_item_body);
	lv_style_set_size(&s_style_item_body, PAGE_MENU_ITEM_WIDTH, PAGE_MENU_ITEM_HEIGHT);
	lv_style_set_border_width(&s_style_item_body, 0);
	lv_style_set_border_color(&s_style_item_body, UI_COLOR_BORDER);
	lv_style_set_bg_color(&s_style_item_body, UI_COLOR_PANEL_2);
	lv_style_set_radius(&s_style_item_body, CONFIG_UI_RADIUS);
	lv_style_set_margin_all(&s_style_item_body, 10);
	lv_style_set_flex_main_place(&s_style_item_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_cross_place(&s_style_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_item_body, LV_FLEX_ALIGN_CENTER);

	// 图标外框
	ui_style_init_column(&s_style_icon_body);
	lv_style_set_size(&s_style_icon_body, PAGE_MENU_ICON_WIDTH, PAGE_MENU_ICON_HEIGHT);
	lv_style_set_border_width(&s_style_icon_body, 1);
	lv_style_set_border_color(&s_style_icon_body, UI_COLOR_BORDER);
	lv_style_set_bg_opa(&s_style_icon_body, LV_OPA_TRANSP);
	lv_style_set_flex_main_place(&s_style_icon_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&s_style_icon_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_icon_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_radius(&s_style_icon_body, CONFIG_UI_RADIUS);

	// 图标LABEL
	lv_style_init(&s_style_icon_label);
	lv_style_set_text_font(&s_style_icon_label, FONT_MENU_ICON);
	lv_style_set_text_color(&s_style_icon_label, UI_COLOR_TEXT);

	// 标题外框
	ui_style_init_row(&s_style_title_body);
	lv_style_set_size(&s_style_title_body, LV_PCT(100), PAGE_MENU_ITEM_HEIGHT - PAGE_MENU_ICON_HEIGHT - 2);
	lv_style_set_flex_main_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_bg_opa(&s_style_title_body, LV_OPA_TRANSP);
	// 标题LABEL
	lv_style_init(&s_style_title_label);
	lv_style_set_text_font(&s_style_title_label, UI_FONT_14);
	lv_style_set_text_color(&s_style_title_label, UI_COLOR_TEXT);

	style_init_done = true;
}

static void go_page(lv_event_t *e) {
	if(lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	const menu_item_t *item = lv_event_get_user_data(e);
	if(item == NULL) {
		return;
	}

	if(item->page_id <= PAGE_INIT || item->page_id >= PAGE_NONE) {
		return;
	}

	ui_nav_go((ui_page_nav_param_t) {
		.page_id = item->page_id,
	});
}

esp_err_t page_menu_show(lv_obj_t *p) {
	(void)style_init();

	

	lv_obj_t *page_body = lv_obj_create(p);
	lv_obj_add_style(page_body, &s_style_page_body, LV_STATE_DEFAULT);

	ui_add_top_status(page_body);

	lv_obj_t *main_body = lv_obj_create(page_body);
	lv_obj_add_style(main_body, &s_style_main_body, LV_STATE_DEFAULT);

	for(int i = 0; i < MENU_ITEM_QTY; i ++) {
		// 外框
		lv_obj_t *item_body = lv_obj_create(main_body);
		lv_obj_add_style(item_body, &s_style_item_body, LV_STATE_DEFAULT);
		

		// 图标外框
		lv_obj_t *item_icon_body = lv_obj_create(item_body);
		lv_obj_add_style(item_icon_body, &s_style_icon_body, LV_STATE_DEFAULT);

		lv_obj_t *item_icon_label = lv_label_create(item_icon_body);
		lv_obj_add_style(item_icon_label, &s_style_icon_label, LV_STATE_DEFAULT);
		lv_obj_center(item_icon_label);
		lv_label_set_text(item_icon_label, menu_item[i].icon);

		// 标题
		lv_obj_t *item_title_body = lv_obj_create(item_body);
		lv_obj_add_style(item_title_body, &s_style_title_body, LV_STATE_DEFAULT);
		
		lv_obj_t *item_title_label = lv_label_create(item_title_body);
		lv_obj_add_style(item_title_label, &s_style_title_label, LV_STATE_DEFAULT);
		lv_label_set_text(item_title_label, menu_item[i].title);
		lv_obj_center(item_title_label);

		// lv_obj_add_flag(item_icon_body, LV_OBJ_FLAG_EVENT_BUBBLE);
		// lv_obj_add_flag(item_icon_label, LV_OBJ_FLAG_EVENT_BUBBLE);
		// lv_obj_add_flag(item_title_body, LV_OBJ_FLAG_EVENT_BUBBLE);
		// lv_obj_add_flag(item_title_label, LV_OBJ_FLAG_EVENT_BUBBLE);


		lv_obj_add_flag(item_body, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(item_body, go_page, LV_EVENT_CLICKED, (void *)&menu_item[i]);

		lv_obj_remove_flag(item_icon_body, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_remove_flag(item_icon_label, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_remove_flag(item_title_body, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_remove_flag(item_title_label, LV_OBJ_FLAG_CLICKABLE);

	}


	return ESP_OK;
}
