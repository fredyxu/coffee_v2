#include "modules/ui/page/page_settings_item/sections/page_settings_section_morse.h"

#include "app/app_settings.h"
#include "modules/ui/ui.h"

#define MORSE_DEFAULT_AUTO_SEND_DELAY_MS 3000

static void morse_reset_defaults_action(const settings_sub_item_t *item)
{
    (void)item;

    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
        .value.b = false,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_MORSE_AUTO_SEND_ENABLE,
        .value.b = false,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_MORSE_AUTO_SEND_DELAY_MS,
        .value.i32 = MORSE_DEFAULT_AUTO_SEND_DELAY_MS,
    });
}

static settings_sub_item_t s_morse_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_MORSE_DECODE_DISPLAY,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "电码转义显示",
        .value = &app_settings.cw_decode_display_enable,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_MORSE_AUTO_SEND,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "自动发送",
        .value = &app_settings.morse_auto_send_enable,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_MORSE_AUTO_SEND_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_MORSE_AUTO_SEND_DELAY,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "自动发送等待时长",
        .value = &app_settings.morse_auto_send_delay_ms,
        .min_value = 500,
        .max_value = 10000,
        .step = 500,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_MORSE_AUTO_SEND_DELAY_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_MORSE_RESET_DEFAULTS,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认摩斯电码设置",
        .on_action = morse_reset_defaults_action,
    },
};

const settings_sub_item_t *page_settings_section_morse_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_morse_sub_items) / sizeof(s_morse_sub_items[0]);
    }

    return s_morse_sub_items;
}
