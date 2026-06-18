#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_ITEM_ID_BACK = 0,
	SETTINGS_ITEM_ID_USER,
    SETTINGS_ITEM_ID_WIFI,
    SETTINGS_ITEM_ID_BT,
    SETTINGS_ITEM_ID_KEY,
    SETTINGS_ITEM_ID_AUDIO,
    SETTINGS_ITEM_ID_DISPLAY,
    SETTINGS_ITEM_ID_WS,
    SETTINGS_ITEM_ID_MORSE,
} settings_item_id_t;

typedef struct {
    settings_item_id_t id;
    const char *title;
    const char *subtitle;

} settings_item_t;

const settings_item_t *page_settings_get_items(size_t *count);
const settings_item_t *page_settings_find_item(settings_item_id_t id);

#ifdef __cplusplus
}
#endif
