#include "app_sim.h"

#include "modules/ui/page/page_init/page_init.h"

void app_sim_run(void)
{
    lv_obj_t *screen = lv_screen_active();
    if(screen == NULL) {
        return;
    }

    if(page_init_show(screen) != ESP_OK) {
        return;
    }

    add_init_msg("[INIT] LVGL READY");
    add_init_msg("[INIT] UI PAGE_INIT LOADED");
    add_init_msg("[INIT] SIMULATOR RUNNING");
}

