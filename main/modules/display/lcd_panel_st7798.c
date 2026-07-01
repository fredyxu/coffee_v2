#include "lcd_i80_8_panel.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config/config_sys.h"

esp_err_t lcd_panel_st7798_init(void)
{
    lcd_i80_panel_write_cmd(LCD_CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    {
        const uint8_t f0_c3[] = {0xC3};
        const uint8_t f0_96[] = {0x96};
        lcd_i80_panel_write_data(0xF0, f0_c3, sizeof(f0_c3));
        lcd_i80_panel_write_data(0xF0, f0_96, sizeof(f0_96));
    }

    {
        const uint8_t b4[] = {0x01};
        const uint8_t b7[] = {0xC6};
        const uint8_t c1[] = {0x06};
        const uint8_t c2[] = {0xA7};
        const uint8_t c5[] = {0x18};
        lcd_i80_panel_write_data(0xB4, b4, sizeof(b4));
        lcd_i80_panel_write_data(0xB7, b7, sizeof(b7));
        lcd_i80_panel_write_data(0xC1, c1, sizeof(c1));
        lcd_i80_panel_write_data(0xC2, c2, sizeof(c2));
        lcd_i80_panel_write_data(0xC5, c5, sizeof(c5));
    }

    {
        const uint8_t e8[] = {0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33};
        lcd_i80_panel_write_data(0xE8, e8, sizeof(e8));
    }

    lcd_i80_panel_apply_landscape_dir();

    {
        const uint8_t colmod[] = {0x55};
        lcd_i80_panel_write_data(LCD_CMD_COLMOD, colmod, sizeof(colmod));
    }

    {
        const uint8_t e0[] = {
            0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54,
            0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B
        };
        const uint8_t e1[] = {
            0xE0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2B, 0x43,
            0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B
        };
        lcd_i80_panel_write_data(0xE0, e0, sizeof(e0));
        lcd_i80_panel_write_data(0xE1, e1, sizeof(e1));
    }

    if(CONFIG_LCD_INVERT_COLOR) {
        lcd_i80_panel_write_cmd(LCD_CMD_INVON);
    } else {
        lcd_i80_panel_write_cmd(LCD_CMD_INVOFF);
    }

    {
        const uint8_t f0_3c[] = {0x3C};
        const uint8_t f0_69[] = {0x69};
        lcd_i80_panel_write_data(0xF0, f0_3c, sizeof(f0_3c));
        lcd_i80_panel_write_data(0xF0, f0_69, sizeof(f0_69));
    }

    lcd_i80_panel_write_cmd(LCD_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_i80_panel_write_cmd(LCD_CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}
