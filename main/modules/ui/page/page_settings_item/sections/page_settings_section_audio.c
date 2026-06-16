#include "modules/ui/page/page_settings_item/sections/page_settings_section_audio.h"

#include "app/app_settings.h"
#include "config/config_sys_key.h"
#include "core/msg/msg.h"
#include "modules/ui/ui.h"

#define AUDIO_DEFAULT_VOLUME 50
#define AUDIO_TONE_PREVIEW_MS 80

#ifdef ESP_PLATFORM
extern esp_err_t audio_set_volume(uint8_t volume_percent);
#endif

static void audio_volume_preview_change(const settings_sub_item_t *item, int value)
{
    (void)item;
    int freq = app_settings.key_tone_hz > 0 ? app_settings.key_tone_hz : KEY_DEFAULT_TONE_HZ;
#ifdef ESP_PLATFORM
    if(value < 0) {
        value = 0;
    } else if(value > 100) {
        value = 100;
    }

    (void)audio_set_volume((uint8_t)value);
#else
    (void)value;
#endif
    msg_t msg = msg_make_cmd_tone(MSG_SRC_LVGL, freq, AUDIO_TONE_PREVIEW_MS, 0);
    (void)msg_send_cmd(&msg, 0);
}

static void audio_key_tone_preview_change(const settings_sub_item_t *item, int value)
{
    (void)item;
    msg_t msg = msg_make_cmd_tone(MSG_SRC_LVGL, value, AUDIO_TONE_PREVIEW_MS, 0);
    (void)msg_send_cmd(&msg, 0);
}

static void audio_reset_defaults_action(const settings_sub_item_t *item)
{
    (void)item;

    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_AUDIO_VOLUME,
        .value.i32 = AUDIO_DEFAULT_VOLUME,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_TONE_HZ,
        .value.i32 = KEY_DEFAULT_TONE_HZ,
    });
}

static settings_sub_item_t s_audio_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "音频音量",
        .value = &app_settings.audio_volume,
        .min_value = 0,
        .max_value = 100,
        .step = 5,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_AUDIO_VOLUME,
        .on_preview_change = audio_volume_preview_change,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_AUDIO_TONE,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "侧音频率",
        .value = &app_settings.key_tone_hz,
        .min_value = 300,
        .max_value = 1200,
        .step = 10,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_TONE_HZ,
        .on_preview_change = audio_key_tone_preview_change,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_AUDIO_RESET_DEFAULTS,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认音频设置",
        .on_action = audio_reset_defaults_action,
    },
};

const settings_sub_item_t *page_settings_section_audio_get_sub_items(size_t *count)
{
    if(count != NULL) {
        *count = sizeof(s_audio_sub_items) / sizeof(s_audio_sub_items[0]);
    }

    return s_audio_sub_items;
}
