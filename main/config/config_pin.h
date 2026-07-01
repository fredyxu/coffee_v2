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

#define PIN_KEY_A              1    // KEY_A / K1 / A / PADDLE_A / DOT
#define PIN_KEY_B              2    // KEY_B / K2 / B / PADDLE_B / DASH


/* ============================================================
 * 📺 LCD 控制引脚（I80接口）
 * ============================================================ */
#define PIN_LCD_DC                 9    // DC / RS / A0 / CD / D-C
#define PIN_LCD_WR                 8    // WR / WRX / W/R / LCD_WR / PCLK
#define PIN_LCD_CS                 10   // CS / CSX / LCD_CS / CE / SS
#define PIN_LCD_RST                21   // RST / RES / RESET / LCD_RST
#define PIN_LCD_BL                 11   // BL / BLK / BKL / LED / LCD_BL

// ESP 侧 8-bit I80 数据总线。转接板把这 8 条线接到 LCD 的高八位 DB8-DB15。
#define PIN_LCD_D0                 4    // LCD DB8
#define PIN_LCD_D1                 5    // LCD DB9
#define PIN_LCD_D2                 6    // LCD DB10
#define PIN_LCD_D3                 7    // LCD DB11
#define PIN_LCD_D4                 12   // LCD DB12
#define PIN_LCD_D5                 13   // LCD DB13
#define PIN_LCD_D6                 14   // LCD DB14
#define PIN_LCD_D7                 15   // LCD DB15


/* ============================================================
 * 🖐️ 触控（I2C）
 * ============================================================ */
#define PIN_TOUCH_SDA              18   // SDA / TP_SDA / T_SDA / CTP_SDA / TSDI
#define PIN_TOUCH_SCL              47   // SCL / SCK / TP_SCL / T_SCL / CTP_SCL /TCLK
#define PIN_TOUCH_INT              17   // INT / IRQ / TP_INT / T_IRQ / CTP_INT / TPEN
#define PIN_TOUCH_RST              16   // RST / RES / RESET / TP_RST / CTP_RST / TCS


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

#define PIN_ENCODER_A              38   // A / CLK / S1 / EC11_A / ROT_A
#define PIN_ENCODER_B              39   // B / DT / S2 / EC11_B / ROT_B
#define PIN_ENCODER_SW             40   // SW / KEY / BTN / EC11_SW / ROT_SW


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

#define PIN_SOUND_I2S_BCLK         41   // BCLK / BCK / SCK / I2S_BCLK
#define PIN_SOUND_I2S_WS           42   // LRC / LRCLK / WS / WSEL / I2S_WS
#define PIN_SOUND_I2S_DOUT         19   // DIN / SDIN / DATA / I2S_DIN

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

#define PIN_MIC_I2S_BCLK           35   // SCK / BCLK / BCK / I2S_SCK
#define PIN_MIC_I2S_WS             36   // WS / LRCLK / LRC / I2S_WS
#define PIN_MIC_I2S_DIN            37   // SD / DOUT / DATA / I2S_SD

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
