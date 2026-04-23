#pragma once
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"

typedef enum {
    PAGE_INIT,
    PAGE_HOME,
    PAGE_MENU,
    PAGE_SETTINGS,
} page_id_t;

esp_err_t ui_init(void);

esp_err_t page_show(page_id_t page_id);


