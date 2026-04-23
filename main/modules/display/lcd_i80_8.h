#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD初始化 - 只做硬件初始化
esp_err_t lcd_i80_8_init(void);

// 背光控制
esp_err_t lcd_set_backlight(uint8_t percent);

// 获取io handle供LVGL使用
esp_lcd_panel_io_handle_t lcd_get_io(void);

// 当前 i80 总线允许的单次最大传输字节数
size_t lcd_get_max_transfer_bytes(void);

// LCD测试 - 纯LCD绘制测试
void lcd_test_color(void);

#ifdef __cplusplus
}
#endif
