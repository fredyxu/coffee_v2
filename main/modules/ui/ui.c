#include "ui.h"
#include "ui/page/page_init/page_init.h"

#include "lvgl.h"
#include "core/utils/log.h"
#include "modules/ui/adapter/lvgl_port.h"
#include "ui_actor.h"
#include <stddef.h>
#include <stdint.h>

#define UI_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))


static page_id_t s_current_page = -1;

/**
 * @brief 切换当前页面
 * @param page_id 目标页面枚举
 * @return ESP_OK 成功，其他表示参数或状态有误
 */
esp_err_t page_show(page_id_t page_id)
{
    if(page_id < PAGE_INIT || page_id > PAGE_SETTINGS) {
        LOG("无效的页面 ID: %d", page_id);
        return ESP_ERR_INVALID_ARG;
    }
    lv_obj_t *screen = lv_scr_act();
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
        LOG("切换到页面: INIT");
        page_init_show(screen);
        break;

    case PAGE_HOME:
        LOG("切换到页面: HOME");
        // view_home_show(screen);
        // add_bottom_navigation(screen);
        break;
    case PAGE_MENU:
        // view_menu_show(screen);
        // add_bottom_navigation(screen);
        break;
    case PAGE_SETTINGS:
        // view_settings_show(screen);
        // add_bottom_navigation(screen);
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
    // lv_obj_t *scr = lv_scr_act();

    // lv_obj_t *label = lv_label_create(scr);
    // lv_label_set_text(label, "Hello LVGL");
    // lv_obj_center(label);
	page_show(PAGE_INIT);
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

