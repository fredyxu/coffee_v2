#include "page_settings_data.h"
#include "config/config_settings.h"

#define SETTINGS_WIFI_SSID_MAX 16

static size_t s_wifi_ssid_count = 3;

static settings_value_list_t s_wifi_ssid_list[SETTINGS_WIFI_SSID_MAX] = {
	{
		.title = "SSID_1",
		.value_int = -50,
	},
	{
		.title = "SSID_2",
		.value_int = -70,
	},
	{
		.title = "SSID_3",
		.value_int = -80,
	},
};

static const settings_sub_item_t s_wifi_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_ENABLE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "开启WIFI",
        .value = &app_settings.wifi_enable,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_SSID_LIST,
        .value_type = SETTINGS_VALUE_TYPE_LIST,
        .title = "可用网络",
        .value_list = s_wifi_ssid_list,
		.value_count = &s_wifi_ssid_count,
    },
    // {
    //     .id = SETTINGS_SUB_ITEM_ID_WIFI_SSID,
    //     .value_type = SETTINGS_VALUE_TYPE_TEXT,
    //     .title = "WiFi名称",
    //     .value = &app_settings.wifi_ssid,
    // },
    {
        .id = SETTINGS_SUB_ITEM_ID_WIFI_PASSWORD,
        .value_type = SETTINGS_VALUE_TYPE_PASSWORD,
        .title = "WiFi密码",
        .value = &app_settings.wifi_password,
    },
	{
        .id = SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "音频音量",
        .value = &app_settings.audio_volume,
        .min_value = 0,
        .max_value = 100,
        .step = 1,
    },

};

static const settings_sub_item_t s_audio_sub_items[] = {
    // {
    //     .id = SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
    //     .value_type = SETTINGS_VALUE_TYPE_INT,
    //     .title = "音量",
    //     .subtitle = "设置信号提示音音量",
    //     .value_text = "70%",
    // 	.value_int = 70,
    //     .min_value = 0,
    //     .max_value = 100,
    //     .step = 1,
    // },
    // {
    //     .id = SETTINGS_SUB_ITEM_ID_AUDIO_TONE,
    //     .value_type = SETTINGS_VALUE_TYPE_INT,
    //     .title = "音调",
    //     .subtitle = "设置信号提示音频率",
    //     .value_text = "700Hz",
    // 	.value_int = 700,
    //     .min_value = 300,
    //     .max_value = 1200,
    //     .step = 50,
    // },
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

    default:
        return NULL;
    }
}
