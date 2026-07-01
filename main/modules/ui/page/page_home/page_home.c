#include "page_home.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_settings.h"
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
#include "modules/ws/ws_cw_cache.h"

#define HOME_PAGE_BOTTOM_BODY_HEIGHT 30
#define HOME_PAGE_MARGIN 5
#define HOME_PAGE_BTN_WIDTH 80

#define HOME_MSG_TITLE_HEIGHT 15
#define HOME_MSG_BODY_WIDTH (DISPLAY_H_RES - 10)
#define HOME_MSG_BODY_PAD 8
#define HOME_MSG_CONTEXT_PAD 5
#define HOME_MSG_BUBBLE_MAX_WIDTH (((HOME_MSG_BODY_WIDTH - HOME_MSG_BODY_PAD * 2) * 85) / 100)
#define HOME_MSG_TEXT_MAX_WIDTH (HOME_MSG_BUBBLE_MAX_WIDTH - HOME_MSG_CONTEXT_PAD * 2)
#define HOME_MSG_SCROLL_STEP 24
#define HOME_FOCUS_BORDER_WIDTH 2
// #define HOME_FOCUS_BORDER_COLOR lv_color_hex(0x06B6D4)
// #define HOME_ACTIVE_BORDER_COLOR lv_color_hex(0xEF4444)

#define HOME_FOCUS_BORDER_COLOR UI_COLOR_ACCENT
#define HOME_ACTIVE_BORDER_COLOR UI_COLOR_ERROR

static lv_obj_t *home_body;
static lv_obj_t *msg_body;
static lv_obj_t *context_body;
static lv_obj_t *context_input_body;
static lv_obj_t *btn_send;
static lv_obj_t *label_cw_input;
static char *s_cw_display_text;
static size_t s_cw_display_len;
static size_t s_cw_display_cap;
static bool s_cw_has_content;

static lv_style_t s_style_msg_body;

static lv_style_t s_style_m_body_left;
static lv_style_t s_style_m_item_body;
static lv_style_t s_style_m_body_right;
static lv_style_t s_style_m_body;

static lv_style_t s_style_m_title_body;
static lv_style_t s_style_m_title_callsign_label;
static lv_style_t s_style_m_title_time_label;

static lv_style_t s_style_m_context_body;
static lv_style_t s_style_m_context_label;

static lv_style_t s_style_line_1;

typedef struct {
	const char *callsign;
	const char *code;
	const char *from;
	const char *time;
	char rendered_code[320];
} home_msg_view_t;

typedef enum {
	HOME_FOCUS_CHAT = 0,
	HOME_FOCUS_INPUT,
	HOME_FOCUS_SEND,
	HOME_FOCUS_COUNT,
} home_focus_t;

typedef enum {
	HOME_ACTIVE_NONE = 0,
	HOME_ACTIVE_CHAT_SCROLL,
	HOME_ACTIVE_INPUT_EDIT,
} home_active_t;

static home_focus_t s_home_focus = HOME_FOCUS_SEND;
static home_active_t s_home_active = HOME_ACTIVE_NONE;

static bool style_init_done = false;

