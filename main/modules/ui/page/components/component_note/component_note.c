#include "component_note.h"

#include "lvgl.h"
#include "modules/ui/style/ui_style.h"
#include "config/config_ui.h"
#include "modules/ui/theme/color.h"
#include "config/config_sys_display.h"
#include "modules/ui/theme/font.h"

// #define UI_NOTE_WIDTH LV_PCT(80)
#define UI_NOTE_WIDTH (DISPLAY_H_RES - 60)
#define UI_NOTE_HEIGHT (DISPLAY_V_RES - 60)
#define UI_NOTE_TITLE_HEIGHT 35
#define UI_NOTE_BTN_BODY_HEIGHT 55
#define UI_NOTE_BTN_HEIGHT 30
#define UI_NOTE_BTN_WIDTH ((UI_NOTE_WIDTH / 2) - 40)
#define UI_NOTE_MSG_BODY_HEIGHT (UI_NOTE_HEIGHT - UI_NOTE_TITLE_HEIGHT - UI_NOTE_BTN_BODY_HEIGHT)

static bool s_style_init_done = false;

static lv_obj_t *s_obj_bg;
// static lv_obj_t *s_obj_body;
// static lv_obj_t *s_obj_title_body;
// static lv_obj_t *s_obj_title_label;
// static lv_obj_t *s_obj_msg_body;
// static lv_obj_t *s_obj_msg_label;
// static lv_obj_t *s_obj_btn_body;

// static lv_obj_t *s_obj_btn_ok_body;
// static lv_obj_t *s_obj_btn_cxl_body;
// static lv_obj_t *s_obj_btn_ok_label;
// static lv_obj_t *s_obj_btn_cxl_label;

static lv_style_t s_style_bg;
static lv_style_t s_style_body;
static lv_style_t s_style_title_body;
static lv_style_t s_style_msg_body;
static lv_style_t s_style_msg_label;
static lv_style_t s_style_btn_body;
static lv_style_t s_style_btn;
static lv_style_t s_style_btn_ok_body;
static lv_style_t s_style_btn_cxl_body;

static lv_style_t s_style_btn_focus;
static ui_note_ctx_t s_note_ctx;
static lv_obj_t *s_obj_btn_ok_body;
static lv_obj_t *s_obj_btn_cxl_body;
static bool s_note_visible;
static bool s_note_focus_cancel;

static void note_update_focus(void)
{
	if(s_obj_btn_ok_body != NULL) {
		if(!s_note_focus_cancel) {
			lv_obj_add_state(s_obj_btn_ok_body, LV_STATE_FOCUSED);
		} else {
			lv_obj_remove_state(s_obj_btn_ok_body, LV_STATE_FOCUSED);
		}
	}

	if(s_obj_btn_cxl_body != NULL) {
		if(s_note_focus_cancel) {
			lv_obj_add_state(s_obj_btn_cxl_body, LV_STATE_FOCUSED);
		} else {
			lv_obj_remove_state(s_obj_btn_cxl_body, LV_STATE_FOCUSED);
		}
	}
}

static void note_confirm(void)
{
	if(s_note_ctx.on_confirm != NULL) {
		s_note_ctx.on_confirm(s_note_ctx.user_data);
	}

	ui_note_hide();
}

static void note_cancel(void)
{
	if(s_note_ctx.on_cancel != NULL) {
		s_note_ctx.on_cancel(s_note_ctx.user_data);
	}

	ui_note_hide();
}

static void note_confirm_event_cb(lv_event_t *e) {
	(void)e;

	note_confirm();
}

static void note_cancel_event_cb(lv_event_t *e) {
	(void)e;

	note_cancel();
}


