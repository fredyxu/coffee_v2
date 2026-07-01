#pragma once

/* ============================================================
 * 显示系统配置。 以下配置为测试使用，如无特殊需要请勿更改。
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

/* 横屏方向：0=正向, 1=反向 */
#define CONFIG_LCD_LANDSCAPE_DIR 0

/* ILI9341 I80 时序档位：0=原始20MHz, 1=保守10MHz */
#define CONFIG_LCD_ILI9341_TIMING_PROFILE 0

/* 颜色反相：1=反色, 0=正常 */
#define CONFIG_LCD_INVERT_COLOR 1

/* LCD 底层纯色自检：0=正常启动, 1=只显示 LCD 色条并停止后续 UI 初始化 */
#define CONFIG_LCD_COLOR_TEST_ONLY 0
