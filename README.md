# ZClub - Coffee V2 (ESP32-S3)

一个基于 **ESP-IDF 5.5.x** 的嵌入式开源项目，包含：
- LCD + 触控 + LVGL UI
- 旋转编码器输入
- 音频输出（MAX98357A, I2S）
- 麦克风输入（INMP441, I2S）
- 基于消息总线的 `INPUT / SYS / CMD` 架构

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
```

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

## 7. 默认关键引脚（节选）

以 `main/config/config_pin.h` 为准，下面仅列常用：

- LCD: `DC=9, WR=8, CS=10, RST=21, BL=11, D0..D7=4,5,6,7,12,13,14,15`
- Touch(I2C): `SDA=18, SCL=47, INT=17, RST=16`
- Encoder: `A=38, B=39, SW=40`
- Key: `KEY1=1, KEY2=2`
- Speaker(I2S): `BCLK=41, WS=42, DOUT=19`
- Mic(I2S): `BCLK=35, WS=36, DIN=37`

## 8. 常见问题

- 编译后新增文件未生效  
  执行：
  ```bash
  idf.py fullclean build
  ```
  （当前工程使用 `file(GLOB ...)`，新增源文件后建议 fullclean 一次）

- 启动后 UI 没变化  
  先看日志是否有 `con/ui_actor/state` 的事件链路输出，再确认 `CMD_UI_*` 是否有对应处理。

---