static void style_init(void) {
	if(style_init_done) {
		return;
	}


	lv_style_init(&s_style_msg_body);
	lv_style_set_layout(&s_style_msg_body, LV_LAYOUT_FLEX);
	lv_style_set_flex_flow(&s_style_msg_body, LV_FLEX_FLOW_COLUMN);
	lv_style_set_size(
		&s_style_msg_body,
		HOME_MSG_BODY_WIDTH,
		DISPLAY_V_RES - CONFIG_UI_TOP_STATUS_HEIGHT - HOME_PAGE_BOTTOM_BODY_HEIGHT - HOME_PAGE_MARGIN * 3
	);
	lv_style_set_radius(&s_style_msg_body, CONFIG_UI_RADIUS);
	lv_style_set_bg_color(&s_style_msg_body, UI_COLOR_TEXT);
	lv_style_set_border_width(&s_style_msg_body, HOME_FOCUS_BORDER_WIDTH);
	lv_style_set_border_color(&s_style_msg_body, UI_COLOR_TEXT);
	lv_style_set_pad_all(&s_style_msg_body, 8);
	lv_style_set_margin_all(&s_style_msg_body, 0);
	lv_style_set_pad_row(&s_style_msg_body, 8);
	lv_style_set_flex_main_place(&s_style_msg_body, LV_FLEX_ALIGN_START);

	// 消息外框
	lv_style_init(&s_style_m_item_body);
	lv_style_set_layout(&s_style_m_item_body, LV_LAYOUT_FLEX);
	lv_style_set_size(&s_style_m_item_body, LV_PCT(100), LV_SIZE_CONTENT);
	lv_style_set_flex_flow(&s_style_m_item_body, LV_FLEX_FLOW_ROW);
	lv_style_set_bg_opa(&s_style_m_item_body, LV_OPA_0);
	lv_style_set_margin_all(&s_style_m_item_body, 0);
	lv_style_set_pad_all(&s_style_m_item_body, 0);
	lv_style_set_border_width(&s_style_m_item_body, 0);

	// 消息体
	ui_style_init_column(&s_style_m_body);
	lv_style_set_width(&s_style_m_body, LV_SIZE_CONTENT);
	lv_style_set_max_width(&s_style_m_body, HOME_MSG_BUBBLE_MAX_WIDTH);
	lv_style_set_height(&s_style_m_body, LV_SIZE_CONTENT);
	lv_style_set_radius(&s_style_m_body, CONFIG_UI_RADIUS);
	lv_style_set_border_width(&s_style_m_body, 0);
	lv_style_set_min_height(&s_style_m_body, 30);
	
	// 别人的消息
	lv_style_init(&s_style_m_body_left);
	lv_style_set_flex_cross_place(&s_style_m_body_left, LV_FLEX_ALIGN_START);
	lv_style_set_bg_color(&s_style_m_body_left, UI_COLOR_WHITE);

	// 自己的消息
	lv_style_init(&s_style_m_body_right);
	lv_style_set_flex_cross_place(&s_style_m_body_right, LV_FLEX_ALIGN_END);
	lv_style_set_bg_color(&s_style_m_body_right, lv_color_hex(0xBAE6FD));


	// 标题
	ui_style_init_row(&s_style_m_title_body);
	lv_style_set_width(&s_style_m_title_body, LV_SIZE_CONTENT);
	lv_style_set_height(&s_style_m_title_body, LV_SIZE_CONTENT);
	lv_style_set_bg_opa(&s_style_m_title_body, LV_OPA_TRANSP);
	lv_style_set_border_width(&s_style_m_title_body, 0);
	lv_style_set_pad_hor(&s_style_m_title_body, 5);
	lv_style_set_pad_column(&s_style_m_title_body, 5);
	lv_style_set_margin_top(&s_style_m_title_body, 5);
	
	// 标题呼号文字样式
	lv_style_init(&s_style_m_title_callsign_label);
	lv_style_set_text_font(&s_style_m_title_callsign_label, UI_FONT_12);
	lv_style_set_bg_opa(&s_style_m_title_callsign_label, LV_OPA_TRANSP);
	lv_style_set_text_color(&s_style_m_title_callsign_label, UI_COLOR_TEXT_DARK);

	// 标题时间文字样式
	lv_style_init(&s_style_m_title_time_label);
	lv_style_set_text_font(&s_style_m_title_time_label, UI_FONT_12);
	lv_style_set_text_color(&s_style_m_title_time_label, UI_COLOR_TEXT_MUTED);
	

	// 分割线
	lv_style_init(&s_style_line_1);
	lv_style_set_size(&s_style_line_1, LV_PCT(100), 1);
	lv_style_set_border_width(&s_style_line_1, 0);
	lv_style_set_pad_all(&s_style_line_1, 0);
	lv_style_set_margin_all(&s_style_line_1, 2);
	lv_style_set_bg_color(&s_style_line_1, UI_COLOR_FOCUS_1);


	// 内容外框
	lv_style_init(&s_style_m_context_body);
	lv_style_set_width(&s_style_m_context_body, LV_SIZE_CONTENT);
	lv_style_set_max_width(&s_style_m_context_body, HOME_MSG_BUBBLE_MAX_WIDTH);
	lv_style_set_height(&s_style_m_context_body, LV_SIZE_CONTENT);
	lv_style_set_pad_all(&s_style_m_context_body, HOME_MSG_CONTEXT_PAD);
	lv_style_set_bg_opa(&s_style_m_context_body, LV_OPA_TRANSP);
	lv_style_set_border_width(&s_style_m_context_body, 0);
	lv_style_set_margin_all(&s_style_m_context_body, 0);

	

	// 消息内容
	lv_style_init(&s_style_m_context_label);
	lv_style_set_width(&s_style_m_context_label, LV_SIZE_CONTENT);
	lv_style_set_max_width(&s_style_m_context_label, HOME_MSG_TEXT_MAX_WIDTH);
	lv_style_set_height(&s_style_m_context_label, LV_SIZE_CONTENT);
	lv_style_set_border_width(&s_style_m_context_label, 0);
	lv_style_set_text_font(&s_style_m_context_label, UI_FONT_12);
	lv_style_set_text_color(&s_style_m_context_label, UI_COLOR_TEXT_DARK);


	style_init_done = true;
}

