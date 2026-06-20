#include "page_menu.h"

#include "lvgl.h"
#include "modules/ui/style/ui_style.h"
#include "modules/ui/page/components/components_top_status/component_top_status.h"

static bool style_init_done = false;
static lv_style_t page_body;

static void style_init(void) {
	if(style_init_done) {
		return;
	}
	// 主体样式
	ui_style_init_column(&page_body);



	style_init_done = true;
}

esp_err_t page_menu_show(lv_obj_t *p) {
	(void)style_init();

	ui_add_top_status(p);




	return ESP_OK;
}
