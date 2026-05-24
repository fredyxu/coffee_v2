#pragma once

#include "lvgl.h"
#include "page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

void page_settings_item_show(lv_obj_t *p, settings_item_id_t id);

#ifdef __cplusplus
}
#endif