static void message_label_set_text(lv_obj_t *label, const char *text)
{
	if(label == NULL) {
		return;
	}

	const char *safe_text = text != NULL ? text : "";
	lv_point_t text_size = {0};
	lv_text_get_size(
		&text_size,
		safe_text,
		UI_FONT_12,
		0,
		0,
		LV_COORD_MAX,
		LV_TEXT_FLAG_NONE
	);

	int32_t width = text_size.x;
	if(width <= 0) {
		width = 1;
	}
	if(width > HOME_MSG_TEXT_MAX_WIDTH) {
		width = HOME_MSG_TEXT_MAX_WIDTH;
	}

	lv_obj_set_width(label, width);
	lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
	lv_label_set_text(label, safe_text);
}

static bool home_msg_is_self(const home_msg_view_t *msg)
{
	return msg != NULL && msg->from != NULL && strcmp(msg->from, "self") == 0;
}

static const char *home_msg_callsign_or_default(const home_msg_view_t *msg, bool is_self)
{
	const char *callsign = msg != NULL ? msg->callsign : NULL;
	if(callsign != NULL && callsign[0] != '\0') {
		return callsign;
	}

	if(is_self) {
		if(app_settings.user_callsign[0] != '\0') {
			return app_settings.user_callsign;
		}
		return USER_DEFAULT_CALLSIGN;
	}

	return "未知用户";
}

static lv_obj_t *message_title_label_create(lv_obj_t *parent, lv_style_t *style, const char *text, lv_text_align_t align)
{
	lv_obj_t *label = lv_label_create(parent);
	lv_obj_add_style(label, style, LV_STATE_DEFAULT);
	lv_label_set_text(label, text != NULL ? text : "");
	lv_obj_set_style_text_align(label, align, LV_STATE_DEFAULT);
	return label;
}

static void message_title_create(lv_obj_t *bubble, const home_msg_view_t *msg, bool is_self)
{
	const char *callsign = home_msg_callsign_or_default(msg, is_self);
	const char *time = msg != NULL && msg->time != NULL ? msg->time : "";

	lv_obj_t *title_body = lv_obj_create(bubble);
	lv_obj_add_style(title_body, &s_style_m_title_body, LV_STATE_DEFAULT);
	lv_obj_set_style_flex_main_place(
		title_body,
		is_self ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
		LV_STATE_DEFAULT
	);
	lv_obj_set_style_flex_cross_place(title_body, LV_FLEX_ALIGN_CENTER, LV_STATE_DEFAULT);

	if(is_self) {
		(void)message_title_label_create(title_body, &s_style_m_title_time_label, time, LV_TEXT_ALIGN_RIGHT);
		(void)message_title_label_create(title_body, &s_style_m_title_callsign_label, callsign, LV_TEXT_ALIGN_RIGHT);
	} else {
		(void)message_title_label_create(title_body, &s_style_m_title_callsign_label, callsign, LV_TEXT_ALIGN_LEFT);
		(void)message_title_label_create(title_body, &s_style_m_title_time_label, time, LV_TEXT_ALIGN_LEFT);
	}
}

