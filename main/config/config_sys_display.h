#pragma once

/* ============================================================
 * 显示系统配置
 * ============================================================ */

/* 屏幕分辨率 */
#define DISPLAY_H_RES 320
#define DISPLAY_V_RES 240

/* 面板像素字节数：RGB565 固定为 2 字节 */
#define DISPLAY_COLOR_BYTES_PER_PIXEL 2

/* LVGL 局部刷新缓冲行数：20~60 较稳；更大吞吐更高但占 RAM */
#define LVGL_DRAW_BUF_LINES 40

/* LCD 控制器：0=ILI9341, 1=ST7798 */
#define CONFIG_LCD_PANEL_CTRL 1

/* 颜色反相：1=反色, 0=正常 */
#define CONFIG_LCD_INVERT_COLOR 1
