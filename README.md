# ZClub Coffee

ZClub Coffee 是一个基于 ESP32-S3 和 LVGL 的电码练习通信终端，支持电键输入、摩斯码显示。可以通过WiFi联网实现模拟通信。



## 功能特性

- LVGL 图形界面
- 支持电码输入与聊天消息显示
- 支持手动键 / 自动键输入
- 电码转义显示
- WiFi 联网
- WebSocket 通信，发送和接收 CW 消息
- 支持触控和编码器操作
- 支持网络对讲

## 硬件 BOM

| 名称 | 数量 | 说明 |
| --- | --- | --- |
| ESP32-S3 开发板 | 1 | 主控 |
| I80 接口 LCD 屏 | 1 | 显示界面 |
| I2C 触摸屏 | 1 | 触摸输入 |
| EC11 旋转编码器 | 1 | 页面操作和首页焦点控制 |
| 电键接口 | 1 | A/B 两路输入 |
| MAX98357A I2S 音频模块 | 1 | 侧音输出 |
| INMP441 I2S 麦克风模块 | 1 | 麦克风输入 |
| 扬声器 | 1 | 接 MAX98357A 输出 |
| 杜邦线 / 排针 / 连接器 | 若干 | 接线使用 |
| 电源模块 / USB 供电 | 1 | 按实际开发板供电方式 |

## 接线说明

### 电键输入

| 功能 | ESP32-S3 GPIO | 说明 |
| --- | --- | --- |
| 电键 A | GPIO1 | GPIO -> 按键 -> GND |
| 电键 B | GPIO2 | GPIO -> 按键 -> GND |
| GND | GND | 公共地 |

说明：

- 电键输入使用内部上拉。
- 当前逻辑为接地触发。
- 自动键模式下 A/B 分别作为左右电键输入。
- 如果开启“左右电键互换”，A/B 逻辑会交换。

### LCD 显示屏，I80 接口

| LCD 信号 | ESP32-S3 GPIO |
| --- | --- |
| DC | GPIO9 |
| WR | GPIO8 |
| CS | GPIO10 |
| RST | GPIO21 |
| BL | GPIO11 |
| D0 | GPIO4 |
| D1 | GPIO5 |
| D2 | GPIO6 |
| D3 | GPIO7 |
| D4 | GPIO12 |
| D5 | GPIO13 |
| D6 | GPIO14 |
| D7 | GPIO15 |

说明：

- 当前固件使用 I80 8 位并口模式。
- 普通 8 位 I80 屏幕通常直接连接屏幕或转接板的 `D0-D7`。
- 有些 I80 转接板虽然工作在 8 位模式，但实际要求使用高 8 位数据线，此时需要将 ESP32-S3 的 `D0-D7` 连接到转接板的 `D8-D15`。
- 如果屏幕背光亮但画面异常、颜色错乱或无显示，需要确认转接板 8 位模式使用的是低 8 位还是高 8 位数据线。

### 触摸屏，I2C

| 触摸信号 | ESP32-S3 GPIO |
| --- | --- |
| SDA | GPIO18 |
| SCL | GPIO47 |
| INT | GPIO17 |
| RST | GPIO16 |

### EC11 旋转编码器

| 编码器信号 | ESP32-S3 GPIO | 说明 |
| --- | --- | --- |
| A | GPIO38 | A 相 |
| B | GPIO39 | B 相 |
| SW | GPIO40 | 按键 |
| GND | GND | 公共地 |

说明：

- 三脚一排中间接 GND，两边接 A/B。
- 如果旋转方向相反，交换 A/B 即可。
- 按键一端接 GPIO40，另一端接 GND。

### MAX98357A 音频输出

| MAX98357A 信号 | ESP32-S3 GPIO |
| --- | --- |
| BCLK | GPIO41 |
| LRC / WS | GPIO42 |
| DIN | GPIO19 |
| VIN | 按模块要求供电 |
| GND | GND |

### INMP441 麦克风输入

| INMP441 信号 | ESP32-S3 GPIO |
| --- | --- |
| SCK / BCLK | GPIO35 |
| WS | GPIO36 |
| SD | GPIO37 |
| L/R | GND |
| VDD | 按模块要求供电 |
| GND | GND |

说明：

- `L/R` 建议接 GND，使用左声道。
- 麦克风和音频输出使用独立 I2S 引脚。

## 设置说明

### WiFi 设置

| 设置项 | 说明 |
| --- | --- |
| 连接状态 | 显示当前 WiFi 是否已连接。 |
| 开启 WiFi | 开启或关闭 WiFi 功能。关闭后设备不会连接无线网络。 |
| 扫描 WiFi | 扫描附近可用的 WiFi。 |
| 选择 WiFi | 从扫描结果中选择要连接的 WiFi。 |
| WiFi 密码 | 输入当前选择 WiFi 的密码。 |
| 信号强度阈值 | 用于判断 WiFi 信号是否足够稳定。数值越接近 0，要求信号越强。 |
| 恢复默认 WiFi 设置 | 恢复 WiFi 相关默认配置。 |

### WebSocket 设置

| 设置项 | 说明 |
| --- | --- |
| 连接状态 | 显示 WebSocket 是否已连接服务器。 |
| 开启 WebSocket | 开启或关闭 WebSocket 通信。 |
| 服务器地址 | WebSocket 服务器地址。未手动设置时使用固件默认地址。 |
| 自动重连 | 连接断开后是否自动尝试重新连接。 |
| 重新连接 | 手动断开并重新连接 WebSocket 服务器。 |

### 电键设置

