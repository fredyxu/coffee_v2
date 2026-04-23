#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t x;
    int16_t y;
    bool pressed;
} touch_ft6336u_point_t;

/**
 * @brief 初始化 FT6336U
 */
esp_err_t touch_ft6336u_init(void);

/**
 * @brief 反初始化 FT6336U
 */
esp_err_t touch_ft6336u_deinit(void);

/**
 * @brief 读取一个触摸点
 * @param point 输出坐标
 * @return true 读取流程成功（不代表一定按下）
 * @return false 通信失败
 */
bool touch_ft6336u_read_point(touch_ft6336u_point_t *point);

/**
 * @brief 设置坐标变换
 * @param swap_xy 交换 XY
 * @param mirror_x X 镜像
 * @param mirror_y Y 镜像
 */
void touch_ft6336u_set_transform(bool swap_xy, bool mirror_x, bool mirror_y);
void touch_ft6336u_set_offset(int16_t offset_x, int16_t offset_y);
void touch_ft6336u_set_resolution(int16_t h_res, int16_t v_res);
esp_err_t touch_ft6336u_set_orientation(uint8_t orientation, lv_display_t *disp);

/**
 * @brief 绑定到 LVGL
 */
esp_err_t touch_ft6336u_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev);

/* 兼容 main.c 现有调用方式 */
typedef touch_ft6336u_point_t touch_point_t;

static inline bool touch_read_point(touch_point_t *point)
{
    return touch_ft6336u_read_point(point);
}

static inline esp_err_t touch_init(void)
{
    return touch_ft6336u_init();
}

static inline esp_err_t touch_deinit(void)
{
    return touch_ft6336u_deinit();
}

static inline esp_err_t touch_lvgl_bind(lv_display_t *disp, lv_indev_t **out_indev)
{
    return touch_ft6336u_lvgl_bind(disp, out_indev);
}

#ifndef touch_set_transform
#define touch_set_transform touch_ft6336u_set_transform
#endif

#ifndef touch_set_offset
#define touch_set_offset touch_ft6336u_set_offset
#endif

#ifndef touch_set_resolution
#define touch_set_resolution touch_ft6336u_set_resolution
#endif

#ifndef touch_set_orientation
#define touch_set_orientation touch_ft6336u_set_orientation
#endif

#ifdef __cplusplus
}
#endif