static void style_init(void) {
	if(s_style_init_done) {
		return;
	}

	// 背景遮罩
	lv_style_init(&s_style_bg);
	lv_style_set_size(&s_style_bg, LV_PCT(100), LV_PCT(100));
	lv_style_set_bg_color(&s_style_bg, UI_COLOR_PANEL);
	lv_style_set_bg_opa(&s_style_bg, LV_OPA_50);
	lv_style_set_border_width(&s_style_bg, 0);
	lv_style_set_pad_all(&s_style_bg, 0);
	lv_style_set_radius(&s_style_bg, 0);

	// 主体样式
	ui_style_init_column(&s_style_body);
	lv_style_set_size(&s_style_body, UI_NOTE_WIDTH, UI_NOTE_HEIGHT);
	// lv_style_set_size(&s_style_body, LV_PCT(80), LV_PCT(80));
	lv_style_set_radius(&s_style_body, CONFIG_UI_RADIUS);
	lv_style_set_clip_corner(&s_style_body, true);
	lv_style_set_bg_opa(&s_style_body, LV_OPA_100);
	lv_style_set_bg_color(&s_style_body, UI_COLOR_PANEL_2);
	lv_style_set_pad_all(&s_style_body, 0);
	lv_style_set_margin_all(&s_style_body, 0);
	lv_style_set_border_width(&s_style_body, 0);

	// 标题外框样式
	ui_style_init_row(&s_style_title_body);
	lv_style_set_size(&s_style_title_body, LV_PCT(100), UI_NOTE_TITLE_HEIGHT);
	lv_style_set_bg_color(&s_style_title_body, UI_COLOR_WINDOW_TITLE);
	lv_style_set_border_width(&s_style_title_body, 0);
	lv_style_set_text_color(&s_style_title_body, UI_COLOR_TEXT);
	lv_style_set_text_font(&s_style_title_body, UI_FONT_16);
	lv_style_set_flex_main_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_title_body, LV_FLEX_ALIGN_CENTER);

	// 消息主体样式
	ui_style_init_column(&s_style_msg_body);
	lv_style_set_size(&s_style_msg_body, LV_PCT(100), UI_NOTE_MSG_BODY_HEIGHT);
	lv_style_set_border_width(&s_style_msg_body, 0);
	lv_style_set_bg_color(&s_style_msg_body, UI_COLOR_PANEL_2);

	// 消息文字
	lv_style_init(&s_style_msg_label);
	lv_style_set_width(&s_style_msg_label, UI_NOTE_WIDTH - 20);
	lv_style_set_text_font(&s_style_msg_label, UI_FONT_12);
	lv_style_set_text_color(&s_style_msg_label, UI_COLOR_TEXT);
	lv_style_set_text_align(&s_style_msg_label, LV_TEXT_ALIGN_CENTER);

	// 按钮区域
	ui_style_init_row(&s_style_btn_body);
	lv_style_set_size(&s_style_btn_body, LV_PCT(100), UI_NOTE_BTN_BODY_HEIGHT);
	lv_style_set_flex_main_place(&s_style_btn_body, LV_FLEX_ALIGN_SPACE_AROUND);
	lv_style_set_bg_color(&s_style_btn_body, UI_COLOR_PANEL_2);
	lv_style_set_border_side(&s_style_btn_body, LV_BORDER_SIDE_TOP);
	lv_style_set_border_width(&s_style_btn_body, 1);
	lv_style_set_border_color(&s_style_btn_body, UI_COLOR_BORDER);

	// 按钮样式
	ui_style_init_row(&s_style_btn);
	lv_style_set_size(&s_style_btn, UI_NOTE_BTN_WIDTH, UI_NOTE_BTN_HEIGHT);
	lv_style_set_radius(&s_style_btn, CONFIG_UI_RADIUS);
	lv_style_set_border_width(&s_style_btn, 1);
	lv_style_set_flex_main_place(&s_style_btn, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&s_style_btn, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_btn, LV_FLEX_ALIGN_CENTER);
	lv_style_set_text_font(&s_style_btn, UI_FONT_14);
	lv_style_set_text_color(&s_style_btn, UI_COLOR_TEXT);

	// 确定按钮
	lv_style_init(&s_style_btn_ok_body);
	lv_style_set_bg_opa(&s_style_btn_ok_body, LV_OPA_COVER);
	lv_style_set_bg_color(&s_style_btn_ok_body, UI_COLOR_WINDOW_TITLE);
	lv_style_set_border_color(&s_style_btn_ok_body, UI_COLOR_WINDOW_TITLE);

	// 取消按钮
	lv_style_init(&s_style_btn_cxl_body);
	lv_style_set_bg_opa(&s_style_btn_cxl_body, LV_OPA_COVER);
	lv_style_set_bg_color(&s_style_btn_cxl_body, UI_COLOR_ERROR);
	lv_style_set_border_color(&s_style_btn_cxl_body, UI_COLOR_ERROR);

	lv_style_init(&s_style_btn_focus);
	lv_style_set_border_color(&s_style_btn_focus, UI_COLOR_TEXT);


	s_style_init_done = true;
}

