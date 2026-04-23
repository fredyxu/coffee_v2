#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t x;
    int16_t y;
    bool pressed;
} touch_ft6336u_point_t;

typedef touch_ft6336u_point_t touch_point_t;

esp_err_t touch_init(void);
esp_err_t touch_deinit(void);
esp_err_t touch_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev);
esp_err_t touch_set_orientation(uint8_t orientation, lv_display_t *disp);
bool touch_read_point(touch_point_t *point);

lv_indev_t *touch_get_indev(void);

/* Touch pin mapping source of truth (configured in config/config_pin.h via touch.c). */
extern const int g_touch_pin_sda;
extern const int g_touch_pin_scl;
extern const int g_touch_pin_int;
extern const int g_touch_pin_rst;

#ifdef __cplusplus
}
#endif
