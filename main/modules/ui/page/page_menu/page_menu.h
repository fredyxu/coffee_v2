#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include "modules/ui/ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const char *title;
    const char *icon;
    ui_page_id_t page_id;
} menu_item_t;

esp_err_t page_menu_show(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
