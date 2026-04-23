#include "test_lcd.h"
#include "config/config_sys.h"
#include "modules/display/lcd_i80_8.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/utils/log.h"


// LCD简单测试 - 显示全红屏幕
void test_lcd_color(void)
{
	LOG("LCD TEST START");
    esp_lcd_panel_io_handle_t io = lcd_get_io();
    if(io == NULL) return;

    uint16_t line[DISPLAY_H_RES];
    for(int i = 0; i < DISPLAY_H_RES; i++) {
        line[i] = 0xF800;  // 红色
    }

    for(int y = 0; y < DISPLAY_V_RES; y++) {
        esp_lcd_panel_io_tx_color(io, 0x2C, line, sizeof(line));
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}