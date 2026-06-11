#pragma once

/* ============================================================
 * WebSocket 系统配置
 * ============================================================ */

#define WS_DEFAULT_ENABLE 1
#define WS_DEFAULT_URL "wss://coffee.veloria.asia/ws"
#define WS_DEFAULT_ROOM "default"
#define WS_DEFAULT_CALLSIGN ""
#define WS_DEFAULT_AUTO_RECONNECT 1

#define WS_DEVICE_ID_PREFIX "coffee-v2-"

/* 断线重连退避时间。 */
#define WS_RECONNECT_MIN_MS 1000
#define WS_RECONNECT_MAX_MS 30000

/* WebSocket actor 任务配置。 */
#define WS_ACTOR_QUEUE_LEN 16
#define WS_ACTOR_TASK_STACK 6144