void ui_note_show(ui_note_ctx_t *ctx) {
	if(ctx == NULL) {
		return;
	}

	ui_note_hide();
	style_init();
	s_note_ctx = *ctx;
	s_note_visible = true;
	s_note_focus_cancel = s_note_ctx.type == ASK;
	s_obj_btn_ok_body = NULL;
	s_obj_btn_cxl_body = NULL;
	// 背景
	s_obj_bg = lv_obj_create(lv_layer_top());
	lv_obj_add_style(s_obj_bg, &s_style_bg, LV_STATE_DEFAULT);
	// 主体
	lv_obj_t *s_obj_body = lv_obj_create(s_obj_bg);
	lv_obj_add_style(s_obj_body, &s_style_body, LV_STATE_DEFAULT);
	lv_obj_center(s_obj_body);

	// 标题外框
	lv_obj_t *s_obj_title_body = lv_obj_create(s_obj_body);
	lv_obj_add_style(s_obj_title_body, &s_style_title_body, LV_STATE_DEFAULT);
	// 标题文字
	lv_obj_t *s_obj_title_label = lv_label_create(s_obj_title_body);
	if(s_note_ctx.type == NOTE) {
		lv_label_set_text(s_obj_title_label, "提示");
	} else if(s_note_ctx.type == MSG) {
		lv_label_set_text(s_obj_title_label, "消息");
	} else if(s_note_ctx.type == ASK) {
		lv_label_set_text(s_obj_title_label, "确认");
	} else if(s_note_ctx.type == ERR) {
		lv_label_set_text(s_obj_title_label, "错误");
	} else {
		lv_label_set_text(s_obj_title_label, "提示");
	}

	// 内容外框
	lv_obj_t *s_obj_msg_body = lv_obj_create(s_obj_body);
	lv_obj_add_style(s_obj_msg_body, &s_style_msg_body, LV_STATE_DEFAULT);
	// 内容文字
	lv_obj_t *s_obj_msg_label = lv_label_create(s_obj_msg_body);
	lv_obj_add_style(s_obj_msg_label, &s_style_msg_label, LV_STATE_DEFAULT);
	lv_label_set_long_mode(s_obj_msg_label, LV_LABEL_LONG_WRAP);
	
	lv_label_set_text(s_obj_msg_label, s_note_ctx.message ? s_note_ctx.message : "");

	// 按钮区域
	lv_obj_t *s_obj_btn_body = lv_obj_create(s_obj_body);
	lv_obj_add_style(s_obj_btn_body, &s_style_btn_body, LV_STATE_DEFAULT);


	// 确定按钮
	s_obj_btn_ok_body = lv_obj_create(s_obj_btn_body);
	lv_obj_add_style(s_obj_btn_ok_body, &s_style_btn, LV_STATE_DEFAULT);
	lv_obj_add_style(s_obj_btn_ok_body, &s_style_btn_ok_body, LV_STATE_DEFAULT);
	lv_obj_add_style(s_obj_btn_ok_body, &s_style_btn_focus, LV_STATE_FOCUSED);

	// 确定文字
	lv_obj_t *s_obj_btn_ok_label = lv_label_create(s_obj_btn_ok_body);
	lv_label_set_text(s_obj_btn_ok_label, s_note_ctx.confirm_text ? s_note_ctx.confirm_text : "确定");

	lv_obj_add_event_cb(s_obj_btn_ok_body, note_confirm_event_cb, LV_EVENT_CLICKED, &s_note_ctx);


	if(s_note_ctx.type == ASK) {
		// 取消按钮
		s_obj_btn_cxl_body = lv_obj_create(s_obj_btn_body);
		lv_obj_add_style(s_obj_btn_cxl_body, &s_style_btn, LV_STATE_DEFAULT);
		lv_obj_add_style(s_obj_btn_cxl_body, &s_style_btn_cxl_body, LV_STATE_DEFAULT);
		lv_obj_add_style(s_obj_btn_cxl_body, &s_style_btn_focus, LV_STATE_FOCUSED);

		// 取消文字
		lv_obj_t *s_obj_btn_cxl_label = lv_label_create(s_obj_btn_cxl_body);
		lv_label_set_text(s_obj_btn_cxl_label, s_note_ctx.cancel_text ? s_note_ctx.cancel_text : "取消");

		lv_obj_add_event_cb(s_obj_btn_cxl_body, note_cancel_event_cb, LV_EVENT_CLICKED, &s_note_ctx);
	}

	note_update_focus();
}


void ui_note_hide(void) {
	if(s_obj_bg != NULL) {
		lv_obj_delete(s_obj_bg);
		s_obj_bg = NULL;
	}
	s_obj_btn_ok_body = NULL;
	s_obj_btn_cxl_body = NULL;
	s_note_visible = false;
	s_note_focus_cancel = false;
	// s_obj_body = NULL;
	// s_obj_title_body = NULL;
	// s_obj_title_label = NULL;
	// s_obj_msg_body = NULL;
	// s_obj_msg_label = NULL;
	// s_obj_btn_body = NULL;
	// s_obj_btn_ok_body = NULL;
	// s_obj_btn_cxl_body = NULL;
	// s_obj_btn_ok_label = NULL;
	// s_obj_btn_cxl_label = NULL;
}

bool ui_note_is_visible(void)
{
	return s_note_visible;
}

void ui_note_handle_input(const msg_t *msg)
{
	if(msg == NULL || !s_note_visible || msg->type != MSG_TYPE_INPUT) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_INPUT_ENCODER_CW:
		case MSG_EVT_INPUT_ENCODER_CCW:
			if(s_note_ctx.type == ASK) {
				s_note_focus_cancel = !s_note_focus_cancel;
				note_update_focus();
			}
			break;

		case MSG_EVT_INPUT_ENCODER_PRESS:
			if(s_note_ctx.type == ASK && s_note_focus_cancel) {
				note_cancel();
			} else {
				note_confirm();
			}
			break;

		default:
			break;
	}
}


ui_note_ctx_t ui_note_ctx_default(void) {
    return (ui_note_ctx_t) {
        .type = NOTE,
        .title = "提示",
        .message = "",
        .confirm_text = "确定",
        .cancel_text = "取消",
        .on_confirm = NULL,
        .on_cancel = NULL,
        .user_data = NULL,
    };
}
