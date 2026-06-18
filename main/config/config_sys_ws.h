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

/* 断线重连退避时间。 */
#define WS_RECONNECT_MIN_MS 1000
#define WS_RECONNECT_MAX_MS 30000

/* WebSocket 空闲心跳。 */
#define WS_HEARTBEAT_INTERVAL_MS 30000
#define WS_HEARTBEAT_PAYLOAD "{\"type\":\"ping\"}"

/* WebSocket 发送等待锁的最长时间。 */
#define WS_SEND_TIMEOUT_MS 2000

/* WebSocket actor 任务配置。 */
#define WS_ACTOR_QUEUE_LEN 16
#define WS_ACTOR_TASK_STACK 6144
