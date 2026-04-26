#pragma once

/* ============================================================
 * WiFi 系统配置（STA）
 * ============================================================ */

/* 默认 STA 账号：为空字符串表示仅初始化，不自动发起连接 */
#define WIFI_STA_SSID "wks-2.4"
#define WIFI_STA_PASSWORD "66668888"

/* 断线自动重连：1=开启, 0=关闭 */
#define WIFI_AUTO_RECONNECT 1

/* 弱信号阈值（dBm），例如 -75/-80 */
#define WIFI_WEAK_RSSI_THRESHOLD -75

