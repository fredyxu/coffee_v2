#include "modules/ui/page/page_settings_item/sections/page_settings_section_ws.h"

#include <stdio.h>

#include "app/app_settings.h"
#include "config/config_sys_ws.h"
#include "core/msg/msg.h"
#include "core/state/status.h"
#include "modules/ui/ui.h"

static char s_ws_status_text[16] = "未连接";
static char s_ws_url_text[sizeof(app_settings.ws_url)];

static void ws_url_changed_action(const settings_sub_item_t *item)
{
    (void)item;
    (void)msg_send_cmd_value(MSG_SRC_LVGL, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
}

static void ws_set_default_url_action(const settings_sub_item_t *item)
{
    (void)item;

    esp_err_t err = app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_WS_URL,
        .value.str = WS_DEFAULT_URL,
    });
    if(err != ESP_OK) {
        return;
    }

    (void)msg_send_cmd_value(MSG_SRC_LVGL, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
}

static settings_sub_item_t s_ws_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
	{
        .id = SETTINGS_SUB_ITEM_ID_WS_ENABLE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "开启WebSocket",
        .value = &app_settings.ws_enable,
        .has_change_cmd_event = true,
        .change_cmd_event = MSG_EVT_CMD_WS_SET_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_STATUS,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "连接状态",
        .value = s_ws_status_text,
        .readonly = true,
    },
    
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_URL,
        .value_type = SETTINGS_VALUE_TYPE_INPUT,
        .title = "服务器地址",
        .value = s_ws_url_text,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_WS_URL,
        .on_change = ws_url_changed_action,
    },

	{
        .id = SETTINGS_SUB_ITEM_ID_WS_SET_DEFAULT_URL,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认服务器地址",
        .on_action = ws_set_default_url_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_AUTO_RECONNECT,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "自动重连",
        .value = &app_settings.ws_auto_reconnect,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_WS_AUTO_RECONNECT,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_RECONNECT,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "重新连接",
        .has_cmd_event = true,
        .cmd_event = MSG_EVT_CMD_WS_RECONNECT,
    },
};

static void ws_url_sync_display_value(void)
{
    const char *url = app_settings.ws_url[0] != '\0' ? app_settings.ws_url : WS_DEFAULT_URL;
    (void)snprintf(s_ws_url_text, sizeof(s_ws_url_text), "%s", url);
}

static void ws_status_sync_from_current(void)
{
    status_current_t current = {0};
    if(status_get_current(&current) != ESP_OK) {
        return;
    }

    (void)snprintf(
        s_ws_status_text,
        sizeof(s_ws_status_text),
        "%s",
        current.ws_connected ? "已连接" : "未连接"
    );
}

const settings_sub_item_t *page_settings_section_ws_get_sub_items(size_t *count)
{
    ws_status_sync_from_current();
    ws_url_sync_display_value();

    if(count != NULL) {
        *count = sizeof(s_ws_sub_items) / sizeof(s_ws_sub_items[0]);
    }

    return s_ws_sub_items;
}

bool page_settings_section_ws_handle_msg(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_SYS) {
        return false;
    }

    switch(msg->event) {
    case MSG_EVT_SYS_WS_CONNECTED:
        (void)snprintf(s_ws_status_text, sizeof(s_ws_status_text), "%s", "已连接");
        return true;

    case MSG_EVT_SYS_WS_DISCONNECTED:
    case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
        (void)snprintf(s_ws_status_text, sizeof(s_ws_status_text), "%s", "未连接");
        return true;

    default:
        break;
    }

    return false;
}
