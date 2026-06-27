#include "page_talk.h"
#include <stdint.h>
#include <string.h>

#include "app/app_settings.h"
#include "lvgl.h"
#include "esp_err.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/page/components/components_top_status/component_top_status.h"
#include "modules/ui/theme/color.h"
#include "modules/ui/ui.h"
#include "modules/ui/ui_actor.h"
#include "modules/ui/theme/font.h"
#include "modules/ws/ws_room_cache.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"


#define PAGE_TALK_ROOM_WIDTH LV_PCT(40)
#define PAGE_TALK_INFO_WIDTH LV_PCT(59)
#define PAGE_TALK_PTT_READY_COLOR lv_color_hex(0x3FA96B)
#define PAGE_TALK_LIST_SCROLL_STEP 24

typedef enum {
	PTT_STATUS_IDLE = 0,
	PTT_STATUS_CHECKING,
	PTT_STATUS_READY,
	PTT_STATUS_BUSY,
	PTT_STATUS_OFFLINE,
	PTT_STATUS_ERROR,
} ptt_status_t;

typedef enum {
	TALK_FOCUS_ROOM_LIST = 0,
	TALK_FOCUS_USER_LIST,
	TALK_FOCUS_PTT,
	TALK_FOCUS_COUNT,
} talk_focus_t;

static ptt_status_t ptt_status = PTT_STATUS_IDLE;
static bool ptt_is_pressed = false;
static talk_focus_t s_talk_focus = TALK_FOCUS_ROOM_LIST;
static bool s_talk_focus_active = false;
static int s_current_room_index = 0;
static int s_room_focus_index = 0;
static char s_current_room_id[ROOM_ID_MAX_LEN + 1] = "default";
static char s_current_room_name[ROOM_NAME_MAX_LEN + 1] = "大厅";
static bool s_room_refresh_deferred = false;
static bool s_user_refresh_deferred = false;


static bool style_init_done = false;

static lv_style_t s_style_main_body;
static lv_style_t s_style_room_list_body;
static lv_style_t s_style_info_body;

static lv_style_t s_style_title_body;
static lv_style_t s_style_title_label;

static lv_style_t s_style_list_body;
static lv_style_t s_style_focus_body;

static lv_style_t s_style_item_body;
static lv_style_t s_style_room_item_body;
static lv_style_t s_style_room_item_selected;
static lv_style_t s_style_room_item_focused;
static lv_style_t s_style_item_title_label;
static lv_style_t s_style_item_sub_title_label;

static lv_style_t s_style_ptt_body;
static lv_style_t s_style_ptt_label;


// 房间列表容器
static lv_obj_t *obj_room_list;
// 用户列表
static lv_obj_t *obj_user_list;
// PTT按钮
static lv_obj_t *obj_ptt_body;
static lv_obj_t *obj_ptt_label;
static lv_obj_t *s_room_item_objs[ROOM_LIST_MAX_COUNT];

static void ptt_set_status(ptt_status_t status);