static lv_obj_t *add_msg(const home_msg_view_t *msg)
{
	if(msg == NULL || msg->code == NULL || msg->code[0] == '\0') {
		return NULL;
	}

	bool is_self = home_msg_is_self(msg);

	lv_obj_t *msg_item = lv_obj_create(msg_body);
	lv_obj_add_style(msg_item, &s_style_m_item_body, LV_STATE_DEFAULT);
	lv_obj_set_style_flex_main_place(
		msg_item,
		is_self ? LV_FLEX_ALIGN_END : LV_FLEX_ALIGN_START,
		LV_STATE_DEFAULT
	);

	lv_obj_t *bubble = lv_obj_create(msg_item);
	lv_obj_add_style(bubble, &s_style_m_body, LV_STATE_DEFAULT);
	lv_obj_add_style(
		bubble,
		is_self ? &s_style_m_body_right : &s_style_m_body_left,
		LV_STATE_DEFAULT
	);

	message_title_create(bubble, msg, is_self);

	lv_obj_t *line_1 = lv_obj_create(bubble);
	lv_obj_add_style(line_1, &s_style_line_1, LV_STATE_DEFAULT);

	lv_obj_t *context_body = lv_obj_create(bubble);
	lv_obj_add_style(context_body, &s_style_m_context_body, LV_STATE_DEFAULT);

	lv_obj_t *context_label = lv_label_create(context_body);
	lv_obj_add_style(context_label, &s_style_m_context_label, LV_STATE_DEFAULT);
	const char *display_code = msg->rendered_code[0] != '\0' ? msg->rendered_code : msg->code;
	message_label_set_text(context_label, display_code);

	return msg_item;
}

static void page_home_scroll_msg_bottom(void)
{
	if(msg_body == NULL) {
		return;
	}

	lv_obj_update_layout(msg_body);
	int32_t y = lv_obj_get_scroll_y(msg_body);
	int32_t max_y = y + lv_obj_get_scroll_bottom(msg_body);
	if(max_y < 0) {
		max_y = 0;
	}
	lv_obj_scroll_to_y(msg_body, max_y, LV_ANIM_OFF);
	lv_obj_update_layout(msg_body);
	y = lv_obj_get_scroll_y(msg_body);
	max_y = y + lv_obj_get_scroll_bottom(msg_body);
	if(max_y < 0) {
		max_y = 0;
	}
	lv_obj_scroll_to_y(msg_body, max_y, LV_ANIM_OFF);
}

static void page_home_scroll_msg_to_item(lv_obj_t *item)
{
	if(msg_body == NULL || item == NULL) {
		return;
	}

	lv_obj_update_layout(msg_body);
	lv_obj_update_layout(item);
	page_home_scroll_msg_bottom();
}

static void page_home_scroll_msg_step(bool down)
{
	if(msg_body == NULL) {
		return;
	}

	lv_obj_update_layout(msg_body);
	int32_t y = lv_obj_get_scroll_y(msg_body);
	int32_t max_y = y + lv_obj_get_scroll_bottom(msg_body);
	int32_t next_y = down ? y + HOME_MSG_SCROLL_STEP : y - HOME_MSG_SCROLL_STEP;

	if(next_y < 0) {
		next_y = 0;
	}
	if(next_y > max_y) {
		next_y = max_y;
	}

	lv_obj_scroll_to_y(msg_body, next_y, LV_ANIM_OFF);
}

static void page_home_add_record(const ws_cw_record_t *record, bool scroll_to_bottom)
{
	if(record == NULL) {
		return;
	}

	home_msg_view_t view = {
		.callsign = record->callsign,
		.code = record->code,
		.from = record->from,
		.time = record->time,
	};
	(void)cw_keyer_actor_render_display_text(record->code, view.rendered_code, sizeof(view.rendered_code));
	lv_obj_t *item = add_msg(&view);

	if(scroll_to_bottom) {
		page_home_scroll_msg_to_item(item);
	}
}

static void page_home_replay_msg_history(void)
{
	size_t count = ws_cw_cache_count();
	for(size_t i = 0; i < count; i++) {
		ws_cw_record_t record = {0};
		if(ws_cw_cache_get(i, &record)) {
			page_home_add_record(&record, false);
		}
	}

	page_home_scroll_msg_bottom();
}

static void page_home_update_send_button(void)
{
	if(btn_send == NULL) {
		return;
	}

	lv_color_t bg_color = s_cw_has_content ? UI_COLOR_ACCENT : UI_COLOR_BUTTON;
	lv_obj_set_style_bg_color(
		btn_send,
		bg_color,
		0
	);
	if(s_home_focus != HOME_FOCUS_SEND) {
		lv_obj_set_style_border_color(btn_send, bg_color, LV_STATE_DEFAULT);
	}
}

static void page_home_clear_delete_history(void)
{
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_CW_CLEAR_DELETE_HISTORY, 1, 0);
}

static void page_home_set_active(home_active_t active)
{
	if(s_home_active == active) {
		return;
	}

	if(s_home_active == HOME_ACTIVE_INPUT_EDIT || active == HOME_ACTIVE_INPUT_EDIT) {
		page_home_clear_delete_history();
	}

	s_home_active = active;
}

