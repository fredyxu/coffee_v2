#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "app/app_settings.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_ITEM_ID_BACK = 0,
    SETTINGS_ITEM_ID_WIFI,
    SETTINGS_ITEM_ID_BT,
    SETTINGS_ITEM_ID_KEY,
    SETTINGS_ITEM_ID_AUDIO,
    SETTINGS_ITEM_ID_DISPLAY,
    SETTINGS_ITEM_ID_WS,
} settings_item_id_t;

typedef enum {
    SETTINGS_ITEM_TYPE_ACTION,
    SETTINGS_ITEM_TYPE_SWITCH,
    SETTINGS_ITEM_TYPE_VALUE,
    SETTINGS_ITEM_TYPE_SELECT,
} settings_set_type_t;

typedef enum {
    SETTINGS_VALUE_TYPE_TEXT = 0,
    SETTINGS_VALUE_TYPE_BOOL,
    SETTINGS_VALUE_TYPE_INT,
    SETTINGS_VALUE_TYPE_LIST,
    SETTINGS_VALUE_TYPE_PASSWORD,
    SETTINGS_VALUE_TYPE_ACTION,
} settings_value_type_t;

typedef struct settings_sub_item_t settings_sub_item_t;
typedef void (*settings_action_cb_t)(const settings_sub_item_t *item);
typedef void (*settings_change_cb_t)(const settings_sub_item_t *item);

typedef enum {
    SETTINGS_PAGE_WIFI = 0,
    SETTINGS_PAGE_WS,
    SETTINGS_PAGE_BT,
} settings_page_t;

typedef enum {
    SETTINGS_ACTION_GO_BAK = 0,
} settings_action_code_t;

typedef struct {
    settings_item_id_t id;
    const char *title;
    const char *subtitle;

} settings_item_t;

typedef enum {
	SETTINGS_SUB_ITEM_ID_BACK = 0,
	
    SETTINGS_SUB_ITEM_ID_WIFI_SSID,
    SETTINGS_SUB_ITEM_ID_WIFI_PASSWORD,
    SETTINGS_SUB_ITEM_ID_WIFI_ENABLE,
    SETTINGS_SUB_ITEM_ID_WIFI_SSID_LIST,
	SETTINGS_SUB_ITEM_ID_WIFI_SCAN,

    SETTINGS_SUB_ITEM_ID_AUDIO_VOLUME,
    SETTINGS_SUB_ITEM_ID_AUDIO_TONE,

    SETTINGS_SUB_ITEM_ID_DISPLAY_BRIGHTNESS,
} settings_sub_item_id_t;

typedef enum {
    SETTINGS_VALUE_SWICH_KEY_MANUL = 0,
    SETTINGS_VALUE_SWICH_KEY_AUTO,

} settings_value_swich_t;

typedef enum {
    SETTINGS_LIST_ITEM_NORMAL = 0,
    SETTINGS_LIST_ITEM_STATUS,
} settings_list_item_type_t;

typedef struct {
    char title[33];
    int value_int;
    bool disabled;
    settings_list_item_type_t type;
} settings_value_list_t;

struct settings_sub_item_t {
    settings_sub_item_id_t id;
    settings_value_type_t value_type;

    const char *title;
    const char *subtitle;

    void *value;

    int min_value;
    int max_value;
    int step;

    bool has_setting_id;
    app_setting_id_t setting_id;

    settings_value_list_t *value_list;
    size_t *value_count;
    size_t value_list_max;

    bool has_cmd_event;
    msg_event_t cmd_event;
    int cmd_value;

    bool has_change_cmd_event;
    msg_event_t change_cmd_event;

    settings_action_cb_t on_action;
    settings_change_cb_t on_change;
};



const settings_item_t *page_settings_get_items(size_t *count);
const settings_item_t *page_settings_find_item(settings_item_id_t id);

const settings_sub_item_t *page_settings_get_sub_items(settings_item_id_t id, size_t *count);

#ifdef __cplusplus
}
#endif