static void style_init() {
	if(style_init_done) {
		return;
	}

	// 主体
	ui_style_init_row(&s_style_main_body);
	lv_style_set_flex_main_place(&s_style_main_body, LV_FLEX_ALIGN_SPACE_BETWEEN);
	lv_style_set_flex_cross_place(&s_style_main_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_main_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_size(&s_style_main_body, LV_PCT(100), PAGE_MAIN_HEIGHT);
	lv_style_set_bg_color(&s_style_main_body, UI_COLOR_BG);



	// 房间列表
	ui_style_init_column(&s_style_room_list_body);
	lv_style_set_flex_main_place(&s_style_room_list_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_cross_place(&s_style_room_list_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_room_list_body, LV_FLEX_ALIGN_START);
	lv_style_set_size(&s_style_room_list_body, PAGE_TALK_ROOM_WIDTH, LV_PCT(100));
	lv_style_set_bg_color(&s_style_room_list_body, UI_COLOR_PANEL_2);



	// 信息表
	ui_style_init_column(&s_style_info_body);
	lv_style_set_size(&s_style_info_body, PAGE_TALK_INFO_WIDTH, LV_PCT(100));
	lv_style_set_bg_color(&s_style_info_body, UI_COLOR_PANEL_2);


	// 标题外框
	ui_style_init_row(&s_style_title_body);
	lv_style_set_size(&s_style_title_body, LV_PCT(100), 30);
	lv_style_set_pad_left(&s_style_title_body, 10);
	lv_style_set_bg_opa(&s_style_title_body, LV_OPA_TRANSP);
	lv_style_set_border_side(&s_style_title_body, LV_BORDER_SIDE_BOTTOM);
	lv_style_set_border_width(&s_style_title_body, 2);
	lv_style_set_border_color(&s_style_title_body, UI_COLOR_TEXT_MUTED);

	// 标题LABEL
	lv_style_init(&s_style_title_label);
	lv_style_set_text_font(&s_style_title_label, UI_FONT_12);
	lv_style_set_text_color(&s_style_title_label, UI_COLOR_TEXT);

	// 列表外框
	ui_style_init_column(&s_style_list_body);
	lv_style_set_flex_main_place(&s_style_list_body, LV_FLEX_ALIGN_START);
	lv_style_set_flex_cross_place(&s_style_list_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_list_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_width(&s_style_list_body, LV_PCT(100));
	lv_style_set_flex_grow(&s_style_list_body, 1);
	lv_style_set_bg_opa(&s_style_list_body, LV_OPA_TRANSP);
	lv_style_set_border_width(&s_style_list_body, 1);
	lv_style_set_border_opa(&s_style_list_body, LV_OPA_TRANSP);

	// 焦点外框
	lv_style_init(&s_style_focus_body);
	lv_style_set_border_width(&s_style_focus_body, 1);
	lv_style_set_border_color(&s_style_focus_body, UI_COLOR_ACCENT);
	lv_style_set_border_opa(&s_style_focus_body, LV_OPA_COVER);


	// 房间名称body
	ui_style_init_row(&s_style_item_body);
	lv_style_set_size(&s_style_item_body, LV_PCT(90), 25);
	lv_style_set_flex_main_place(&s_style_item_body, LV_FLEX_ALIGN_SPACE_BETWEEN);
	lv_style_set_flex_cross_place(&s_style_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_border_side(&s_style_item_body, LV_BORDER_SIDE_BOTTOM);
	lv_style_set_border_width(&s_style_item_body, 1);
	lv_style_set_border_color(&s_style_item_body, UI_COLOR_BORDER);
	lv_style_set_bg_opa(&s_style_item_body, LV_OPA_TRANSP);
	lv_style_set_pad_hor(&s_style_item_body, 10);

	// 房间 item：基础透明边框，焦点时只改边框色，避免布局跳动
	ui_style_init_row(&s_style_room_item_body);
	lv_style_set_size(&s_style_room_item_body, LV_PCT(90), 25);
	lv_style_set_flex_main_place(&s_style_room_item_body, LV_FLEX_ALIGN_SPACE_BETWEEN);
	lv_style_set_flex_cross_place(&s_style_room_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_room_item_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_border_width(&s_style_room_item_body, 1);
	lv_style_set_border_color(&s_style_room_item_body, UI_COLOR_ACCENT);
	lv_style_set_border_opa(&s_style_room_item_body, LV_OPA_TRANSP);
	lv_style_set_bg_opa(&s_style_room_item_body, LV_OPA_TRANSP);
	lv_style_set_pad_hor(&s_style_room_item_body, 10);

	lv_style_init(&s_style_room_item_selected);
	lv_style_set_bg_color(&s_style_room_item_selected, UI_COLOR_ACCENT);
	lv_style_set_bg_opa(&s_style_room_item_selected, LV_OPA_COVER);

	lv_style_init(&s_style_room_item_focused);
	lv_style_set_border_width(&s_style_room_item_focused, 1);
	lv_style_set_border_color(&s_style_room_item_focused, UI_COLOR_ACCENT);
	lv_style_set_border_opa(&s_style_room_item_focused, LV_OPA_COVER);

	// 主名称label
	lv_style_init(&s_style_item_title_label);
	lv_style_set_text_font(&s_style_item_title_label, UI_FONT_12);
	lv_style_set_text_color(&s_style_item_title_label, UI_COLOR_TEXT);

	// 副名称label
	lv_style_init(&s_style_item_sub_title_label);
	lv_style_set_text_font(&s_style_item_sub_title_label, UI_FONT_12);
	// lv_style_set_text_color(&s_style_item_sub_title_label, UI_COLOR_TEXT_MUTED);
	lv_style_set_text_color(&s_style_item_sub_title_label, UI_COLOR_TEXT);

	// PTT
	ui_style_init_row(&s_style_ptt_body);
	lv_style_set_size(&s_style_ptt_body, LV_PCT(90), 40);
	// lv_style_set_bg_color(&s_style_ptt_body, UI_COLOR_ACCENT);
	lv_style_set_bg_color(&s_style_ptt_body, UI_COLOR_DISABLED);
	lv_style_set_radius(&s_style_ptt_body, CONFIG_UI_MARGIN);
	lv_style_set_border_width(&s_style_ptt_body, 1);
	lv_style_set_border_color(&s_style_ptt_body, UI_COLOR_ACCENT);
	lv_style_set_border_opa(&s_style_ptt_body, LV_OPA_TRANSP);
	lv_style_set_margin_bottom(&s_style_ptt_body, 5);
	lv_style_set_margin_top(&s_style_ptt_body, 5);
	lv_style_set_flex_main_place(&s_style_ptt_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&s_style_ptt_body, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&s_style_ptt_body, LV_FLEX_ALIGN_CENTER);

	// PTT文字
	lv_style_init(&s_style_ptt_label);
	lv_style_set_text_font(&s_style_ptt_label, UI_FONT_16);
	lv_style_set_text_color(&s_style_ptt_label, UI_COLOR_TEXT);
	// lv_style_set_size(&s_style_ptt_label, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
	// lv_style_set_height(&s_style_ptt_label, LV_PCT(90));

	style_init_done = true;
}

static void talk_focus_apply(void)
{
	if(obj_room_list != NULL) {
		lv_obj_remove_style(obj_room_list, &s_style_focus_body, LV_PART_MAIN);
		if(s_talk_focus == TALK_FOCUS_ROOM_LIST) {
			lv_obj_add_style(obj_room_list, &s_style_focus_body, LV_PART_MAIN);
		}
	}

	if(obj_user_list != NULL) {
		lv_obj_remove_style(obj_user_list, &s_style_focus_body, LV_PART_MAIN);
		if(s_talk_focus == TALK_FOCUS_USER_LIST) {
			lv_obj_add_style(obj_user_list, &s_style_focus_body, LV_PART_MAIN);
		}
	}

	if(obj_ptt_body != NULL) {
		lv_obj_remove_style(obj_ptt_body, &s_style_focus_body, LV_PART_MAIN);
		if(s_talk_focus == TALK_FOCUS_PTT) {
			lv_obj_add_style(obj_ptt_body, &s_style_focus_body, LV_PART_MAIN);
		}
	}
}

static void room_item_refresh_style(int index)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	if(index != 0) {
		return;
	}
#else
	size_t room_count = ws_room_cache_room_count();
	if(room_count > ROOM_LIST_MAX_COUNT) {
		room_count = ROOM_LIST_MAX_COUNT;
	}
	if(index < 0 || index >= (int)room_count) {
		return;
	}
#endif

	lv_obj_t *obj = s_room_item_objs[index];
	if(obj == NULL) {
		return;
	}

	lv_obj_remove_style(obj, &s_style_room_item_selected, LV_PART_MAIN);
	lv_obj_remove_style(obj, &s_style_room_item_focused, LV_PART_MAIN);

	if(index == s_current_room_index) {
		lv_obj_add_style(obj, &s_style_room_item_selected, LV_PART_MAIN);
	}

	if(index == s_room_focus_index) {
		lv_obj_add_style(obj, &s_style_room_item_focused, LV_PART_MAIN);
	}
}

static void room_refresh_all_styles(void)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	room_item_refresh_style(0);
#else
	size_t room_count = ws_room_cache_room_count();
	if(room_count > ROOM_LIST_MAX_COUNT) {
		room_count = ROOM_LIST_MAX_COUNT;
	}
	for(int i = 0; i < (int)room_count; i++) {
		room_item_refresh_style(i);
	}
#endif
}

static void room_focus_move(int step)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	(void)step;
	s_room_focus_index = 0;
	room_refresh_all_styles();
	if(s_room_item_objs[0] != NULL) {
		lv_obj_scroll_to_view(s_room_item_objs[0], LV_ANIM_ON);
	}
	return;
#else
	size_t room_count = ws_room_cache_room_count();
	if(room_count == 0) {
		return;
	}
	if(room_count > ROOM_LIST_MAX_COUNT) {
		room_count = ROOM_LIST_MAX_COUNT;
	}

	int old = s_room_focus_index;
	s_room_focus_index += step;
	if(s_room_focus_index < 0) {
		s_room_focus_index = (int)room_count - 1;
	} else if(s_room_focus_index >= (int)room_count) {
		s_room_focus_index = 0;
	}

	room_item_refresh_style(old);
	room_item_refresh_style(s_room_focus_index);

	if(s_room_item_objs[s_room_focus_index] != NULL) {
		lv_obj_scroll_to_view(s_room_item_objs[s_room_focus_index], LV_ANIM_ON);
	}
#endif
}

static void room_set_current(int index)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	(void)index;
	s_current_room_index = 0;
	s_room_focus_index = 0;
	room_refresh_all_styles();
	return;
#else
	ws_room_record_t room = {0};
	if(index < 0 || !ws_room_cache_get_room((size_t)index, &room)) {
		return;
	}

	if(ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY) {
		return;
	}

	if(index == s_current_room_index) {
		s_room_focus_index = index;
		room_refresh_all_styles();
		return;
	}

	s_current_room_index = index;
	s_room_focus_index = index;
	strncpy(s_current_room_id, room.id, sizeof(s_current_room_id) - 1);
	s_current_room_id[sizeof(s_current_room_id) - 1] = '\0';
	strncpy(s_current_room_name, room.name, sizeof(s_current_room_name) - 1);
	s_current_room_name[sizeof(s_current_room_name) - 1] = '\0';

	(void)app_settings_update(&(app_settings_update_t) {
		.id = APP_SETTING_ID_WS_ROOM,
		.value.str = s_current_room_id,
	});
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_USERS_REQ, 1, 0);

	room_refresh_all_styles();
	if(s_room_item_objs[s_room_focus_index] != NULL) {
		lv_obj_scroll_to_view(s_room_item_objs[s_room_focus_index], LV_ANIM_ON);
	}
#endif
}