static void page_home_set_border_state(lv_obj_t *obj,
									   bool focused,
									   bool active,
									   lv_color_t normal_color,
									   lv_color_t focus_color,
									   lv_color_t active_color)
{
	if(obj == NULL) {
		return;
	}

	lv_obj_set_style_border_width(obj, HOME_FOCUS_BORDER_WIDTH, LV_STATE_DEFAULT);
	lv_obj_set_style_border_color(
		obj,
		active ? active_color : (focused ? focus_color : normal_color),
		LV_STATE_DEFAULT
	);
}

static void page_home_apply_focus(void)
{
	page_home_set_border_state(
		msg_body,
		s_home_focus == HOME_FOCUS_CHAT,
		s_home_active == HOME_ACTIVE_CHAT_SCROLL,
		UI_COLOR_TEXT,
		HOME_FOCUS_BORDER_COLOR,
		HOME_ACTIVE_BORDER_COLOR
	);
	page_home_set_border_state(
		context_input_body,
		s_home_focus == HOME_FOCUS_INPUT,
		s_home_active == HOME_ACTIVE_INPUT_EDIT,
		UI_COLOR_WHITE,
		HOME_FOCUS_BORDER_COLOR,
		HOME_ACTIVE_BORDER_COLOR
	);
	lv_color_t send_bg_color = s_cw_has_content ? UI_COLOR_ACCENT : UI_COLOR_BUTTON;
	page_home_set_border_state(
		btn_send,
		s_home_focus == HOME_FOCUS_SEND,
		false,
		send_bg_color,
		HOME_FOCUS_BORDER_COLOR,
		HOME_FOCUS_BORDER_COLOR
	);
}

static void page_home_focus_step(int delta)
{
	page_home_set_active(HOME_ACTIVE_NONE);

	int next = (int)s_home_focus + delta;
	if(next < 0) {
		next = HOME_FOCUS_COUNT - 1;
	} else if(next >= HOME_FOCUS_COUNT) {
		next = 0;
	}

	s_home_focus = (home_focus_t)next;
	page_home_apply_focus();
}

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

static void page_home_send_button_event_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	if(code == LV_EVENT_SINGLE_CLICKED) {
		(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_CW_SEND, 1, 0);
	}
}

static void page_home_send_cw(void)
{
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_CW_SEND, 1, 0);
}

static void page_home_delete_last_cw_group(void)
{
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_CW_DELETE_LAST_GROUP, 1, 0);
}

static void page_home_restore_last_cw_group(void)
{
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_CW_RESTORE_LAST_GROUP, 1, 0);
}

static void page_home_handle_encoder_rotate(bool cw)
{
	if(s_home_active == HOME_ACTIVE_CHAT_SCROLL) {
		page_home_scroll_msg_step(cw);
		return;
	}

	if(s_home_active == HOME_ACTIVE_INPUT_EDIT) {
		if(cw) {
			page_home_delete_last_cw_group();
		} else {
			page_home_restore_last_cw_group();
		}
		return;
	}

	page_home_focus_step(cw ? 1 : -1);
}

static void page_home_handle_encoder_press(void)
{
	switch(s_home_focus) {
		case HOME_FOCUS_CHAT:
			page_home_set_active(
				s_home_active == HOME_ACTIVE_CHAT_SCROLL ?
				HOME_ACTIVE_NONE : HOME_ACTIVE_CHAT_SCROLL
			);
			page_home_apply_focus();
			break;
		case HOME_FOCUS_INPUT:
			page_home_set_active(
				s_home_active == HOME_ACTIVE_INPUT_EDIT ?
				HOME_ACTIVE_NONE : HOME_ACTIVE_INPUT_EDIT
			);
			page_home_apply_focus();
			break;
		case HOME_FOCUS_SEND:
			page_home_send_cw();
			break;
		default:
			break;
	}
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
            .page_id = PAGE_MENU,
        });
		return;
    }

	if(msg->event == MSG_EVT_INPUT_ENCODER_CW) {
		page_home_handle_encoder_rotate(true);
		return;
	}

	if(msg->event == MSG_EVT_INPUT_ENCODER_CCW) {
		page_home_handle_encoder_rotate(false);
		return;
	}

    if(msg->event == MSG_EVT_INPUT_ENCODER_PRESS) {
		page_home_handle_encoder_press();
		return;
    }

    if(msg->event == MSG_EVT_INPUT_CW_DISPLAY_SYMBOL) {
		page_home_append_cw_symbol(msg->data.text);
		return;
    }

    if(msg->event == MSG_EVT_INPUT_CW_CLEARED) {
		page_home_set_cw_display_text("");
		return;
    }

    if(msg->event == MSG_EVT_INPUT_CW_TEXT_CHANGED) {
		page_home_set_cw_display_text(cw_keyer_actor_get_display_text());
		return;
    }

    if(msg->event == MSG_EVT_INPUT_CW_CONTENT_STATE) {
		s_cw_has_content = msg->data.value != 0;
		page_home_update_send_button();
    }
}

