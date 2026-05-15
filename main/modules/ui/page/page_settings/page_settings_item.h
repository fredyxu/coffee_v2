#pragma once

#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	settings_sub_item_id_t sub_item_id;
	lv_obj_t *obj;
	void *data;
	void (*on_enter)(void *data);
} page_settings_item_focus_item_t;

typedef enum {
	OP_MENU = 0,
	OP_SELECTED,
} op_status_type_t;



void page_settings_item_show(lv_obj_t *p, settings_item_id_t id);

#ifdef __cplusplus
}
#endif