static void room_item_clicked_cb(lv_event_t *e)
{
	if(lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}

	int index = (int)(intptr_t)lv_event_get_user_data(e);
	room_set_current(index);
}

static void room_init_default(void)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	s_current_room_index = 0;
	s_room_focus_index = 0;
	strncpy(s_current_room_id, "default", sizeof(s_current_room_id) - 1);
	s_current_room_id[sizeof(s_current_room_id) - 1] = '\0';
	strncpy(s_current_room_name, "大厅", sizeof(s_current_room_name) - 1);
	s_current_room_name[sizeof(s_current_room_name) - 1] = '\0';

	if(strcmp(app_settings.ws_room, s_current_room_id) != 0) {
		(void)app_settings_update(&(app_settings_update_t) {
			.id = APP_SETTING_ID_WS_ROOM,
			.value.str = s_current_room_id,
		});
		(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
	}
#else
	const char *wanted = app_settings.ws_room[0] != '\0' ? app_settings.ws_room : "default";
	ws_room_record_t room = {0};
	size_t index = 0;
	if(!ws_room_cache_find_room(wanted, &index, &room)) {
		if(!ws_room_cache_get_room(0, &room)) {
			strncpy(room.id, "default", sizeof(room.id) - 1);
			strncpy(room.name, "大厅", sizeof(room.name) - 1);
		}
		index = 0;
	}

	s_current_room_index = (int)index;
	s_room_focus_index = (int)index;
	strncpy(s_current_room_id, room.id, sizeof(s_current_room_id) - 1);
	s_current_room_id[sizeof(s_current_room_id) - 1] = '\0';
	strncpy(s_current_room_name, room.name, sizeof(s_current_room_name) - 1);
	s_current_room_name[sizeof(s_current_room_name) - 1] = '\0';

	if(strcmp(app_settings.ws_room, s_current_room_id) != 0) {
		(void)app_settings_update(&(app_settings_update_t) {
			.id = APP_SETTING_ID_WS_ROOM,
			.value.str = s_current_room_id,
		});
		(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
	}
#endif
}

static void update_room_list() {
	if(obj_room_list == NULL) {
		return;
	}
	lv_obj_clean(obj_room_list);
	memset(s_room_item_objs, 0, sizeof(s_room_item_objs));

#if !INTERCOM_ROOM_SYNC_ENABLE
	LOG("talk page update room list: static default room current=%s", s_current_room_id);
	lv_obj_t *item = lv_obj_create(obj_room_list);
	lv_obj_add_style(item, &s_style_room_item_body, LV_STATE_DEFAULT);
	lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(item, room_item_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
	s_room_item_objs[0] = item;

	lv_obj_t *title_label = lv_label_create(item);
	lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
	lv_obj_add_flag(title_label, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_label_set_text(title_label, s_current_room_name);

	lv_obj_t *sub_label = lv_label_create(item);
	lv_obj_add_style(sub_label, &s_style_item_sub_title_label, LV_STATE_DEFAULT);
	lv_obj_add_flag(sub_label, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_label_set_text(sub_label, "1");

	room_refresh_all_styles();
	return;
#else
	size_t room_count = ws_room_cache_room_count();
	LOG("talk page update room list: count=%u revision=%u truncated=%d current=%s",
		(unsigned)room_count,
		(unsigned)ws_room_cache_room_revision(),
		(int)ws_room_cache_rooms_truncated(),
		s_current_room_id);
	if(room_count > ROOM_LIST_MAX_COUNT) {
		room_count = ROOM_LIST_MAX_COUNT;
	}
	if(room_count == 0) {
		lv_obj_t *item = lv_obj_create(obj_room_list);
		lv_obj_add_style(item, &s_style_item_body, LV_STATE_DEFAULT);
		lv_obj_t *title_label = lv_label_create(item);
		lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
		lv_label_set_text(title_label, "暂无房间");
		return;
	}

	for(int i = 0; i < (int)room_count; i ++ ) {
		ws_room_record_t room = {0};
		if(!ws_room_cache_get_room((size_t)i, &room)) {
			continue;
		}
		LOG("talk page room[%d]: id=%s name=%s users=%d locked=%d",
			i,
			room.id,
			room.name,
			room.user_count,
			(int)room.locked);
		lv_obj_t *item = lv_obj_create(obj_room_list);
		lv_obj_add_style(item, &s_style_room_item_body, LV_STATE_DEFAULT);
		lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(item, room_item_clicked_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
		s_room_item_objs[i] = item;

		lv_obj_t *title_label = lv_label_create(item);
		lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
		lv_obj_add_flag(title_label, LV_OBJ_FLAG_EVENT_BUBBLE);
		lv_label_set_text(title_label, room.name);

		lv_obj_t *sub_label = lv_label_create(item);
		lv_obj_add_style(sub_label, &s_style_item_sub_title_label, LV_STATE_DEFAULT);
		lv_obj_add_flag(sub_label, LV_OBJ_FLAG_EVENT_BUBBLE);
		lv_label_set_text_fmt(sub_label, "%d", room.user_count);

	}

	room_refresh_all_styles();
#endif
}

static void update_user_list() {
	if(obj_user_list == NULL) {
		return;
	}
	lv_obj_clean(obj_user_list);

#if !INTERCOM_ROOM_SYNC_ENABLE
	const char *callsign = app_settings.user_callsign[0] != '\0' ?
						   app_settings.user_callsign :
						   app_settings.ws_callsign;
	if(callsign == NULL || callsign[0] == '\0') {
		callsign = USER_DEFAULT_CALLSIGN;
	}
	LOG("talk page update user list: static local user callsign=%s", callsign);
	lv_obj_t *item = lv_obj_create(obj_user_list);
	lv_obj_add_style(item, &s_style_item_body, LV_STATE_DEFAULT);
	lv_obj_t *title_label = lv_label_create(item);
	lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
	lv_label_set_text(title_label, callsign);
	return;
#else
	char users_room[ROOM_ID_MAX_LEN + 1] = {0};
	if(ws_room_cache_current_users_room(users_room, sizeof(users_room)) &&
	   users_room[0] != '\0' && strcmp(users_room, s_current_room_id) != 0) {
		LOG("talk page user list waiting: users_room=%s current_room=%s user_revision=%u",
			users_room,
			s_current_room_id,
			(unsigned)ws_room_cache_user_revision());
		lv_obj_t *item = lv_obj_create(obj_user_list);
		lv_obj_add_style(item, &s_style_item_body, LV_STATE_DEFAULT);
		lv_obj_t *title_label = lv_label_create(item);
		lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
		lv_label_set_text(title_label, "等待成员列表");
		return;
	}

	size_t user_count = ws_room_cache_user_count();
	LOG("talk page update user list: room=%s count=%u revision=%u truncated=%d",
		users_room[0] != '\0' ? users_room : "-",
		(unsigned)user_count,
		(unsigned)ws_room_cache_user_revision(),
		(int)ws_room_cache_users_truncated());
	if(user_count > ROOM_USERS_MAX_COUNT) {
		user_count = ROOM_USERS_MAX_COUNT;
	}
	if(user_count == 0) {
		lv_obj_t *item = lv_obj_create(obj_user_list);
		lv_obj_add_style(item, &s_style_item_body, LV_STATE_DEFAULT);
		lv_obj_t *title_label = lv_label_create(item);
		lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
		lv_label_set_text(title_label, "暂无成员");
		return;
	}

	for(int i = 0; i < (int)user_count; i ++) {
		ws_room_user_record_t user = {0};
		if(!ws_room_cache_get_user((size_t)i, &user)) {
			continue;
		}
		LOG("talk page user[%d]: callsign=%s device=%s talking=%d fw=%s",
			i,
			user.callsign,
			user.device_id,
			(int)user.talking,
			user.fw_version);
		lv_obj_t *item = lv_obj_create(obj_user_list);
		lv_obj_add_style(item, &s_style_item_body, LV_STATE_DEFAULT);

		lv_obj_t *title_label = lv_label_create(item);
		lv_obj_add_style(title_label, &s_style_item_title_label, LV_STATE_DEFAULT);
		lv_label_set_text_fmt(title_label, "%s%s", user.talking ? "* " : "", user.callsign);
	}
#endif
}

static void ptt_send_stop(void)
{
	(void)msg_send_cmd_value(
		MSG_SRC_UI,
		MSG_EVT_CMD_INTERCOM_TALK_STOP,
		1,
		0
	);
}

static bool page_talk_audio_active(void)
{
	return ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY;
}

static void page_talk_request_room_snapshots(void)
{
#if !INTERCOM_ROOM_SYNC_ENABLE
	LOG("talk page room snapshot request skipped: room sync disabled");
	return;
#endif
	if(page_talk_audio_active()) {
		s_room_refresh_deferred = true;
		s_user_refresh_deferred = true;
		return;
	}
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_LIST_REQ, 1, 0);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_USERS_REQ, 1, 0);
}

static void page_talk_join_room(void)
{
	LOG("talk page enter intercom room: room=%s", s_current_room_id);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_AUDIO_STOP, 1, 0);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_INTERCOM_ROOM_JOIN, 1, 0);
#if INTERCOM_ROOM_SYNC_ENABLE
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_LIST_REQ, 1, 0);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_USERS_REQ, 1, 0);
#else
	LOG("talk page room snapshots skipped: room sync disabled");
