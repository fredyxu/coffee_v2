#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_CMD_SWRESET            0x01
#define LCD_CMD_SLPOUT             0x11
#define LCD_CMD_DISPON             0x29
#define LCD_CMD_CASET              0x2A
#define LCD_CMD_PASET              0x2B
#define LCD_CMD_RAMWR              0x2C
#define LCD_CMD_MADCTL             0x36
#define LCD_CMD_COLMOD             0x3A
#define LCD_CMD_INVON              0x21
#define LCD_CMD_INVOFF             0x20

esp_err_t lcd_panel_st7798_init(void);
esp_err_t lcd_panel_ili9341_init(void);

void lcd_i80_panel_write_cmd(uint8_t cmd);
void lcd_i80_panel_write_data(uint8_t cmd, const uint8_t *data, size_t len);
void lcd_i80_panel_apply_landscape_dir(void);
void lcd_i80_panel_apply_madctl(uint8_t normal, uint8_t reverse);

#ifdef __cplusplus
}
#endif