static void page_home_msg_handler(const msg_t *msg)
{
	if(msg == NULL || msg->type != MSG_TYPE_SYS) {
		return;
	}

	if(msg->event == MSG_EVT_SYS_WS_CW_RECEIVED) {
		ws_cw_record_t record = {0};
		if(ws_cw_cache_get_by_seq((uint32_t)msg->data.value, &record)) {
			page_home_add_record(&record, true);
		}
	}
}

static const ui_page_ops_t page_home_ops = {
    .on_input = page_home_input_handler,
	.on_msg = page_home_msg_handler,
};


esp_err_t page_home_show(lv_obj_t *p) {
	if(p == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	style_init();

	home_body = lv_obj_create(p);

	lv_obj_add_style(home_body, &style_page_body, 0);
	lv_obj_set_style_pad_row(home_body, HOME_PAGE_MARGIN, 0);
	// lv_obj_clear_flag(home_body, LV_OBJ_FLAG_SCROLLABLE);

	ui_add_top_status(home_body);

	msg_body = lv_obj_create(home_body);
	lv_obj_add_style(msg_body, &s_style_msg_body, LV_STATE_DEFAULT);
	

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
	lv_obj_set_size(
		context_input_body,
		DISPLAY_H_RES - HOME_PAGE_BTN_WIDTH - 10 - HOME_PAGE_MARGIN,
		HOME_PAGE_BOTTOM_BODY_HEIGHT
	);
	lv_obj_set_style_radius(context_input_body, 5, 0);
	lv_obj_set_style_bg_color(context_input_body, UI_COLOR_WHITE, 0);
	lv_obj_set_style_border_width(context_input_body, HOME_FOCUS_BORDER_WIDTH, 0);
	lv_obj_set_style_border_color(context_input_body, UI_COLOR_WHITE, 0);
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
	lv_obj_set_style_size(
		btn_send,
		HOME_PAGE_BTN_WIDTH,
		HOME_PAGE_BOTTOM_BODY_HEIGHT,
		0
	);
	lv_obj_set_style_bg_color(btn_send, UI_COLOR_BUTTON, 0);
	lv_obj_set_style_radius(btn_send, 5, 0);
	lv_obj_set_style_border_width(btn_send, HOME_FOCUS_BORDER_WIDTH, 0);
	lv_obj_set_style_border_color(btn_send, UI_COLOR_BUTTON, 0);
	lv_obj_remove_flag(btn_send, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(btn_send, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_pad_all(btn_send, 0, 0);
	lv_obj_set_style_margin_all(btn_send, 0, 0);
	lv_obj_add_event_cb(btn_send, page_home_send_button_event_cb, LV_EVENT_SINGLE_CLICKED, NULL);

	lv_obj_t *label_send = lv_label_create(btn_send);
	lv_obj_set_style_text_font(label_send, UI_FONT_14, 0);
	lv_obj_set_style_text_color(label_send, UI_COLOR_TEXT, 0);
	lv_label_set_text(label_send, "发送");
	lv_obj_center(label_send);
	s_cw_has_content = cw_keyer_actor_get_raw_text()[0] != '\0';
	page_home_update_send_button();
	page_home_apply_focus();


	// lv_label_set_text(btn_send, "发送");
	// lv_obj_set_style_text_color(btn_send, UI_COLOR_TEXT, 0);
	// lv_arclabel_set_text_vertical_align(btn_send, LV_TEXT_ALIGN_CENTER);
	// lv_arclabel_set_text_horizontal_align(btn_send, LV_TEXT_ALIGN_CENTER);


	ui_actor_set_ops(&page_home_ops);

	page_home_replay_msg_history();

	return ESP_OK;
}
