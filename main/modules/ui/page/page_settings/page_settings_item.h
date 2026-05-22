#pragma once

#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

void page_settings_item_show(lv_obj_t *p, settings_item_id_t id);

#ifdef __cplusplus
}
#endif
