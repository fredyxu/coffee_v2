#include "modules/ui/page/page_settings/sections/page_settings_section_key.h"

#include "app/app_settings.h"
#include "modules/ui/ui.h"

static settings_sub_item_t s_key_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_ENABLE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "开启电键",
        .value = &app_settings.key_enable,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_MODE,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "电键模式",
        .subtitle = "0手动 1自动",
        .value = &app_settings.key_mode,
        .min_value = 0,
        .max_value = 1,
        .step = 1,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_MODE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_SWAP_AB,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "A/B互换",
        .value = &app_settings.key_swap_ab,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_SWAP_AB,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_DEBOUNCE_MS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "消抖ms",
        .value = &app_settings.key_debounce_ms,
        .min_value = 0,
        .max_value = 1000,
        .step = 1,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_DEBOUNCE_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_MANUAL_DI_MS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "手动DI ms",
        .value = &app_settings.key_manual_di_ms,
        .min_value = 1,
        .max_value = 2000,
        .step = 10,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_MANUAL_DI_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "手动自适应",
        .value = &app_settings.key_manual_adaptive_enable,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_AUTO_DI_MS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "自动DI ms",
        .subtitle = "0为手动DI的0.7倍",
        .value = &app_settings.key_auto_di_ms,
        .min_value = 0,
        .max_value = 2000,
        .step = 10,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_AUTO_DI_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_AUTO_DA_RATIO,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "自动DA倍数",
        .value = app_settings.key_auto_da_ratio,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_AUTO_DA_RATIO,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_TONE_HZ,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "侧音Hz",
        .value = &app_settings.key_tone_hz,
        .min_value = 1,
        .max_value = 3000,
        .step = 10,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_TONE_HZ,
    },
};

const settings_sub_item_t *page_settings_section_key_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_key_sub_items) / sizeof(s_key_sub_items[0]);
    }

    return s_key_sub_items;
}
