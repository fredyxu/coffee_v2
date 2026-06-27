#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config_room.h"
#include "esp_err.h"

typedef struct {
	char id[ROOM_ID_MAX_LEN + 1];
	char name[ROOM_NAME_MAX_LEN + 1];
	int user_count;
	bool locked;
} ws_room_record_t;

typedef struct {
	uint32_t revision;
	char server_time[ROOM_SERVER_TIME_MAX_LEN + 1];
	bool truncated;
	size_t count;
	ws_room_record_t rooms[ROOM_LIST_MAX_COUNT];
} ws_room_snapshot_t;

typedef struct {
	char device_id[ROOM_USER_DEVICE_ID_MAX_LEN + 1];
	char callsign[ROOM_USER_CALLSIGN_MAX_LEN + 1];
	char fw_version[ROOM_USER_FW_VERSION_MAX_LEN + 1];
	bool talking;
} ws_room_user_record_t;

typedef struct {
	char room[ROOM_ID_MAX_LEN + 1];
	uint32_t revision;
	char server_time[ROOM_SERVER_TIME_MAX_LEN + 1];
	bool truncated;
	size_t count;
	ws_room_user_record_t users[ROOM_USERS_MAX_COUNT];
} ws_room_users_snapshot_t;

esp_err_t ws_room_cache_update_rooms(const ws_room_snapshot_t *snapshot);
esp_err_t ws_room_cache_update_users(const ws_room_users_snapshot_t *snapshot);

size_t ws_room_cache_room_count(void);
size_t ws_room_cache_user_count(void);
uint32_t ws_room_cache_room_revision(void);
uint32_t ws_room_cache_user_revision(void);
bool ws_room_cache_rooms_truncated(void);
bool ws_room_cache_users_truncated(void);

bool ws_room_cache_get_room(size_t index, ws_room_record_t *out_room);
bool ws_room_cache_get_user(size_t index, ws_room_user_record_t *out_user);
bool ws_room_cache_find_room(const char *room_id, size_t *out_index, ws_room_record_t *out_room);
bool ws_room_cache_current_users_room(char *out_room, size_t out_size);
