#include "modules/ui/page/page_settings/sections/page_settings_section_audio.h"

#include "app/app_settings.h"

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

const settings_sub_item_t *page_settings_section_audio_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_audio_sub_items) / sizeof(s_audio_sub_items[0]);
    }

    return s_audio_sub_items;
}
