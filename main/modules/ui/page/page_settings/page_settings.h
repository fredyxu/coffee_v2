#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include <stdint.h>
#include "modules/ui/page/page_settings/page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif



esp_err_t page_settings_show(lv_obj_t *p);



#ifdef __cplusplus
}
#endif
