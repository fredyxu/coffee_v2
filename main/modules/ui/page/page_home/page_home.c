#include "page_home.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ui/theme/color.h"
#include "ui/theme/font.h"
#include "ui/page/components/components_top_status/component_top_status.h"

#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/ui/ui.h"
#include "config/config_ui.h"
#include "modules/ui/style/ui_style.h"
#include "core/msg/msg.h"
#include "modules/key/cw_keyer_actor.h"
#include "modules/ui/ui_actor.h"

#define HOME_PAGE_BOTTOM_BODY_HEIGHT 30
#define HOME_PAGE_MARGIN 5
#define HOME_PAGE_BTN_WIDTH 80

static lv_obj_t *home_body;
static lv_obj_t *msg_body;
static lv_obj_t *context_body;
static lv_obj_t *context_input_body;
static lv_obj_t *btn_send;
static lv_obj_t *label_cw_input;
static char *s_cw_display_text;
static size_t s_cw_display_len;
static size_t s_cw_display_cap;

static bool page_home_display_reserve(size_t needed)
{
	if(needed <= s_cw_display_cap) {
		return true;
	}

	size_t next_cap = s_cw_display_cap > 0 ? s_cw_display_cap : 128;
	while(next_cap < needed) {
		next_cap *= 2;
	}

	char *next = (char *)realloc(s_cw_display_text, next_cap);
	if(next == NULL) {
		return false;
	}

	s_cw_display_text = next;
	s_cw_display_cap = next_cap;
	return true;
}

static void page_home_refresh_cw_display(void)
{
	if(label_cw_input == NULL) {
		return;
	}

	lv_label_set_text(label_cw_input, s_cw_display_text != NULL ? s_cw_display_text : "");
	lv_obj_update_layout(label_cw_input);
	lv_obj_align(label_cw_input, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void page_home_append_cw_symbol(const char *symbol)
{
	if(symbol == NULL || symbol[0] == '\0') {
		return;
	}

	size_t symbol_len = strlen(symbol);
	size_t needed = s_cw_display_len + symbol_len + 1;
	if(!page_home_display_reserve(needed)) {
		return;
	}

	memcpy(s_cw_display_text + s_cw_display_len, symbol, symbol_len);
	s_cw_display_len += symbol_len;
	s_cw_display_text[s_cw_display_len] = '\0';
	page_home_refresh_cw_display();
}

static void page_home_set_cw_display_text(const char *text)
{
	size_t text_len = text != NULL ? strlen(text) : 0;
	if(!page_home_display_reserve(text_len + 1)) {
		return;
	}

	if(text_len > 0) {
		memcpy(s_cw_display_text, text, text_len);
	}
	s_cw_display_len = text_len;
	s_cw_display_text[s_cw_display_len] = '\0';
	page_home_refresh_cw_display();
}

// 事件处理
static void page_home_input_handler(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_INPUT) {
        return;
    }

	// 长按编码器进入设置页面
    if(msg->event == MSG_EVT_INPUT_ENCODER_LONG_PRESS) {
        ui_nav_go((ui_page_nav_param_t) {
            .page_id = PAGE_SETTINGS,
        });
		return;
    }

    if(msg->event == MSG_EVT_INPUT_CW_DISPLAY_SYMBOL) {
		page_home_append_cw_symbol(msg->data.text);
    }
}

static const ui_page_ops_t page_home_ops = {
    .on_input = page_home_input_handler,
};


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
	lv_obj_remove_flag(context_input_body, LV_OBJ_FLAG_SCROLLABLE);

	label_cw_input = lv_label_create(context_input_body);
	lv_obj_set_style_text_font(label_cw_input, UI_FONT_14, 0);
	lv_obj_set_style_text_color(label_cw_input, UI_COLOR_TEXT_DARK, 0);
	lv_obj_set_style_text_align(label_cw_input, LV_TEXT_ALIGN_RIGHT, 0);
	lv_label_set_long_mode(label_cw_input, LV_LABEL_LONG_CLIP);
	page_home_set_cw_display_text(cw_keyer_actor_get_display_text());

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


	ui_actor_set_ops(&page_home_ops);

	return ESP_OK;
}
