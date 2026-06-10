#pragma once

/* 1=开发模式(开启日志), 0=生产模式(关闭日志) */
#define CONFIG_APP_MODE 1

/* 固件版本号，上报给远程 WebSocket 服务用于识别客户端能力。 */
#define APP_FIRMWARE_VERSION "0.1.0"

/* 触摸 I2C 频率（Hz） */
#define TOUCH_I2C_FREQ_HZ 400000

/* 聚合子配置 */
#include "config/config_sys_display.h"	// IWYU pragma: export
#include "config/config_sys_audio.h"	// IWYU pragma: export
#include "config/config_sys_task.h"		// IWYU pragma: export
#include "config/config_sys_wifi.h"		// IWYU pragma: export
#include "config/config_sys_ws.h"		// IWYU pragma: export
#include "config/config_ui.h"			// IWYU pragma: export