#endif
}

static void page_talk_leave_room(void)
{
	LOG("talk page leave intercom room: room=%s status=%d pressed=%d",
		s_current_room_id,
		(int)ptt_status,
		(int)ptt_is_pressed);
	if(ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY) {
		ptt_send_stop();
	}
	ptt_is_pressed = false;
	ptt_set_status(PTT_STATUS_IDLE);
	(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_INTERCOM_ROOM_LEAVE, 1, 0);
#if INTERCOM_ROOM_SYNC_ENABLE
#else
	LOG("talk page room leave presence skipped: room sync disabled");
#endif
}

static void ptt_set_status(ptt_status_t status)
{
	ptt_status_t old_status = ptt_status;
	if(ptt_status != status) {
		LOG("talk page ptt status: %d -> %d pressed=%d",
			(int)ptt_status,
			(int)status,
			(int)ptt_is_pressed);
	}
	ptt_status = status;

	bool was_audio_active = old_status == PTT_STATUS_CHECKING || old_status == PTT_STATUS_READY;
	bool is_audio_active = ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY;
	if(was_audio_active && !is_audio_active && (s_room_refresh_deferred || s_user_refresh_deferred)) {
		LOG("talk page flush deferred room snapshots: room=%d users=%d",
			(int)s_room_refresh_deferred,
			(int)s_user_refresh_deferred);
		s_room_refresh_deferred = false;
		s_user_refresh_deferred = false;
#if INTERCOM_ROOM_SYNC_ENABLE
		(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_LIST_REQ, 1, 0);
		(void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_WS_ROOM_USERS_REQ, 1, 0);
#endif
	}

	if(obj_ptt_body == NULL || obj_ptt_label == NULL) {
		return;
	}

	switch(status) {
	case PTT_STATUS_IDLE:
		lv_obj_set_style_bg_color(obj_ptt_body, UI_COLOR_ACCENT, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "PTT");
		break;

	case PTT_STATUS_CHECKING:
		lv_obj_set_style_bg_color(obj_ptt_body, UI_COLOR_ERROR, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "等待");
		break;

	case PTT_STATUS_READY:
		lv_obj_set_style_bg_color(obj_ptt_body, PAGE_TALK_PTT_READY_COLOR, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "讲话");
		break;

	case PTT_STATUS_BUSY:
		lv_obj_set_style_bg_color(obj_ptt_body, UI_COLOR_DISABLED, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "占用");
		break;

	case PTT_STATUS_OFFLINE:
		lv_obj_set_style_bg_color(obj_ptt_body, UI_COLOR_DISABLED, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "离线");
		break;

	case PTT_STATUS_ERROR:
		lv_obj_set_style_bg_color(obj_ptt_body, UI_COLOR_ERROR, LV_STATE_DEFAULT);
		lv_label_set_text(obj_ptt_label, "失败");
		break;
	}
}

