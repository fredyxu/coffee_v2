#include "page_settings_data.h"

static const settings_item_t s_settings_items[] = {
    {
        .id = SETTINGS_ITEM_ID_BACK,
        .title = "< 返回",
        .subtitle = "回到主界面",
    },
    {
        .id = SETTINGS_ITEM_ID_WIFI,
        .title = "WiFi",
        .subtitle = "无线网络设置",
    },
	{
        .id = SETTINGS_ITEM_ID_WS,
        .title = "WebSocket",
        .subtitle = "WebSocket服务器设置",
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
