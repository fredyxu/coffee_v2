#include "page_settings_data.h"
#include "app/app_settings.h"
#include "modules/wifi/wifi_settings.h"
#include "modules/ui/ui.h"

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

static settings_sub_item_t s_audio_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "音频音量",
        .value = &app_settings.audio_volume,
        .min_value = 0,
        .max_value = 100,
        .step = 1,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_AUDIO_VOLUME,
    },
};

static settings_sub_item_t s_display_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_DISPLAY_BRIGHTNESS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "屏幕亮度",
        .value = &app_settings.display_brightness,
        .min_value = 0,
        .max_value = 100,
        .step = 5,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_DISPLAY_BRIGHTNESS,
    },
};

static const settings_item_t s_settings_items[] = {
    {
        .id = SETTINGS_ITEM_ID_BACK,
        .title = "返回",
        .subtitle = "回到主界面",
    },
    {
        .id = SETTINGS_ITEM_ID_WIFI,
        .title = "WiFi",
        .subtitle = "无线网络设置",
    },
    {
        .id = SETTINGS_ITEM_ID_KEY,
        .title = "电键",
        .subtitle = "设置电报码输入方式",
    },
    {
        .id = SETTINGS_ITEM_ID_AUDIO,
        .title = "音频",
        .subtitle = "音量/音调设置",
    },
    {
        .id = SETTINGS_ITEM_ID_DISPLAY,
        .title = "显示",
        .subtitle = "调节屏幕亮度",
    },
    {
        .id = SETTINGS_ITEM_ID_WS,
        .title = "WebSocket",
        .subtitle = "WebSocket服务器设置",
    },
};

const settings_item_t *page_settings_get_items(size_t *count) {
    if (count != NULL) {
        *count = sizeof(s_settings_items) / sizeof(s_settings_items[0]);
    }

    return s_settings_items;
}

const settings_item_t *page_settings_find_item(settings_item_id_t id) {
    for (size_t i = 0; i < sizeof(s_settings_items) / sizeof(s_settings_items[0]); i++) {
        if (s_settings_items[i].id == id) {
            return &s_settings_items[i];
        }
    }

    return NULL;
}

const settings_sub_item_t *page_settings_get_sub_items(settings_item_id_t id, size_t *count) {
    if (count != NULL) {
        *count = 0;
    }

    switch (id) {
    case SETTINGS_ITEM_ID_WIFI:
        if (count != NULL) {
            *count = sizeof(s_wifi_sub_items) / sizeof(s_wifi_sub_items[0]);
        }
        return s_wifi_sub_items;

    case SETTINGS_ITEM_ID_AUDIO:
        if (count != NULL) {
            *count = sizeof(s_audio_sub_items) / sizeof(s_audio_sub_items[0]);
        }
        return s_audio_sub_items;

    case SETTINGS_ITEM_ID_DISPLAY:
        if (count != NULL) {
            *count = sizeof(s_display_sub_items) / sizeof(s_display_sub_items[0]);
        }
        return s_display_sub_items;

    default:
        return NULL;
    }
}
