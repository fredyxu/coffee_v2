#include "modules/ui/page/page_settings_item/page_settings_item_data.h"

#include "modules/ui/page/page_settings_item/sections/page_settings_section_audio.h"
#include "modules/ui/page/page_settings_item/sections/page_settings_section_display.h"
#include "modules/ui/page/page_settings_item/sections/page_settings_section_key.h"
#include "modules/ui/page/page_settings_item/sections/page_settings_section_wifi.h"
#include "modules/ui/page/page_settings_item/sections/page_settings_section_ws.h"

const settings_sub_item_t *page_settings_item_get_sub_items(settings_item_id_t id, size_t *count)
{
    if(count != NULL) {
        *count = 0;
    }

    switch(id) {
    case SETTINGS_ITEM_ID_WIFI:
        return page_settings_section_wifi_get_sub_items(count);

    case SETTINGS_ITEM_ID_AUDIO:
        return page_settings_section_audio_get_sub_items(count);

    case SETTINGS_ITEM_ID_KEY:
        return page_settings_section_key_get_sub_items(count);

    case SETTINGS_ITEM_ID_DISPLAY:
        return page_settings_section_display_get_sub_items(count);

    case SETTINGS_ITEM_ID_WS:
        return page_settings_section_ws_get_sub_items(count);

    default:
        return NULL;
    }
}