static const char *ptt_ack_name(int code)
{
	switch(code) {
		case MSG_INTERCOM_TALK_ACK_OK:
			return "ok";
		case MSG_INTERCOM_TALK_ACK_BUSY:
			return "busy";
		case MSG_INTERCOM_TALK_ACK_OFFLINE:
			return "offline";
		case MSG_INTERCOM_TALK_ACK_TIMEOUT:
			return "timeout";
		case MSG_INTERCOM_TALK_ACK_LOCAL_ERROR:
			return "local_error";
		default:
			return "unknown";
	}
}

static void ptt_check_result(int ack_code)
{
	LOG("talk page ptt ack: code=%d reason=%s pressed=%d",
		ack_code,
		ptt_ack_name(ack_code),
		(int)ptt_is_pressed);
	if(ack_code <= 0) {
		if(!ptt_is_pressed) {
			ptt_set_status(PTT_STATUS_IDLE);
		} else if(ack_code == MSG_INTERCOM_TALK_ACK_BUSY) {
			ptt_set_status(PTT_STATUS_BUSY);
		} else if(ack_code == MSG_INTERCOM_TALK_ACK_OFFLINE) {
			ptt_is_pressed = false;
			ptt_set_status(PTT_STATUS_OFFLINE);
		} else {
			ptt_is_pressed = false;
			ptt_set_status(PTT_STATUS_ERROR);
		}
		return;
	}

	if(ptt_is_pressed) {
		ptt_set_status(PTT_STATUS_READY);
	} else {
		ptt_send_stop();
		ptt_set_status(PTT_STATUS_IDLE);
	}
}

