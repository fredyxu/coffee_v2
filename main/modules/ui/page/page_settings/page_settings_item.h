#pragma once

#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings.h"
#include "modules/ui/ui_actor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_FOCUS_ACTION_NONE = 0,
    SETTINGS_FOCUS_ACTION_ENTER,
    SETTINGS_FOCUS_ACTION_TOGGLE,
    SETTINGS_FOCUS_ACTION_SELECT,
    SETTINGS_FOCUS_ACTION_BACK,
} page_settings_focus_action_t;

typedef struct {
    settings_sub_item_id_t sub_item_id;
    settings_value_type_t value_type;
    lv_obj_t *obj;
    lv_obj_t *control;
    const settings_sub_item_t *item;
    void (*on_enter)(const settings_sub_item_t *item);
    void (*on_change)(const settings_sub_item_t *item, int step);
} page_settings_item_focus_item_t;

typedef enum {
	OP_MENU = 0,
	OP_SELECTED,
} op_status_type_t;


void page_settings_item_show(lv_obj_t *p, settings_item_id_t id);

#ifdef __cplusplus
}
#endif