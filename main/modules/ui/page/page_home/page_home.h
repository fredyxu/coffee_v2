#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*view_init_done_cb_t)(void *);

esp_err_t page_home_show(lv_obj_t *parent);


#ifdef __cplusplus
}
#endif
