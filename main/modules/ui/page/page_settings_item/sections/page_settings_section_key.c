#include "modules/ui/page/page_settings_item/sections/page_settings_section_key.h"

#include <stdio.h>
#include <stdlib.h>

#include "app/app_settings.h"
#include "config/config_sys_key.h"
#include "core/msg/msg.h"
#include "modules/ui/ui.h"

static char s_key_mode_text[8];
static char s_key_swap_text[8];
static int32_t s_key_auto_da_ratio_x10;
static int32_t s_key_auto_gap_ratio_x10;

static int32_t key_ratio_to_x10(const char *text, const char *fallback, float min_value, float max_value)
{
    const char *source = (text != NULL && text[0] != '\0') ? text : fallback;
    float value = strtof(source, NULL);
    if(value < min_value) {
        value = min_value;
    } else if(value > max_value) {
        value = max_value;
    }

    return (int32_t)(value * 10.0f + 0.5f);
}

static void key_sync_display_values(void)
{
    (void)snprintf(
        s_key_mode_text,
        sizeof(s_key_mode_text),
        "%s",
        app_settings.key_mode == KEY_MODE_AUTO ? "自动" : "手动"
    );
    (void)snprintf(
        s_key_swap_text,
        sizeof(s_key_swap_text),
        "%s",
        app_settings.key_swap_ab ? "交换" : "正常"
    );
    s_key_auto_da_ratio_x10 = key_ratio_to_x10(app_settings.key_auto_da_ratio, KEY_DEFAULT_AUTO_DA_RATIO, 1.0f, 5.0f);
    s_key_auto_gap_ratio_x10 = key_ratio_to_x10(app_settings.key_auto_gap_ratio, KEY_DEFAULT_AUTO_GAP_RATIO, 0.3f, 1.0f);
}

static void key_ratio_format_value(const settings_sub_item_t *item, char *buffer, size_t buffer_size)
{
    if(item == NULL || item->value == NULL || buffer == NULL || buffer_size == 0) {
        return;
    }

    int32_t value_x10 = *(int32_t *)item->value;
    (void)snprintf(buffer, buffer_size, "%d.%d", (int)(value_x10 / 10), (int)(value_x10 % 10));
}

static void key_auto_da_ratio_format_value(const settings_sub_item_t *item, char *buffer, size_t buffer_size)
{
    key_ratio_format_value(item, buffer, buffer_size);
}

static void key_auto_gap_ratio_format_value(const settings_sub_item_t *item, char *buffer, size_t buffer_size)
{
    key_ratio_format_value(item, buffer, buffer_size);
}

static void key_auto_da_ratio_changed_action(const settings_sub_item_t *item)
{
    if(item == NULL || item->value == NULL) {
        return;
    }

    char ratio_text[8];
    key_auto_da_ratio_format_value(item, ratio_text, sizeof(ratio_text));
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_AUTO_DA_RATIO,
        .value.str = ratio_text,
    });
}

static void key_auto_gap_ratio_changed_action(const settings_sub_item_t *item)
{
    if(item == NULL || item->value == NULL) {
        return;
    }

    char ratio_text[8];
    key_auto_gap_ratio_format_value(item, ratio_text, sizeof(ratio_text));
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_AUTO_GAP_RATIO,
        .value.str = ratio_text,
    });
}

static void key_notify_mode_refresh(void)
{
    (void)msg_send_cmd_value(MSG_SRC_UI, MSG_EVT_CMD_KEY_REFRESH_MODE, 0, 0);
}

static void key_mode_toggle_action(const settings_sub_item_t *item)
{
    (void)item;
    int32_t next_mode = app_settings.key_mode == KEY_MODE_AUTO ? KEY_MODE_MANUAL : KEY_MODE_AUTO;
    if(app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_MODE,
        .value.i32 = next_mode,
    }) == ESP_OK) {
        key_sync_display_values();
        key_notify_mode_refresh();
    }
}

static void key_left_toggle_action(const settings_sub_item_t *item)
{
    (void)item;
    if(app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_SWAP_AB,
        .value.b = !app_settings.key_swap_ab,
    }) == ESP_OK) {
        key_sync_display_values();
    }
}

