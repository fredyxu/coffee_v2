#include "component_top_status.h"

#include "lvgl.h"
#include "esp_err.h"
#include "config/config_sys.h"
#include "ui/theme/color.h"

static lv_obj_t *status_body;

esp_err_t ui_add_top_status(lv_obj_t *p) {
	status_body = lv_obj_crete(p);
	lv_obj_set_style_size(status_body, DISPLAY_H_RES, 20, 0);
	lv_obj_set_style_bg_color(status_body, UI_COLOR_BG_SECONDARY, 0);
	return ESP_OK;
}