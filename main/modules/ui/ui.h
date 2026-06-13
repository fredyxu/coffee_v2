#pragma once
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "modules/ui/page/page_settings/page_settings_data.h"




typedef enum {
    PAGE_INIT = 0,
    PAGE_HOME,
    PAGE_MENU,
    PAGE_SETTINGS,
	PAGE_SETTINGS_ITEM,
	PAGE_NONE,
} ui_page_id_t;

typedef struct {
	ui_page_id_t page_id;
	settings_item_id_t settings_item_id;
} ui_page_nav_param_t;

typedef struct {
	ui_page_nav_param_t stack[8];
	size_t depth;
} ui_nav_t;



esp_err_t ui_init(void);

// static esp_err_t page_show(ui_page_id_t page_id);

void ui_nav_back(void);
void ui_nav_go(ui_page_nav_param_t param);

typedef struct settings_sub_item_t settings_sub_item_t;
void ui_nav_back_action(const settings_sub_item_t *item);

// 初始化页面
esp_err_t ui_page_init(lv_obj_t *p, lv_obj_t **s);

ui_page_id_t ui_get_current_page(void);
