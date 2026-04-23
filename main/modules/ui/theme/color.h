#pragma once

#include "lvgl.h"

/**
 * @brief 全局色彩宏：统一视觉基调，便于全界面一致地调用。
 */
#define UI_COLOR_BG					lv_color_hex(0x0B1220)  /* 主背景 */
// #define UI_COLOR_BG					lv_color_hex(0x123456)  /* 主背景 */
#define UI_COLOR_BG_SECONDARY		lv_color_hex(0x111A2F)  /* 卡片 / 面板 */
#define UI_COLOR_TEXT       		lv_color_hex(0xFFFFFF)  /* 主要文字 */
#define UI_COLOR_TEXT_1           	lv_color_hex(0x9AB9F7)  /* 说明文字 */
#define UI_COLOR_ACCENT				lv_color_hex(0x26DFE6)  /* 成功 / 完成状态 */
#define UI_COLOR_WARN				lv_color_hex(0xFFA029)  /* 运行 / 警示 */
#define UI_COLOR_BORDER				lv_color_hex(0x2F8CFF)  /* 边框/高亮轮廓 */
#define UI_COLOR_BUTTON_START		lv_color_hex(0x1D2D4C)  /* 按钮渐变起始色 */
#define UI_COLOR_BUTTON_END			lv_color_hex(0x2B3B6A)  /* 按钮渐变结束色 */