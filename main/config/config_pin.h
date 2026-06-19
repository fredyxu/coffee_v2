#pragma once

/* ============================================================
 * 摩尔斯电键 A/B 输入
 * ============================================================ */
/*
GPIO1 -> 电键 A
GPIO2 -> 电键 B

接法：
GPIO -> 按键 -> GND
启用内部上拉
*/

#define PIN_KEY_A              1
#define PIN_KEY_B              2


/* ============================================================
 * 📺 LCD 控制引脚（I80接口）
 * ============================================================ */
#define PIN_LCD_DC                 9
#define PIN_LCD_WR                 8
#define PIN_LCD_CS                 10
#define PIN_LCD_RST                21
#define PIN_LCD_BL                 11

// LCD 数据总线引脚 D0-D7
#define PIN_LCD_D0                 4
#define PIN_LCD_D1                 5
#define PIN_LCD_D2                 6
#define PIN_LCD_D3                 7
#define PIN_LCD_D4                 12
#define PIN_LCD_D5                 13
#define PIN_LCD_D6                 14
#define PIN_LCD_D7                 15


/* ============================================================
 * 🖐️ 触控（I2C）
 * ============================================================ */
#define PIN_TOUCH_SDA              18
#define PIN_TOUCH_SCL              47
#define PIN_TOUCH_INT              17
#define PIN_TOUCH_RST              16


/* ============================================================
 * 🎛️ EC11 旋转编码器（无丝印 → 按结构说明）
 * ============================================================ */
/*
EC11 引脚说明：

三脚一排（信号）：
    中间 → GND
    两边 → A / B（顺序影响方向）

两脚一排（按键）：
    一个 → GND
    一个 → SW

注意：
- 旋转方向反了只需交换 A/B
*/

#define PIN_ENCODER_A              38   // EC11：A相（旋转信号）
#define PIN_ENCODER_B              39   // EC11：B相（旋转信号）
#define PIN_ENCODER_SW             40   // EC11：按键（SW）


/* ============================================================
 * 🔊 MAX98357A（I2S 音频输出）
 * ============================================================ */
/*
模块丝印：
LRC  → LRCLK / WS
BCLK → Bit Clock
DIN  → 数据输入（接ESP32输出）
GAIN → 增益设置（可悬空或接GND/VDD）
SD   → Shutdown（拉高使能）
VIN  → 电源
GND  → 地
*/

#define PIN_SOUND_I2S_BCLK         41   // → BCLK
#define PIN_SOUND_I2S_WS           42   // → LRC (WS / LRCLK)
#define PIN_SOUND_I2S_DOUT         19   // → DIN

// I2S 配置（输出）
// 采样率 / DMA / 任务优先级等系统参数请配置在 config_sys.h


/* ============================================================
 * 🎤 INMP441（I2S 麦克风输入）
 * ============================================================ */
/*
模块丝印：
L/R → 声道选择
WS  → LRCLK
SCK → BCLK
SD  → 数据输出（接ESP32输入）
VDD → 电源
GND → 地

接线建议：
- L/R → GND（左声道，推荐）
*/

#define PIN_MIC_I2S_BCLK           35   // → SCK
#define PIN_MIC_I2S_WS             36   // → WS
#define PIN_MIC_I2S_DIN            37   // → SD

/* ============================================================
 * ⚠️ GPIO 能力说明（ESP32-S3）
 * ============================================================ */
/*
GPIO 34~39:
    ✔ 输入专用（非常适合麦克风/编码器）

GPIO 40+:
    ✔ 高速IO（适合I2S/LCD）

当前设计：
✔ 编码器 → 38/39（最佳）
✔ Mic → 独立I2S（无干扰）
✔ Sound → 独立I2S（稳定）
*/


/* ============================================================
 * 🔮 预留扩展
 * ============================================================ */

// #define PIN_AUDIO_PA_EN            -1

// #define PIN_SD_MOSI                -1
// #define PIN_SD_MISO                -1
// #define PIN_SD_CLK                 -1
// #define PIN_SD_CS                  -1

// #define PIN_STATUS_LED             -1
