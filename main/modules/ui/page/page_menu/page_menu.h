#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplucplus 
extern "C" {
#endif

esp_err_t page_menu_show(lv_obj_t *parent);

#ifdef __cplucplus
}
#endif