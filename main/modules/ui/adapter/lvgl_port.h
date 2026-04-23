#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

// SemaphoreHandle_t lvgl_port_get_flush_sem(void);

esp_err_t lvgl_port_init(esp_lcd_panel_io_handle_t io);


void lvgl_port_lock(void);
void lvgl_port_unlock(void);
void lvgl_port_run(void (*cb)(void *), void *arg);

lv_display_t *lvgl_port_get_display(void);

#ifdef __cplusplus
}
#endif
