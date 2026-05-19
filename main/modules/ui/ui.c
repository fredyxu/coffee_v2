#include "ui.h"
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"
#include "core/utils/log.h"
#include "modules/ui/adapter/lvgl_port.h"
#include "modules/ui/style/ui_style.h"
#include "ui_actor.h"

#include "modules/ui/page/page_init/page_init.h"
#include "modules/ui/page/page_home/page_home.h"
#include "modules/ui/page/page_settings/page_settings.h"
#include "modules/ui/page/page_settings/page_settings_item.h"
#include "modules/ui/ui_actor.h"


#define UI_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


static page_id_t s_current_page = -1;

static lv_obj_t *screen;

static void ui_screen_init() {
	screen = lv_scr_act();
}

/**
 * @brief 切换当前页面
 * @param page_id 目标页面枚举
 * @return ESP_OK 成功，其他表示参数或状态有误
 */
esp_err_t page_show(page_id_t page_id) {
	ui_actor_clean_ops();
    if(page_id < PAGE_INIT || page_id > PAGE_NONE) {
        LOG("无效的页面 ID: %d", page_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if(screen == NULL) {
        LOG("当前没有活动屏幕");
        return ESP_ERR_INVALID_STATE;
    }
    if(page_id == s_current_page) {
        LOG("页面已在显示: %d", page_id);
        return ESP_OK;
    }

    lv_obj_clean(screen);

    switch(page_id) {
    case PAGE_INIT:
        page_init_show(screen);
        break;

    case PAGE_HOME:
        page_home_show(screen);
        break;
    case PAGE_SETTINGS_ITEM:
		page_settings_item_show(screen, SETTINGS_ITEM_ID_WIFI);
        break;
    case PAGE_SETTINGS:
		page_settings_show(screen);
        break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
    s_current_page = page_id;
    return ESP_OK;
}

/**
 * @brief UI 初始化入口，默认展示自检页面并输出日志。
 */
static void ui_create(void *arg)
{
	ui_style_init();
	ui_screen_init();
	page_show(PAGE_SETTINGS_ITEM);
	// page_settings_item_show(screen, 0);
}

esp_err_t ui_init(void)
{
    lvgl_port_run(ui_create, NULL);

	esp_err_t err = ui_actor_init();
	if(err != ESP_OK) {
		LOG("UI actor init error");
		return err;
	}
    return ESP_OK;
}




// esp_err_t ui_page_init(lv_obj_t *p, lv_obj_t **s) {
//     if(p == NULL || s == NULL) {
//         return ESP_ERR_INVALID_ARG;
//     }
// 	lv_obj_clean(p);
// 	*s = lv_obj_create(p);
// 	lv_obj_set_size(*s, DISPLAY_H_RES, DISPLAY_V_RES);
// 	lv_obj_set_layout(*s, LV_LAYOUT_FLEX);
// 	lv_obj_set_flex_flow(*s, LV_FLEX_FLOW_COLUMN);
// 	lv_obj_set_flex_align(*s, 
// 							LV_FLEX_ALIGN_START,
// 							LV_FLEX_ALIGN_CENTER,
// 							LV_FLEX_ALIGN_START);
	
// 	// lv_obj_set_style_pad_row(*s, 0, 0);
// 	// lv_obj_set_style_pad_column(*s, 0, 0);

// 	lv_obj_set_style_border_width(*s, 0, LV_STATE_DEFAULT);
// 	lv_obj_set_style_outline_width(*s, 0, LV_STATE_DEFAULT);
// 	lv_obj_set_style_shadow_width(*s, 0, LV_STATE_DEFAULT);
// 	lv_obj_remove_flag(*s, LV_OBJ_FLAG_SCROLLABLE);
// 	lv_obj_set_style_radius(*s, 0, LV_STATE_DEFAULT);
// 	lv_obj_set_style_pad_all(*s, 0, LV_STATE_DEFAULT);
// 	lv_obj_set_style_margin_all(*s, 0, LV_STATE_DEFAULT);


//     return ESP_OK;
// }
