#include "page_home.h"
#include "lvgl.h"

#include "ui/theme/color.h"
#include "ui/theme/font.h"
#include "ui/page/components/components_top_status/component_top_status.h"

#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/ui/ui.h"
#include "config/config_ui.h"
#include "modules/ui/style/ui_style.h"

#define HOME_PAGE_BOTTOM_BODY_HEIGHT 30
#define HOME_PAGE_MARGIN 5
#define HOME_PAGE_BTN_WIDTH 80

static lv_obj_t *home_body;
static lv_obj_t *msg_body;
static lv_obj_t *context_body;
static lv_obj_t *context_input_body;
static lv_obj_t *btn_send;


esp_err_t page_home_show(lv_obj_t *p) {
	if(p == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	home_body = lv_obj_create(p);

	lv_obj_add_style(home_body, &style_page_body, 0);
	lv_obj_set_style_pad_row(home_body, HOME_PAGE_MARGIN, 0);
	// lv_obj_clear_flag(home_body, LV_OBJ_FLAG_SCROLLABLE);

	ui_add_top_status(home_body);

	msg_body = lv_obj_create(home_body);
	lv_obj_set_style_size(msg_body,
	DISPLAY_H_RES - 10, 
	DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT - HOME_PAGE_BOTTOM_BODY_HEIGHT - HOME_PAGE_MARGIN * 3, 
	0);
	lv_obj_set_style_radius(msg_body, 5, 0);
	lv_obj_set_style_bg_color(msg_body, UI_COLOR_TEXT, 0);
	lv_obj_set_style_opa(msg_body, LV_OPA_100, 0);
	lv_obj_set_style_border_width(msg_body, 0, 0);
	lv_obj_set_style_pad_all(msg_body, 0, 0);
	lv_obj_set_style_margin_all(msg_body, 0, 0);

	context_body = lv_obj_create(home_body);
	lv_obj_set_style_size(context_body, DISPLAY_H_RES - 10, HOME_PAGE_BOTTOM_BODY_HEIGHT, 0);
	lv_obj_set_style_radius(context_body, 0, 0);
	lv_obj_set_style_bg_color(context_body, UI_COLOR_BG, 0);
	lv_obj_set_style_border_width(context_body, 0, 0);
	lv_obj_remove_flag(context_body, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(context_body, 0, 0);

	lv_obj_set_layout(context_body, LV_LAYOUT_FLEX);
	lv_obj_set_flex_flow(context_body, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(
		context_body,
		LV_FLEX_ALIGN_SPACE_BETWEEN,
		LV_FLEX_ALIGN_CENTER,
		LV_FLEX_ALIGN_CENTER
	);
	lv_obj_set_style_pad_row(context_body, HOME_PAGE_MARGIN, 0);
	// lv_obj_set_style_bg_color(context_body, UI_COLOR_ACCENT, 0);

	context_input_body = lv_obj_create(context_body);
	lv_obj_set_size(context_input_body, DISPLAY_H_RES - HOME_PAGE_BTN_WIDTH - 10 - HOME_PAGE_MARGIN, HOME_PAGE_BOTTOM_BODY_HEIGHT);
	lv_obj_set_style_radius(context_input_body, 5, 0);
	lv_obj_set_style_border_width(context_input_body, 0, 0);
	lv_obj_set_style_pad_all(context_input_body, 0, 0);
	lv_obj_set_style_margin_all(context_input_body, 0, 0);

	btn_send = lv_obj_create(context_body);
	lv_obj_set_style_size(btn_send, HOME_PAGE_BTN_WIDTH, HOME_PAGE_BOTTOM_BODY_HEIGHT, 0);
	lv_obj_set_style_bg_color(btn_send, UI_COLOR_BUTTON, 0);
	lv_obj_set_style_radius(btn_send, 5, 0);
	lv_obj_set_style_border_width(btn_send, 0, 0);
	lv_obj_remove_flag(btn_send, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_pad_all(btn_send, 0, 0);
	lv_obj_set_style_margin_all(btn_send, 0, 0);

	lv_obj_t *label_send = lv_label_create(btn_send);
	lv_obj_set_style_text_font(label_send, UI_FONT_14, 0);
	lv_obj_set_style_text_color(label_send, UI_COLOR_TEXT, 0);
	lv_label_set_text(label_send, "发送");
	lv_obj_center(label_send);


	// lv_label_set_text(btn_send, "发送");
	// lv_obj_set_style_text_color(btn_send, UI_COLOR_TEXT, 0);
	// lv_arclabel_set_text_vertical_align(btn_send, LV_TEXT_ALIGN_CENTER);
	// lv_arclabel_set_text_horizontal_align(btn_send, LV_TEXT_ALIGN_CENTER);

	return ESP_OK;
}