| 设置项 | 说明 |
| --- | --- |
| 电键模式 | 在手动键和自动键之间切换。手动键根据按下时间判断点/划；自动键按住后会连续输出点或划。 |
| 左右电键互换 | 交换 A/B 两个电键输入的含义。适合左右手习惯不同或接线相反的情况。 |
| 消抖时长 | 过滤机械按键抖动。数值太小可能误触发，数值太大可能导致按键响应变慢。 |
| 手动键点时长 | 手动键模式下，用来判断点和划的时间阈值。按下时间短于该值认为是点，长于该值认为是划。 |
| 手动键时长自适应 | 手动键模式下，系统根据用户输入节奏动态调整点/划判断。适合手速变化较大的情况。 |
| 自动键点时长 | 自动键模式下，一个点持续的时间。数值越小，自动键速度越快。 |
| 自动键划时长倍数 | 自动键模式下，一个划相当于点长度的多少倍。常见摩斯码规则中划约等于 3 个点长，也可以按习惯调整。 |
| 自动键连发间隔倍数 | 自动键连续输出点/划时，中间停顿相当于点长度的多少倍。数值越小，连发越紧凑。 |
| 恢复默认消抖时长 | 将消抖时长恢复为默认值。 |
| 恢复默认电键设置 | 将电键相关参数恢复到默认值。 |

### 音频设置

| 设置项 | 说明 |
| --- | --- |
| 侧音频率 | 设置电键输入时听到的提示音频率。频率越高声音越尖，频率越低声音越沉。 |
| 音量 | 设置侧音音量。 |
| 恢复默认音频设置 | 将音频相关参数恢复到默认值。 |

### 显示设置

| 设置项 | 说明 |
| --- | --- |
| 屏幕亮度 | 调整屏幕背光亮度。 |
| 恢复默认显示设置 | 将显示相关参数恢复到默认值。 |

### 摩斯码设置

| 设置项 | 说明 |
| --- | --- |
| 电码转义显示 | 开启后，界面会在电码后显示识别出的字母或数字。无法识别的电码显示为 `*`。 |
| 自动发送 | 开启后，停止输入一段时间会自动发送当前电码。 |
| 自动发送等待时间 | 自动发送前需要等待的空闲时间。数值越小，发送越快；数值越大，留给继续输入的时间越长。 |

### 用户信息设置

| 设置项 | 说明 |
| --- | --- |
| 用户呼号 | 设置发送消息时使用的呼号。默认呼号为 `ZClubUser`。 |

## 首页操作

普通模式：

- 旋转编码器：在聊天信息框、输入框、发送按钮之间切换焦点
- 单击聊天信息框：进入 / 退出聊天滚动模式
- 单击输入框：进入 / 退出输入编辑模式
- 单击发送按钮：发送当前电码

聊天滚动模式：

- 旋转编码器：上下滚动聊天记录
- 再次单击：退出聊天滚动模式

输入编辑模式：

- 顺时针旋转：删除最后一组电码
- 逆时针旋转：恢复最近删除的一组
- 再次单击：退出输入编辑模式

## 固件刷写

仓库的 `release/latest/` 目录提供可直接刷写的固件文件：

```text
release/latest/
  bootloader.bin
  partition-table.bin
  coffee.bin
  flash_args
  flasher_args.json
```

### macOS 命令行刷写

先查看串口：

```bash
ls /dev/cu.*
```

刷写：

```bash
python -m esptool \
  --chip esp32s3 \
  -p /dev/cu.usbmodemXXXX \
  -b 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode dio \
  --flash_size 16MB \
  --flash_freq 80m \
  0x0 release/latest/bootloader.bin \
  0x8000 release/latest/partition-table.bin \
  0x10000 release/latest/coffee.bin
```

将 `/dev/cu.usbmodemXXXX` 替换为实际串口。

### Linux 命令行刷写

先查看串口：

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

刷写：

```bash
python -m esptool \
  --chip esp32s3 \
  -p /dev/ttyUSB0 \
  -b 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode dio \
  --flash_size 16MB \
  --flash_freq 80m \
  0x0 release/latest/bootloader.bin \
  0x8000 release/latest/partition-table.bin \
  0x10000 release/latest/coffee.bin
```

如果 Linux 串口没有权限，可将当前用户加入 `dialout` 组后重新登录：

```bash
sudo usermod -aG dialout $USER
```

### Windows PowerShell 命令行刷写

先在设备管理器中查看串口号，例如 `COM5`。

刷写：

```powershell
py -m esptool `
  --chip esp32s3 `
  -p COM5 `
  -b 460800 `
  --before default_reset `
  --after hard_reset `
  write_flash `
  --flash_mode dio `
  --flash_size 16MB `
  --flash_freq 80m `
  0x0 release/latest/bootloader.bin `
  0x8000 release/latest/partition-table.bin `
  0x10000 release/latest/coffee.bin
```

如果 `py` 不可用，也可以使用 `python -m esptool`。

### 使用 Flash Download Tool 图形化刷写

Espressif 官方 Flash Download Tool 文档：

https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html

选择：

```text
ChipType: ESP32-S3
WorkMode: Develop
LoadMode: UART
```

烧录文件：

| Address | File |
| --- | --- |
| `0x0` | `release/latest/bootloader.bin` |
| `0x8000` | `release/latest/partition-table.bin` |
| `0x10000` | `release/latest/coffee.bin` |

Flash 参数：

| 参数 | 值 |
| --- | --- |
| SPI SPEED | `80MHz` |
| SPI MODE | `DIO` |
| Flash Size | `16MB` |
| BAUD | `460800` |

选择正确的 `COM` 口后，点击 `START` 开始烧录。


## 版本更新日志
### 0.2.0 
2026-06-27
- 新增 网络对讲中对房间列表的控制和房间成员的维护

2026-06-25
- 调整 设置菜单按钮修改为菜单页面入口，通过菜单页面进入不同的功能板块
- 新增 网络对讲功能