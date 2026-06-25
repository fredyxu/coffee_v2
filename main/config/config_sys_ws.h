#pragma once

/* ============================================================
 * WebSocket 系统配置
 * ============================================================ */

#define WS_DEFAULT_ENABLE 1
#define WS_DEFAULT_URL "wss://coffee.zclub.cool/ws"
#define WS_DEFAULT_ROOM "default"
#define WS_DEFAULT_CALLSIGN ""
#define USER_DEFAULT_CALLSIGN "ZClubUser"
#define WS_DEFAULT_AUTO_RECONNECT 1
#define WS_DEFAULT_TOKEN "zclub-coffee-v2"

#define WS_DEVICE_ID_PREFIX "coffee-v2-"

/* TLS 校验模式。
 *
 * 默认使用 ESP-IDF full certificate bundle，普通公网 CA 签发的 WSS 证书
 * 不需要用户额外配置。特殊自建 CA 或证书链不兼容时，可以切到
 * WS_TLS_VERIFY_MODE_CUSTOM_CA 并提供 WS_CUSTOM_CA_PEM。
 */
#define WS_TLS_VERIFY_MODE_BUNDLE 0
#define WS_TLS_VERIFY_MODE_CUSTOM_CA 1
#define WS_TLS_VERIFY_MODE_INSECURE_DEV 2

#ifndef WS_TLS_VERIFY_MODE
#define WS_TLS_VERIFY_MODE WS_TLS_VERIFY_MODE_BUNDLE
#endif

/* PEM 格式 CA。仅 WS_TLS_VERIFY_MODE_CUSTOM_CA 使用。
 * 示例：
 * #define WS_CUSTOM_CA_PEM "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"
 */
#ifndef WS_CUSTOM_CA_PEM
#define WS_CUSTOM_CA_PEM NULL
#endif

/* 仅调试用。默认禁止跳过证书校验。 */
#ifndef WS_TLS_INSECURE_DEV_ENABLED
#define WS_TLS_INSECURE_DEV_ENABLED 0
#endif

/* 断线重连退避时间。 */
#define WS_RECONNECT_MIN_MS 1000
#define WS_RECONNECT_MAX_MS 30000

/* WebSocket 空闲心跳。 */
#define WS_HEARTBEAT_INTERVAL_MS 30000
#define WS_HEARTBEAT_PAYLOAD "{\"type\":\"ping\"}"

/* WebSocket 发送等待锁的最长时间。 */
#define WS_SEND_TIMEOUT_MS 2000

/* WebSocket 音频帧发送等待锁的最长时间。 */
#define WS_AUDIO_SEND_TIMEOUT_MS 1000

/* 连续音频发送失败达到该次数后，才认为 WebSocket 需要重连。 */
#define WS_AUDIO_SEND_MAX_CONSECUTIVE_FAILS 3

/* WebSocket 连接阶段网络超时。 */
#define WS_NETWORK_TIMEOUT_MS 5000

/* WebSocket actor 任务配置。 */
#define WS_ACTOR_QUEUE_LEN 16
#define WS_ACTOR_TASK_STACK 6144
