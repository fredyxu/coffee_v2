#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "app_sim.h"

int main(void)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(320, 240);
    if(disp == NULL) {
        return 1;
    }
    lv_sdl_window_set_title(disp, "Coffee Simulator");

    lv_indev_t *mouse = lv_sdl_mouse_create();
    if(mouse) {
        lv_indev_set_display(mouse, disp);
    }

    lv_indev_t *keyboard = lv_sdl_keyboard_create();
    if(keyboard) {
        lv_indev_set_display(keyboard, disp);
    }

    app_sim_run();

    while(1) {
        lv_timer_handler();
        lv_delay_ms(5);
    }

    return 0;
}
