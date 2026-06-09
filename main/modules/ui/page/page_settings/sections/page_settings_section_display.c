#include "modules/ui/page/page_settings/sections/page_settings_section_display.h"

#include "app/app_settings.h"

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

const settings_sub_item_t *page_settings_section_display_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_display_sub_items) / sizeof(s_display_sub_items[0]);
    }

    return s_display_sub_items;
}
