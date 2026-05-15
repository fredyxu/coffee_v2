#pragma once
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"

typedef enum {
    PAGE_INIT = 0,
    PAGE_HOME,
    PAGE_MENU,
    PAGE_SETTINGS,
	PAGE_SETTINGS_ITEM,
	PAGE_NONE,
} page_id_t;

esp_err_t ui_init(void);

esp_err_t page_show(page_id_t page_id);

// 初始化页面
esp_err_t ui_page_init(lv_obj_t *p, lv_obj_t **s);

