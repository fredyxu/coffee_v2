#include "modules/ui/page/page_settings_item/sections/page_settings_section_wifi.h"

#include "app/app_settings.h"
#include "modules/ui/ui.h"
#include "modules/wifi/wifi_settings.h"

static settings_sub_item_t s_wifi_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_ENABLE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "开启WIFI",
        .value = &app_settings.wifi_enable,
        .has_change_cmd_event = true,
        .change_cmd_event = MSG_EVT_CMD_WIFI_SET_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_SCAN,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "扫描网络",
        .has_cmd_event = true,
        .cmd_event = MSG_EVT_CMD_WIFI_SCAN,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_SSID_LIST,
        .value_type = SETTINGS_VALUE_TYPE_LIST,
        .title = "可用网络",
        .value_source = &wifi_settings_ssid_source,
    },
};

const settings_sub_item_t *page_settings_section_wifi_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_wifi_sub_items) / sizeof(s_wifi_sub_items[0]);
    }

    return s_wifi_sub_items;
}
