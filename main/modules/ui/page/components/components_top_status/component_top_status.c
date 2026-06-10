#include "component_top_status.h"

#include "lvgl.h"
#include "esp_err.h"
#include "config/config_sys.h"
#include "core/state/status.h"
#include "ui/theme/color.h"
#include "ui/theme/font.h"
#include <stdbool.h>

#define TOP_STATUS_ICON_HEIGHT 20
#define STATUS_SIZE 5
// #define TOP_STATUS_BG_COLOR UI_COLOR_PANEL_2
#define TOP_STATUS_BG_COLOR UI_COLOR_PANEL

static lv_obj_t *status_body;
static lv_obj_t *obj_status_body;
static lv_obj_t *obj_icon_body;

static lv_obj_t *obj_icon_wifi;
static lv_obj_t *obj_icon_link;
// static lv_obj_t *obj_icon_bt;
static lv_obj_t *obj_status;

static lv_obj_t *label_icon_wifi;
static lv_obj_t *label_icon_link;
// static lv_obj_t *label_icon_bt;
static lv_obj_t *label_status;

static lv_style_t style_body;
static lv_style_t style_icon;
static lv_style_t style_icon_label;

static bool style_inited = false;

void ui_top_status_handle_sys_msg(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_SYS) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_SYS_WIFI_CONNECTED:
        case MSG_EVT_SYS_WIFI_DISCONNECTED:
        case MSG_EVT_SYS_WIFI_SIGNAL_LEVEL:
        case MSG_EVT_SYS_WS_CONNECTED:
        case MSG_EVT_SYS_WS_DISCONNECTED:
        case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
            ui_top_status_ref_icon();
            break;

        default:
            break;
    }
}

void ui_top_status_ref_icon(void) {
    if(obj_icon_wifi == NULL || obj_icon_link == NULL) {
        return;
    }

    status_current_t cur = {0};
    if(status_get_current(&cur) != ESP_OK) {
        return;
    }

	if(cur.wifi_connected) {
		// lv_label_set_text(label_icon_wifi, ICON_WIFI_FULL);
		if(cur.wifi_level < 1) {
			lv_label_set_text(label_icon_wifi, ICON_WIFI_NO);
		} else if(cur.wifi_level == 1) {
			lv_label_set_text(label_icon_wifi, ICON_WIFI_1);
		} else if(cur.wifi_level == 2) {
			lv_label_set_text(label_icon_wifi, ICON_WIFI_2);
		} else if(cur.wifi_level == 3) {
			lv_label_set_text(label_icon_wifi, ICON_WIFI_3);
		} else {
			lv_label_set_text(label_icon_wifi, ICON_WIFI_FULL);
		}
	} else {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_NO);
	}
	

    lv_label_set_text(label_icon_link, cur.ws_connected ? ICON_LINK_WS_DONE : ICON_LINK_WS_BREAK);
}

