# ZClub - Coffee V2 (ESP32-S3)

一个基于 **ESP-IDF 5.5.x** 的嵌入式开源项目

此项目为Coffee项目的再次重启，使用的硬件有所调整。增加了麦克风，计划扩展网络对讲功能。


## 0. 项目进度

已完成：
- [x] 核心消息链路
- [x] 启动阶段初始化页面
- [x] 本地存储模块
- [x] WIFI模块


下一步计划：
- [ ] 首页

## 1. 功能概览

- 设备启动初始化（LCD、触控、LVGL、输入/音频模块）
- `state` 统一做输入和系统事件决策
- `con` 负责消息汇聚与命令分发
- `ui_actor` / `audio_actor` 负责执行命令

## 2. 快速开始

### 2.1 环境要求

- ESP-IDF `>= 5.5.4`
- Python 环境可正常执行 `idf.py`

### 2.2 编译

在项目根目录执行：

```bash
idf.py build
```

### 2.3 烧录与串口

```bash
idf.py -p <PORT> flash monitor
```

例如：

```bash
idf.py -p /dev/tty.usbmodemXXX flash monitor
```

## 3. 目录说明

```text
main/
  app/            启动初始化
  core/           con / state / msg 等核心模块
  modules/        ui / input / audio / mic / display
  config/         引脚与系统参数
doc/              项目文档与每日更新日志
```

### 3.1 文档入口

- 文档目录：[doc/README.md](doc/README.md)
- 每日更新日志：[doc/daily/2026-04-26.md](doc/daily/2026-04-26.md)

## 4. 消息架构（简版）

- `MSG_TYPE_INPUT`：用户输入事件（按键、编码器等）
- `MSG_TYPE_SYS`：系统事件（初始化完成、WiFi 状态等）
- `MSG_TYPE_CMD`：决策层输出给 actor 的执行命令

典型链路：

```text
input/sys -> con -> state -> cmd -> ui/audio actor
```

## 5. 引脚配置位置

主要引脚定义在：

- `main/config/config_pin.h`

显示与音频参数在：

- `main/config/config_sys_display.h`
- `main/config/config_sys_audio.h`

如果你的硬件接线不同，优先修改这几个配置文件。

## 6. BOM

> 下面是当前代码默认配置对应的一套参考 BOM。  
> 不同厂商型号可替换，但接口与电气参数要匹配。

| 类别 | 器件 | 数量 | 说明 |
|---|---|---:|---|
| 主控 | ESP32-S3 开发板（带 USB） | 1 | 需有足够 GPIO，建议支持 PSRAM |
| 显示 | 320x240 TFT LCD（I80 并口，代码默认 ST7798） | 1 | RGB565，8-bit 数据总线 |
| 触控 | 电容触控模块（FT6336U，I2C） | 1 | 与 LCD 配套触控面板常见 |
| 输入 | EC11 旋转编码器（带按键） | 1 | A/B + SW |
| 输入 | 轻触按键 | 2 | 用于 DIT/DAH（摩尔斯双键） |
| 音频输出 | MAX98357A（I2S DAC 功放） | 1 | 接无源喇叭 |
| 音频输出 | 喇叭（4Ω/3W 或同级） | 1 | 与功放匹配 |
| 麦克风 | INMP441（I2S 数字麦） | 1 | 单声道采集 |
| 供电 | 5V 电源/USB 供电 | 1 | 满足主控 + 屏幕 + 音频峰值电流 |
| 连接 | 杜邦线/焊接导线/排针 | 若干 | 连接各模块 |

## 7. 详细引脚接线表

以 `main/config/config_pin.h` 为准。下表为当前代码默认接线。

### 7.1 摩尔斯双键（DIT / DAH）

| ESP32-S3 GPIO | 外设引脚/端子 | 方向 | 说明 |
|---|---|---|---|
| GPIO1 | KEY1 (DIT) | 输入 | 接法：`GPIO -> 按键 -> GND`，使用内部上拉 |
| GPIO2 | KEY2 (DAH) | 输入 | 接法：`GPIO -> 按键 -> GND`，使用内部上拉 |

### 7.2 LCD（I80 并口，8-bit）

| ESP32-S3 GPIO | LCD 引脚 | 方向 | 说明 |
|---|---|---|---|
| GPIO9 | DC | 输出 | 数据/命令选择 |
| GPIO8 | WR | 输出 | 写时钟 |
| GPIO10 | CS | 输出 | 片选 |
| GPIO21 | RST | 输出 | 硬复位 |
| GPIO11 | BL | 输出 | 背光控制 |
| GPIO4 | D0 | 输出 | 数据总线 bit0 |
| GPIO5 | D1 | 输出 | 数据总线 bit1 |
| GPIO6 | D2 | 输出 | 数据总线 bit2 |
| GPIO7 | D3 | 输出 | 数据总线 bit3 |
| GPIO12 | D4 | 输出 | 数据总线 bit4 |
| GPIO13 | D5 | 输出 | 数据总线 bit5 |
| GPIO14 | D6 | 输出 | 数据总线 bit6 |
| GPIO15 | D7 | 输出 | 数据总线 bit7 |

### 7.3 触控 FT6336U（I2C）

| ESP32-S3 GPIO | 触控引脚 | 方向 | 说明 |
|---|---|---|---|
| GPIO18 | SDA | 双向 | I2C 数据线 |
| GPIO47 | SCL | 输出 | I2C 时钟线 |
| GPIO17 | INT | 输入 | 触控中断输入 |
| GPIO16 | RST | 输出 | 触控复位 |

### 7.4 EC11 旋转编码器

| ESP32-S3 GPIO | 编码器引脚 | 方向 | 说明 |
|---|---|---|---|
| GPIO38 | A | 输入 | 旋转 A 相 |
| GPIO39 | B | 输入 | 旋转 B 相 |
| GPIO40 | SW | 输入 | 按键输入 |
| GND | C/COM | - | 编码器公共端接地 |

### 7.5 MAX98357A（I2S 音频输出）

| ESP32-S3 GPIO | MAX98357A 引脚 | 方向 | 说明 |
|---|---|---|---|
| GPIO41 | BCLK | 输出 | I2S 位时钟 |
| GPIO42 | LRC / WS | 输出 | I2S 声道时钟 |
| GPIO19 | DIN | 输出 | I2S 数据输出到功放 |
| 5V / 3V3 | VIN | - | 模块供电（按模块规格） |
| GND | GND | - | 电源地 |

### 7.6 INMP441（I2S 麦克风输入）

| ESP32-S3 GPIO | INMP441 引脚 | 方向 | 说明 |
|---|---|---|---|
| GPIO35 | SCK / BCLK | 输出 | I2S 位时钟 |
| GPIO36 | WS | 输出 | I2S 声道时钟 |
| GPIO37 | SD | 输入 | I2S 数据输入（麦克风输出） |
| GND | L/R | - | 建议接 GND（左声道） |
| 3V3 | VDD | - | 模块供电 |
| GND | GND | - | 电源地 |

### 7.7 预留引脚（当前未启用）

| 名称 | 配置值 | 说明 |
|---|---:|---|
| `PIN_AUDIO_PA_EN` | -1 | 功放使能引脚预留 |
| `PIN_SD_MOSI` | -1 | SD 卡 SPI 预留 |
| `PIN_SD_MISO` | -1 | SD 卡 SPI 预留 |
| `PIN_SD_CLK` | -1 | SD 卡 SPI 预留 |
| `PIN_SD_CS` | -1 | SD 卡 SPI 预留 |
| `PIN_STATUS_LED` | -1 | 状态灯预留 |

---
