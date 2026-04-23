#include "touch.h"
#include "config/config_pin.h"

extern esp_err_t touch_ft6336u_init(void);
extern esp_err_t touch_ft6336u_deinit(void);
extern esp_err_t touch_ft6336u_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev);
extern esp_err_t touch_ft6336u_set_orientation(uint8_t orientation, lv_display_t *disp);
extern bool touch_ft6336u_read_point(touch_ft6336u_point_t *point);

const int g_touch_pin_sda = PIN_TOUCH_SDA;
const int g_touch_pin_scl = PIN_TOUCH_SCL;
const int g_touch_pin_int = PIN_TOUCH_INT;
const int g_touch_pin_rst = PIN_TOUCH_RST;

static lv_indev_t *s_indev = NULL;

esp_err_t touch_init(void)
{
    return touch_ft6336u_init();
}

esp_err_t touch_deinit(void)
{
    esp_err_t err = touch_ft6336u_deinit();
    if(err == ESP_OK) {
        s_indev = NULL;
    }
    return err;
}

esp_err_t touch_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev)
{
    esp_err_t err = touch_ft6336u_lvgl_bind(disp, out_indev);
    if(err == ESP_OK && out_indev && *out_indev) {
        s_indev = *out_indev;
    }
    return err;
}

esp_err_t touch_set_orientation(uint8_t orientation, lv_display_t *disp)
{
    return touch_ft6336u_set_orientation(orientation, disp);
}

bool touch_read_point(touch_point_t *point)
{
    return touch_ft6336u_read_point(point);
}

lv_indev_t *touch_get_indev(void)
{
    return s_indev;
}
