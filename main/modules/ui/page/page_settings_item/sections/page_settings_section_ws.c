#include "modules/ui/page/page_settings_item/sections/page_settings_section_ws.h"

#include "app/app_settings.h"
#include "modules/ui/ui.h"

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
        .id = SETTINGS_SUB_ITEM_ID_WS_URL,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "服务器地址",
        .value = app_settings.ws_url,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_WS_URL,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_ROOM,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "房间",
        .value = app_settings.ws_room,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_WS_ROOM,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WS_CALLSIGN,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "呼号",
        .value = app_settings.ws_callsign,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_WS_CALLSIGN,
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

const settings_sub_item_t *page_settings_section_ws_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_ws_sub_items) / sizeof(s_ws_sub_items[0]);
    }

    return s_ws_sub_items;
}
