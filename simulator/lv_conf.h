#ifndef LV_CONF_H
#define LV_CONF_H

/* Minimal config for desktop simulator */

#define LV_USE_OS LV_OS_NONE

#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 16
#define LV_DPI_DEF 130

#define LV_USE_SDL 1
#if LV_USE_SDL
    #define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
    #define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
    #define LV_SDL_BUF_COUNT 1
    #define LV_SDL_ACCELERATED 1
    #define LV_SDL_FULLSCREEN 0
    #define LV_SDL_DIRECT_EXIT 1
#endif

#endif /* LV_CONF_H */