static void init_style(void)
{
    if(style_inited) {
        return;
    }
    lv_style_init(&style_body);
	lv_style_set_bg_color(&style_body, TOP_STATUS_BG_COLOR);
    // lv_style_set_bg_opa(&style_body, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_body, 0);
    lv_style_set_pad_all(&style_body, 0);
	lv_style_set_radius(&style_body, 0);


	lv_style_init(&style_icon);
	lv_style_set_flex_flow(&style_icon, LV_FLEX_FLOW_ROW);
	lv_style_set_flex_main_place(&style_icon, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_cross_place(&style_icon, LV_FLEX_ALIGN_CENTER);
	lv_style_set_flex_track_place(&style_icon, LV_FLEX_ALIGN_CENTER);
	// lv_style_set_pad_left(&style_icon, 1);
	lv_style_set_text_font(&style_icon, UI_FONT_14);
    lv_style_set_text_color(&style_icon, UI_COLOR_ACCENT);
	lv_style_set_size(&style_icon, TOP_STATUS_ICON_HEIGHT, TOP_STATUS_ICON_HEIGHT);
	lv_style_set_border_width(&style_icon, 0);
	lv_style_set_bg_color(&style_icon, TOP_STATUS_BG_COLOR);
	// lv_style_set_margin_top(&style_icon, 2);
	// lv_style_set_pad_top(&style_icon, 2);
	lv_style_set_pad_all(&style_icon, 0);
	lv_style_set_margin_all(&style_icon, 0);


	lv_style_init(&style_icon_label);
	lv_style_set_text_font(&style_icon_label, UI_FONT_14);
	lv_style_set_text_color(&style_icon_label, UI_COLOR_ACCENT);
	lv_style_set_translate_y(&style_icon_label, 1);
	lv_style_set_bg_color(&style_icon_label, TOP_STATUS_BG_COLOR);
	// lv_style_text_font(label_icon_wifi, UI_FONT_14, 0);
	// lv_obj_set_style_text_color(label_icon_wifi, UI_COLOR_ACCENT, 0);
	// lv_obj_center(label_icon_wifi);
	// lv_obj_set_style_translate_y(label_icon_wifi, 1, 0);

	
    style_inited = true;
}


esp_err_t ui_add_top_status(lv_obj_t *p)
{
    if(p == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

	init_style();

	status_body = lv_obj_create(p);
    lv_obj_set_size(status_body, DISPLAY_H_RES, CONFIG_UI_TOP_STATUS_HEIGHT);
    lv_obj_align(status_body, LV_ALIGN_TOP_MID, 0, 0);
	// lv_obj_set_style_bg_color(status_body, UI_COLOR_BG_SECONDARY, 0);
	lv_obj_set_style_bg_color(status_body, UI_COLOR_PANEL_2, 0);

    lv_obj_set_style_bg_opa(status_body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_body, 0, 0);
    lv_obj_set_style_radius(status_body, 0, 0);
    lv_obj_set_style_pad_left(status_body, 4, 0);
	lv_obj_set_style_pad_right(status_body, 4, 0);
	lv_obj_set_style_margin_all(status_body, 0, 0);
	lv_obj_set_style_clip_corner(status_body, false, 0);
	// 禁止出现滚动条
	lv_obj_remove_flag(status_body, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_flex_flow(status_body, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(status_body, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

	obj_status_body = lv_obj_create(status_body);
	lv_obj_add_style(obj_status_body, &style_body, 0);
	lv_obj_set_size(obj_status_body, LV_PCT(50), TOP_STATUS_ICON_HEIGHT);
	lv_obj_set_flex_flow(obj_status_body, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(obj_status_body,
						  LV_FLEX_ALIGN_START,
						  LV_FLEX_ALIGN_CENTER,
						  LV_FLEX_ALIGN_CENTER);

	obj_icon_body = lv_obj_create(status_body);
	lv_obj_add_style(obj_icon_body, &style_body, 0);
	lv_obj_set_style_pad_right(obj_icon_body, 5, 0);
	lv_obj_set_size(obj_icon_body, LV_PCT(50), TOP_STATUS_ICON_HEIGHT);
	lv_obj_set_flex_flow(obj_icon_body, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(obj_icon_body, 
							LV_FLEX_ALIGN_END, 
							LV_FLEX_ALIGN_CENTER, 
							LV_FLEX_ALIGN_CENTER);

    obj_icon_wifi = lv_obj_create(obj_icon_body);
	lv_obj_add_style(obj_icon_wifi, &style_icon, 0);
	lv_obj_remove_flag(obj_icon_wifi, LV_OBJ_FLAG_SCROLLABLE);

	label_icon_wifi = lv_label_create(obj_icon_wifi);
	lv_obj_add_style(label_icon_wifi, &style_icon_label, 0);
	lv_label_set_text(label_icon_wifi, ICON_WIFI_NO);
	lv_obj_center(label_icon_wifi);


	obj_icon_link = lv_obj_create(obj_icon_body);
	lv_obj_add_style(obj_icon_link, &style_icon, 0);
	lv_obj_remove_flag(obj_icon_link, LV_OBJ_FLAG_SCROLLABLE);

	label_icon_link = lv_label_create(obj_icon_link);
	lv_obj_add_style(label_icon_link, &style_icon_label, 0);
	lv_label_set_text(label_icon_link, ICON_LINK_WS_BREAK);
	lv_obj_center(label_icon_link);

	obj_status = lv_obj_create(obj_status_body);
	lv_obj_add_style(obj_status, &style_icon, 0);
	lv_obj_remove_flag(obj_status, LV_OBJ_FLAG_SCROLLABLE);

	label_status = lv_label_create(obj_status);
	lv_obj_add_style(label_status, &style_icon_label, 0);
	lv_label_set_text(label_status, "●");
	lv_obj_center(label_status);


	return ESP_OK;
}


void ui_top_status_set_wifi_signal(int level) {
	if(label_icon_wifi == NULL) {
		return;
	}

	if(level <= 0) {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_NO);
	} else if(level == 1) {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_1);
	} else if(level == 2) {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_2);
	} else if(level == 3) {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_3);
	} else {
		lv_label_set_text(label_icon_wifi, ICON_WIFI_FULL);
	}
}