static void check_ptt_status() {
	LOG("talk page ptt start pressed: room=%s status=%d",
		s_current_room_id,
		(int)ptt_status);
	ptt_set_status(PTT_STATUS_CHECKING);
	(void)msg_send_cmd_value(
		MSG_SRC_UI,
		MSG_EVT_CMD_INTERCOM_TALK_START_REQ,
		1,
		0
	);
}

static void ptt_pressed(lv_event_t *e) {
	(void)e;

	if(ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY) {
		LOG("talk page ptt press ignored: status=%d", (int)ptt_status);
		return;
	}

	ptt_is_pressed = true;
	check_ptt_status();
}

static void ptt_released(lv_event_t *e) {
	(void)e;

	if(!ptt_is_pressed) {
		LOG("talk page ptt release ignored: not pressed status=%d", (int)ptt_status);
		return;
	}

	ptt_is_pressed = false;

	if(ptt_status == PTT_STATUS_READY) {
		LOG("talk page ptt release: send stop from ready");
		ptt_send_stop();
		ptt_set_status(PTT_STATUS_IDLE);
		page_talk_request_room_snapshots();
		return;
	}

	if(ptt_status == PTT_STATUS_CHECKING) {
		LOG("talk page ptt release while checking: defer stop");
		ptt_send_stop();
		return;
	}

	if(ptt_status == PTT_STATUS_BUSY || ptt_status == PTT_STATUS_ERROR) {
		ptt_set_status(PTT_STATUS_IDLE);
	}
}

