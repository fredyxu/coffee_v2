#pragma once

#include "lvgl.h"
#include "modules/ui/page/page_settings/page_settings_focus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PAGE_SETTINGS_BINDING_RESULT_NONE = 0,
	PAGE_SETTINGS_BINDING_RESULT_UNHANDLED,
	PAGE_SETTINGS_BINDING_RESULT_VALUE_CHANGED,
	PAGE_SETTINGS_BINDING_RESULT_EDIT_BEGIN,
	PAGE_SETTINGS_BINDING_RESULT_EDIT_APPLY,
	PAGE_SETTINGS_BINDING_RESULT_CMD_SENT,
	PAGE_SETTINGS_BINDING_RESULT_CMD_FAILED,
} page_settings_binding_result_t;

page_settings_binding_result_t page_settings_binding_rotate_current(int step);
page_settings_binding_result_t page_settings_binding_press(page_settings_item_focus_item_t *focus);
page_settings_binding_result_t page_settings_binding_handle_slider_event(lv_event_t *e);

#ifdef __cplusplus
}
#endif
