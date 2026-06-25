#pragma once

/* ============================================================
 * 显示系统配置
 * ============================================================ */

/* 屏幕分辨率 */
#define DISPLAY_H_RES 320
#define DISPLAY_V_RES 240

/* 面板像素字节数：RGB565 固定为 2 字节 */
#define DISPLAY_COLOR_BYTES_PER_PIXEL 2

/* LVGL 局部刷新缓冲行数：较小的缓冲释放内部 DMA RAM，给 TLS/AES 写入留空间。 */
#define LVGL_DRAW_BUF_LINES 20

/* LCD 控制器：0=ILI9341, 1=ST7798 */
#define CONFIG_LCD_PANEL_CTRL 1

/* 颜色反相：1=反色, 0=正常 */
#define CONFIG_LCD_INVERT_COLOR 1

