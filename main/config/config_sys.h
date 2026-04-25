#pragma once

/* 1=开发模式(开启日志), 0=生产模式(关闭日志) */
#define CONFIG_APP_MODE 1

/* 触摸 I2C 频率（Hz） */
#define TOUCH_I2C_FREQ_HZ 400000

/* 聚合子配置 */
#include "config/config_sys_display.h"
#include "config/config_sys_audio.h"
#include "config/config_sys_task.h"
#include "config/config_ui.h"