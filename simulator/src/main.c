#include "lvgl.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "app_sim.h"
#include <stdlib.h>

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#endif

static void place_window_on_display(lv_display_t *disp)
{
    const char *display_env = getenv("SIM_DISPLAY_INDEX");
    int display_index = (display_env != NULL) ? atoi(display_env) : 1;

#if defined(SDL_MAJOR_VERSION)
    struct SDL_Window *window = lv_sdl_window_get_window(disp);
    if(window == NULL) {
        return;
    }

    int display_count = SDL_GetNumVideoDisplays();
    if(display_count <= 0) {
        return;
    }
    if(display_index < 0 || display_index >= display_count) {
        display_index = 0;
    }

    SDL_Rect bounds;
    if(SDL_GetDisplayBounds(display_index, &bounds) != 0) {
        return;
    }
    SDL_SetWindowPosition(window, bounds.x + 60, bounds.y + 60);
#else
    (void)disp;
    (void)display_index;
#endif
}

int main(void)
{
    lv_init();

    lv_display_t *disp = lv_sdl_window_create(320, 240);
    if(disp == NULL) {
        return 1;
    }
    lv_sdl_window_set_title(disp, "Coffee Simulator");
    place_window_on_display(disp);

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