static void talk_focus_move(int step)
{
	int next = (int)s_talk_focus + step;
	if(next < 0) {
		next = TALK_FOCUS_COUNT - 1;
	} else if(next >= TALK_FOCUS_COUNT) {
		next = 0;
	}

	s_talk_focus = (talk_focus_t)next;
	s_talk_focus_active = false;
	talk_focus_apply();
}

static void talk_handle_encoder_rotate(bool clockwise)
{
	int step = clockwise ? 1 : -1;

	if(!s_talk_focus_active) {
		talk_focus_move(step);
		return;
	}

	switch(s_talk_focus) {
		case TALK_FOCUS_ROOM_LIST:
			room_focus_move(step);
			break;

		case TALK_FOCUS_USER_LIST:
			if(obj_user_list != NULL) {
				lv_obj_scroll_by(obj_user_list, 0, -step * PAGE_TALK_LIST_SCROLL_STEP, LV_ANIM_ON);
			}
			break;

		case TALK_FOCUS_PTT:
		default:
			talk_focus_move(step);
			break;
	}
}

static void talk_handle_encoder_press(void)
{
	switch(s_talk_focus) {
		case TALK_FOCUS_ROOM_LIST:
		case TALK_FOCUS_USER_LIST:
			s_talk_focus_active = !s_talk_focus_active;
			break;

		case TALK_FOCUS_PTT:
		default:
			s_talk_focus_active = false;
			break;
	}

	talk_focus_apply();
}

static void page_talk_input_handler(const msg_t *msg)
{
	if(msg == NULL || msg->type != MSG_TYPE_INPUT) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_INPUT_ENCODER_CW:
			talk_handle_encoder_rotate(true);
			break;

		case MSG_EVT_INPUT_ENCODER_CCW:
			talk_handle_encoder_rotate(false);
			break;

		case MSG_EVT_INPUT_ENCODER_PRESS:
			talk_handle_encoder_press();
			break;

		case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
			if(s_talk_focus == TALK_FOCUS_PTT) {
				ptt_pressed(NULL);
			}
			break;

		case MSG_EVT_INPUT_ENCODER_RELEASE:
			if(s_talk_focus == TALK_FOCUS_PTT) {
				ptt_released(NULL);
			}
			break;

		default:
			break;
	}
}

