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
#include "modules/ui/page/page_settings_item/page_settings_item.h"
#include "modules/ui/page/page_menu/page_menu.h"
#include "modules/ui/ui_actor.h"
#include "modules/ui/page/page_talk/page_talk.h"


#define UI_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static ui_nav_t ui_nav = {0};
static ui_page_nav_param_t s_current_page = {
	.page_id = PAGE_NONE,
};

static lv_obj_t *screen;

static void ui_screen_init() {
	screen = lv_scr_act();
}

static void ui_nav_push_current(void) {
	if(s_current_page.page_id == PAGE_INIT || s_current_page.page_id == PAGE_NONE) {
		return;
	}

	if(s_current_page.page_id > PAGE_INIT && s_current_page.page_id < PAGE_NONE) {
		if(ui_nav.depth < UI_ARRAY_SIZE(ui_nav.stack)) {
			ui_nav.stack[ui_nav.depth++] = s_current_page;
		} else {
			LOG("导航栈已满，无法记录历史页面");
		}
	}
}



/**
 * @brief 切换当前页面
 * @param param 目标页面和页面参数
 * @return ESP_OK 成功，其他表示参数或状态有误
 */
static esp_err_t page_show(ui_page_nav_param_t param) {
    if(param.page_id < PAGE_INIT || param.page_id >= PAGE_NONE) {
        LOG("无效的页面 ID: %d", param.page_id);
        return ESP_ERR_INVALID_ARG;
    }
    
    if(screen == NULL) {
        LOG("当前没有活动屏幕");
        return ESP_ERR_INVALID_STATE;
    }
    if(param.page_id == s_current_page.page_id &&
       param.settings_item_id == s_current_page.settings_item_id) {
        LOG("页面已在显示: %d", param.page_id);
        return ESP_OK;
    }

	ui_actor_leave_current_page();
	ui_actor_clean_ops();
    lv_obj_clean(screen);

    switch(param.page_id) {
    case PAGE_INIT:
        page_init_show(screen);
        break;

    case PAGE_HOME:
        page_home_show(screen);
        break;
    case PAGE_SETTINGS_ITEM:
		page_settings_item_show(screen, param.settings_item_id);
        break;
    case PAGE_SETTINGS:
		page_settings_show(screen);
        break;
	case PAGE_MENU:
		page_menu_show(screen);
		break;
	case PAGE_TALK:
		page_talk_show(screen);
		break;
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
	s_current_page = param;
    return ESP_OK;
}

/**
 * @brief UI 初始化入口，默认展示自检页面并输出日志。
 */
static void ui_create(void *arg)
{
	ui_style_init();
	ui_screen_init();
	ui_nav_go((ui_page_nav_param_t) {
		.page_id = PAGE_INIT,
	});
	// ui_nav_go((ui_page_nav_param_t) {
	// 	.page_id = PAGE_SETTINGS_ITEM,
	// 	.settings_item_id = SETTINGS_ITEM_ID_WS,
	// });

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


void ui_nav_back(void) {
	if(ui_nav.depth > 0) {
		ui_page_nav_param_t param = ui_nav.stack[--ui_nav.depth];
		page_show(param);
	} else {
		LOG("导航栈已空，无法返回");
	}
}

void ui_nav_go(ui_page_nav_param_t param) {
	ui_nav_push_current();
	page_show(param);
}

void ui_nav_back_action(const settings_sub_item_t *item)
{
    (void)item;
    ui_nav_back();
}



ui_page_id_t ui_get_current_page(void)
{
    return s_current_page.page_id;
}