static void key_reset_debounce_action(const settings_sub_item_t *item)
{
    (void)item;
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_DEBOUNCE_MS,
        .value.i32 = KEY_DEFAULT_DEBOUNCE_MS,
    });
}

static void key_reset_defaults_action(const settings_sub_item_t *item)
{
    (void)item;

    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_ENABLE,
        .value.b = true,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_MODE,
        .value.i32 = KEY_DEFAULT_MODE,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_SWAP_AB,
        .value.b = KEY_DEFAULT_SWAP_AB != 0,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_DEBOUNCE_MS,
        .value.i32 = KEY_DEFAULT_DEBOUNCE_MS,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_MANUAL_DI_MS,
        .value.i32 = KEY_DEFAULT_MANUAL_DI_MS,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
        .value.b = KEY_DEFAULT_MANUAL_ADAPTIVE_ENABLE != 0,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_AUTO_DI_MS,
        .value.i32 = KEY_DEFAULT_AUTO_DI_MS,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_AUTO_DA_RATIO,
        .value.str = KEY_DEFAULT_AUTO_DA_RATIO,
    });
    (void)app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_KEY_AUTO_GAP_RATIO,
        .value.str = KEY_DEFAULT_AUTO_GAP_RATIO,
    });

    key_sync_display_values();
    key_notify_mode_refresh();
}

static void key_ensure_enabled(void)
{
    if(!app_settings.key_enable) {
        (void)app_settings_update(&(app_settings_update_t) {
            .id = APP_SETTING_ID_KEY_ENABLE,
            .value.b = true,
        });
    }

    if(app_settings.key_auto_di_ms <= 0) {
        (void)app_settings_update(&(app_settings_update_t) {
            .id = APP_SETTING_ID_KEY_AUTO_DI_MS,
            .value.i32 = KEY_DEFAULT_AUTO_DI_MS,
        });
    }
}

static settings_sub_item_t s_key_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_MODE,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "电键模式",
        .value = s_key_mode_text,
        .on_action = key_mode_toggle_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_SWAP_AB,
        .value_type = SETTINGS_VALUE_TYPE_TEXT,
        .title = "左右电键互换",
        .value = s_key_swap_text,
        .on_action = key_left_toggle_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_DEBOUNCE_MS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "消抖时长",
        .value = &app_settings.key_debounce_ms,
        .min_value = 0,
        .max_value = 100,
        .step = 1,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_DEBOUNCE_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_RESET_DEBOUNCE,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认消抖时长",
        .on_action = key_reset_debounce_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_MANUAL_DI_MS,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "手动点时长",
        .value = &app_settings.key_manual_di_ms,
        .min_value = 40,
        .max_value = 300,
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
        .title = "自动点时长",
        .value = &app_settings.key_auto_di_ms,
        .min_value = 40,
        .max_value = 300,
        .step = 10,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_KEY_AUTO_DI_MS,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_AUTO_DA_RATIO,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "自动划倍数",
        .value = &s_key_auto_da_ratio_x10,
        .min_value = 10,
        .max_value = 50,
        .step = 1,
        .on_change = key_auto_da_ratio_changed_action,
        .format_value = key_auto_da_ratio_format_value,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_AUTO_GAP_RATIO,
        .value_type = SETTINGS_VALUE_TYPE_INT,
        .title = "自动间隔倍数",
        .value = &s_key_auto_gap_ratio_x10,
        .min_value = 3,
        .max_value = 10,
        .step = 1,
        .on_change = key_auto_gap_ratio_changed_action,
        .format_value = key_auto_gap_ratio_format_value,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_KEY_RESET_DEFAULTS,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "恢复默认电键设置",
        .on_action = key_reset_defaults_action,
    },
};

const settings_sub_item_t *page_settings_section_key_get_sub_items(size_t *count)
{
    key_ensure_enabled();
    key_sync_display_values();

    if(count != NULL) {
        *count = sizeof(s_key_sub_items) / sizeof(s_key_sub_items[0]);
    }

    return s_key_sub_items;
}
