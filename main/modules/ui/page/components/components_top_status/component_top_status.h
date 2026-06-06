#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif



void ui_top_status_set_wifi_signal(int level);

esp_err_t ui_add_top_status(lv_obj_t *p);
void ui_top_status_ref_icon();
void ui_top_status_handle_sys_msg(const msg_t *msg);

#ifdef __cplusplus
}
#endif