static void page_talk_msg_handler(const msg_t *msg)
{
	if(msg == NULL || msg->type != MSG_TYPE_SYS) {
		return;
	}

	switch(msg->event) {
		case MSG_EVT_SYS_INTERCOM_TALK_START_ACK:
			ptt_check_result(msg->data.value);
			break;

		case MSG_EVT_SYS_WS_CONNECTED:
#if INTERCOM_ROOM_SYNC_ENABLE
			LOG("talk page ws connected: request room list/users");
			page_talk_request_room_snapshots();
#else
			LOG("talk page ws connected: room sync disabled");
#endif
			break;

		case MSG_EVT_SYS_WS_ROOM_LIST_UPDATED:
#if INTERCOM_ROOM_SYNC_ENABLE
			if(page_talk_audio_active()) {
				s_room_refresh_deferred = true;
				break;
			}
			LOG("talk page room list updated event: revision=%d", msg->data.value);
			room_init_default();
			update_room_list();
#else
			LOG("talk page room list update ignored: room sync disabled revision=%d", msg->data.value);
#endif
			break;

		case MSG_EVT_SYS_WS_ROOM_USERS_UPDATED:
#if INTERCOM_ROOM_SYNC_ENABLE
			if(page_talk_audio_active()) {
				s_user_refresh_deferred = true;
				break;
			}
			LOG("talk page room users updated event: revision=%d", msg->data.value);
			update_user_list();
#else
			LOG("talk page room users update ignored: room sync disabled revision=%d", msg->data.value);
#endif
			break;

		case MSG_EVT_SYS_WS_DISCONNECTED:
		case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
		case MSG_EVT_SYS_WIFI_DISCONNECTED:
			LOG("talk page connection lost event=%d status=%d pressed=%d",
				(int)msg->event,
				(int)ptt_status,
				(int)ptt_is_pressed);
			if(ptt_status == PTT_STATUS_CHECKING || ptt_status == PTT_STATUS_READY) {
				ptt_is_pressed = false;
				ptt_set_status(PTT_STATUS_OFFLINE);
			}
			break;

		default:
			break;
	}
}

static const ui_page_ops_t page_talk_ops = {
	.on_input = page_talk_input_handler,
	.on_msg = page_talk_msg_handler,
	.on_leave = page_talk_leave_room,
};

esp_err_t page_talk_show(lv_obj_t *p) {
	style_init();
	ui_actor_set_ops(&page_talk_ops);
	room_init_default();
	s_talk_focus = TALK_FOCUS_ROOM_LIST;
	s_talk_focus_active = false;
	ptt_is_pressed = false;

	lv_obj_t *page_body = lv_obj_create(p);
	lv_obj_add_style(page_body, &style_page_body, LV_STATE_DEFAULT);

	ui_add_top_status(page_body);

	lv_obj_t *main_body = lv_obj_create(page_body);
	lv_obj_add_style(main_body, &s_style_main_body, LV_STATE_DEFAULT);
	
	lv_obj_t *room_list_body = lv_obj_create(main_body);
	lv_obj_add_style(room_list_body, &s_style_room_list_body, LV_STATE_DEFAULT);

	lv_obj_t *room_list_title_body = lv_obj_create(room_list_body);
	lv_obj_add_style(room_list_title_body, &s_style_title_body, LV_STATE_DEFAULT);
	
	lv_obj_t *room_list_title_label = lv_label_create(room_list_title_body);
	lv_obj_add_style(room_list_title_label, &s_style_title_label, LV_STATE_DEFAULT);
	lv_label_set_text(room_list_title_label, "房间列表");

	obj_room_list = lv_obj_create(room_list_body);
	lv_obj_add_style(obj_room_list, &s_style_list_body, LV_STATE_DEFAULT);
	
	lv_obj_t *info_body = lv_obj_create(main_body);
	lv_obj_add_style(info_body, &s_style_info_body, LV_STATE_DEFAULT);

	lv_obj_t *user_list_title_body = lv_obj_create(info_body);
	lv_obj_add_style(user_list_title_body, &s_style_title_body, LV_STATE_DEFAULT);

	
	
	lv_obj_t *user_list_title_label = lv_label_create(user_list_title_body);
	lv_obj_add_style(user_list_title_label, &s_style_title_label, LV_STATE_DEFAULT);
	lv_label_set_text(user_list_title_label, "成员列表");

	
	obj_user_list = lv_obj_create(info_body);
	lv_obj_add_style(obj_user_list, &s_style_list_body, LV_STATE_DEFAULT);
	

	obj_ptt_body = lv_obj_create(info_body);
	lv_obj_add_style(obj_ptt_body, &s_style_ptt_body, LV_STATE_DEFAULT);
	lv_obj_add_flag(obj_ptt_body, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(obj_ptt_body, LV_OBJ_FLAG_SCROLLABLE);

	
	obj_ptt_label = lv_label_create(obj_ptt_body);
	lv_obj_add_style(obj_ptt_label, &s_style_ptt_label, LV_STATE_DEFAULT);

	lv_obj_add_event_cb(obj_ptt_body, ptt_pressed, LV_EVENT_PRESSED, NULL);
	lv_obj_add_event_cb(obj_ptt_body, ptt_released, LV_EVENT_RELEASED, NULL);
	lv_obj_add_event_cb(obj_ptt_body, ptt_released, LV_EVENT_PRESS_LOST, NULL);

	update_room_list();
	update_user_list();
	page_talk_join_room();


	ptt_set_status(PTT_STATUS_IDLE);
	talk_focus_apply();

	return ESP_OK;
}
