#pragma once

/* 对讲房间 / 成员列表配置 */

/*
 * 房间/成员列表 WS 同步开关。
 *
 * 0：只保留对讲页 UI 占位，不向服务器发送 room_list / room_users /
 *    room join / room leave，优先保证 PTT 音频链路稳定。
 * 1：启用完整房间/成员快照同步。
 */
#ifndef INTERCOM_ROOM_SYNC_ENABLE
#define INTERCOM_ROOM_SYNC_ENABLE 1
#endif

#define ROOM_LIST_MAX_COUNT 32
#define ROOM_USERS_MAX_COUNT 128

#define ROOM_ID_MAX_LEN 32
#define ROOM_NAME_MAX_LEN 32
#define ROOM_SERVER_TIME_MAX_LEN 32

#define ROOM_USER_DEVICE_ID_MAX_LEN 48
#define ROOM_USER_CALLSIGN_MAX_LEN 64
#define ROOM_USER_FW_VERSION_MAX_LEN 32
