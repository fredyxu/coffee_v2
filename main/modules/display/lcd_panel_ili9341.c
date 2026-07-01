#include "lcd_i80_8_panel.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config/config_sys.h"

esp_err_t lcd_panel_ili9341_init(void)
{
    lcd_i80_panel_write_cmd(LCD_CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    {
        const uint8_t data_cf[] = {0x00, 0xC1, 0x30};
        lcd_i80_panel_write_data(0xCF, data_cf, sizeof(data_cf));
        const uint8_t data_ed[] = {0x64, 0x03, 0x12, 0x81};
        lcd_i80_panel_write_data(0xED, data_ed, sizeof(data_ed));
        const uint8_t data_e8[] = {0x85, 0x00, 0x78};
        lcd_i80_panel_write_data(0xE8, data_e8, sizeof(data_e8));
        const uint8_t data_cb[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
        lcd_i80_panel_write_data(0xCB, data_cb, sizeof(data_cb));
        const uint8_t data_f7[] = {0x20};
        lcd_i80_panel_write_data(0xF7, data_f7, sizeof(data_f7));
        const uint8_t data_ea[] = {0x00, 0x00};
        lcd_i80_panel_write_data(0xEA, data_ea, sizeof(data_ea));
        const uint8_t data_c0[] = {0x23};
        lcd_i80_panel_write_data(0xC0, data_c0, sizeof(data_c0));
        const uint8_t data_c1[] = {0x10};
        lcd_i80_panel_write_data(0xC1, data_c1, sizeof(data_c1));
        const uint8_t data_c5[] = {0x3E, 0x28};
        lcd_i80_panel_write_data(0xC5, data_c5, sizeof(data_c5));
        const uint8_t data_c7[] = {0x86};
        lcd_i80_panel_write_data(0xC7, data_c7, sizeof(data_c7));
    }

    lcd_i80_panel_apply_landscape_dir();

    {
        const uint8_t data_3a[] = {0x55};
        lcd_i80_panel_write_data(LCD_CMD_COLMOD, data_3a, sizeof(data_3a));
    }

    {
        const uint8_t data_b1[] = {0x00, 0x18};
        lcd_i80_panel_write_data(0xB1, data_b1, sizeof(data_b1));
        const uint8_t data_b6[] = {0x08, 0x82, 0x27};
        lcd_i80_panel_write_data(0xB6, data_b6, sizeof(data_b6));
    }

    if(CONFIG_LCD_INVERT_COLOR) {
        lcd_i80_panel_write_cmd(LCD_CMD_INVON);
    } else {
        lcd_i80_panel_write_cmd(LCD_CMD_INVOFF);
    }

    lcd_i80_panel_write_cmd(LCD_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_i80_panel_write_cmd(LCD_CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));
    return ESP_OK;
}
