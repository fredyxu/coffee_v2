#include "modules/ui/page/page_settings_item/sections/page_settings_section_display.h"

#include "app/app_settings.h"
#include "modules/ui/ui.h"

#define DISPLAY_DEFAULT_BRIGHTNESS 50

#ifdef ESP_PLATFORM
extern esp_err_t lcd_set_backlight(uint8_t percent);
#endif

static void display_brightness_preview_change(const settings_sub_item_t *item, int value)
{
    (void)item;
#ifdef ESP_PLATFORM
    if(value < 0) {
        value = 0;
    } else if(value > 100) {
        value = 100;
    }

    (void)lcd_set_backlight((uint8_t)value);
#else
    (void)value;
#endif
}

static void display_reset_defaults_action(const settings_sub_item_t *item)
{
    (void)item;
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_DISPLAY_BRIGHTNESS,
        .value.i32 = DISPLAY_DEFAULT_BRIGHTNESS,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
        .value.b = false,
    });
}

static settings_sub_item_t s_display_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_DISPLAY_BRIGHTNESS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "屏幕亮度",
        .value = &app_settings.display_brightness,
        .min_value = 5,
        .max_value = 100,
        .step = 5,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_DISPLAY_BRIGHTNESS,
        .on_preview_change = display_brightness_preview_change,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_DISPLAY_CW_DECODE,
        .value_type = SETTINGS_VALUE_TYPE_BOOL,
        .title = "电码转义显示",
        .value = &app_settings.cw_decode_display_enable,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_DISPLAY_RESET_DEFAULTS,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认显示设置",
        .on_action = display_reset_defaults_action,
    },
};

const settings_sub_item_t *page_settings_section_display_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_display_sub_items) / sizeof(s_display_sub_items[0]);
    }

    return s_display_sub_items;
}
