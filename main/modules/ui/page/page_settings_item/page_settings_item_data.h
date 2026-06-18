#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "app/app_settings.h"
#include "core/msg/msg.h"
#include "modules/ui/page/page_settings/page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_VALUE_TYPE_TEXT = 0,
    SETTINGS_VALUE_TYPE_BOOL,
    SETTINGS_VALUE_TYPE_INT,
    SETTINGS_VALUE_TYPE_LIST,
    SETTINGS_VALUE_TYPE_PASSWORD,
    SETTINGS_VALUE_TYPE_ACTION,
	SETTINGS_VALUE_TYPE_INPUT,
} settings_value_type_t;

typedef struct settings_sub_item_t settings_sub_item_t;
typedef struct settings_value_list_t settings_value_list_t;
typedef void (*settings_action_cb_t)(const settings_sub_item_t *item);
typedef void (*settings_change_cb_t)(const settings_sub_item_t *item);
typedef void (*settings_preview_change_cb_t)(const settings_sub_item_t *item, int value);
typedef void (*settings_value_format_cb_t)(const settings_sub_item_t *item, char *buffer, size_t buffer_size);
typedef void (*settings_value_list_action_cb_t)(const settings_value_list_t *item, void *user_data);

typedef enum {
	SETTINGS_SUB_ITEM_ID_BACK = 0,

    SETTINGS_SUB_ITEM_ID_WIFI_SSID,
    SETTINGS_SUB_ITEM_ID_WIFI_PASSWORD,
    SETTINGS_SUB_ITEM_ID_WIFI_ENABLE,
    SETTINGS_SUB_ITEM_ID_WIFI_SSID_LIST,
	SETTINGS_SUB_ITEM_ID_WIFI_SCAN,

    SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
    SETTINGS_SUB_ITEM_ID_AUDIO_TONE,
	SETTINGS_SUB_ITEM_ID_AUDIO_RESET_DEFAULTS,

    SETTINGS_SUB_ITEM_ID_DISPLAY_BRIGHTNESS,
	SETTINGS_SUB_ITEM_ID_DISPLAY_RESET_DEFAULTS,

    SETTINGS_SUB_ITEM_ID_MORSE_DECODE_DISPLAY,
    SETTINGS_SUB_ITEM_ID_MORSE_AUTO_SEND,
    SETTINGS_SUB_ITEM_ID_MORSE_AUTO_SEND_DELAY,
    SETTINGS_SUB_ITEM_ID_MORSE_RESET_DEFAULTS,

    SETTINGS_SUB_ITEM_ID_WS_STATUS,
    SETTINGS_SUB_ITEM_ID_WS_ENABLE,
    SETTINGS_SUB_ITEM_ID_WS_URL,
    SETTINGS_SUB_ITEM_ID_WS_ROOM,
    SETTINGS_SUB_ITEM_ID_WS_CALLSIGN,
    SETTINGS_SUB_ITEM_ID_WS_AUTO_RECONNECT,
    SETTINGS_SUB_ITEM_ID_WS_RECONNECT,
	SETTINGS_SUB_ITEM_ID_WS_SET_DEFAULT_URL,

    SETTINGS_SUB_ITEM_ID_KEY_ENABLE,
    SETTINGS_SUB_ITEM_ID_KEY_MODE,
    SETTINGS_SUB_ITEM_ID_KEY_SWAP_AB,
    SETTINGS_SUB_ITEM_ID_KEY_DEBOUNCE_MS,
    SETTINGS_SUB_ITEM_ID_KEY_MANUAL_DI_MS,
    SETTINGS_SUB_ITEM_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
    SETTINGS_SUB_ITEM_ID_KEY_AUTO_DI_MS,
    SETTINGS_SUB_ITEM_ID_KEY_AUTO_DA_RATIO,
    SETTINGS_SUB_ITEM_ID_KEY_AUTO_GAP_RATIO,
    SETTINGS_SUB_ITEM_ID_KEY_TONE_HZ,
	SETTINGS_SUB_ITEM_ID_KEY_RESET_DEBOUNCE,
	SETTINGS_SUB_ITEM_ID_KEY_RESET_DEFAULTS,

	SETTINGS_SUB_ITEM_ID_USER_CALLSIGN,
} settings_sub_item_id_t;

typedef enum {
    SETTINGS_VALUE_SWICH_KEY_MANUL = 0,
    SETTINGS_VALUE_SWICH_KEY_AUTO,

} settings_value_swich_t;

typedef enum {
    SETTINGS_LIST_ITEM_NORMAL = 0,
    SETTINGS_LIST_ITEM_STATUS,
} settings_list_item_type_t;

struct settings_value_list_t {
    char title[48];
	char value_str[65];
    int value_int;
    bool disabled;
    settings_list_item_type_t type;
	bool selected;
    settings_value_list_action_cb_t on_action;
    void *user_data;
};

typedef struct {
    settings_value_list_t *list;
    size_t *count;
    size_t max;
} settings_value_list_source_t;

struct settings_sub_item_t {
    settings_sub_item_id_t id;
    settings_value_type_t value_type;

    const char *title;
    const char *subtitle;

    void *value;

    int min_value;
    int max_value;
    int step;

	// 这个值是否绑定到 app_settings_update()。有的话，修改时走持久化设置更新。
    bool has_setting_id;
    app_setting_id_t setting_id;

    bool readonly;

    const settings_value_list_source_t *value_source;

    bool has_cmd_event;
    msg_event_t cmd_event;
    int cmd_value;

    bool has_change_cmd_event;
    msg_event_t change_cmd_event;

    settings_action_cb_t on_action;
    settings_change_cb_t on_change;
    settings_preview_change_cb_t on_preview_change;
    settings_value_format_cb_t format_value;
};

const settings_sub_item_t *page_settings_item_get_sub_items(settings_item_id_t id, size_t *count);
bool page_settings_item_handle_msg(settings_item_id_t id, const msg_t *msg);

#ifdef __cplusplus
}
#endif
