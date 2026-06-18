#include "modules/ui/page/page_settings_item/sections/page_settings_section_user.h"

#include <stdio.h>

#include "app/app_settings.h"
#include "core/msg/msg.h"
#include "modules/ui/ui.h"

static char s_user_callsign_text[sizeof(app_settings.user_callsign)];

static void user_callsign_changed_action(const settings_sub_item_t *item)
{
    (void)item;
    (void)msg_send_cmd_value(MSG_SRC_LVGL, MSG_EVT_CMD_WS_RECONNECT, 1, 0);
}

static settings_sub_item_t s_user_sub_items[] = {
    {
        .id = SETTINGS_SUB_ITEM_ID_BACK,
        .value_type = SETTINGS_VALUE_TYPE_ACTION,
        .title = "返回",
        .on_action = ui_nav_back_action,
    },
    {
        .id = SETTINGS_SUB_ITEM_ID_USER_CALLSIGN,
        .value_type = SETTINGS_VALUE_TYPE_INPUT,
        .title = "用户呼号",
        .subtitle = "请输入用户呼号",
        .value = s_user_callsign_text,
        .has_setting_id = true,
        .setting_id = APP_SETTING_ID_USER_CALLSIGN,
        .on_change = user_callsign_changed_action,
    },
};

static void user_callsign_sync_display_value(void)
{
    (void)snprintf(
        s_user_callsign_text,
        sizeof(s_user_callsign_text),
        "%s",
        app_settings.user_callsign
    );
}

const settings_sub_item_t *page_settings_section_user_get_sub_items(size_t *count)
{
    user_callsign_sync_display_value();

    if(count != NULL) {
        *count = sizeof(s_user_sub_items) / sizeof(s_user_sub_items[0]);
    }

    return s_user_sub_items;
}
