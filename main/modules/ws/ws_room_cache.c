#include "modules/ws/ws_room_cache.h"

#include <string.h>

#include "core/utils/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static ws_room_snapshot_t s_rooms = {
	.revision = 0,
	.count = 1,
	.rooms = {
		{
			.id = "default",
			.name = "大厅",
			.user_count = 0,
			.locked = false,
		},
	},
};
static ws_room_users_snapshot_t s_users;
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buf;
static portMUX_TYPE s_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;

static esp_err_t ws_room_cache_lock_init(void)
{
	if(s_lock != NULL) {
		return ESP_OK;
	}

	portENTER_CRITICAL(&s_lock_init_mux);
	if(s_lock == NULL) {
		s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
	}
	portEXIT_CRITICAL(&s_lock_init_mux);

	return s_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool ws_room_cache_take(void)
{
	if(ws_room_cache_lock_init() != ESP_OK) {
		return false;
	}
	return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE;
}

static void ws_room_cache_give(void)
{
	xSemaphoreGive(s_lock);
}

static void ws_room_cache_copy_text(char *dst, size_t dst_size, const char *src)
{
	if(dst == NULL || dst_size == 0) {
		return;
	}
	if(src == NULL) {
		dst[0] = '\0';
		return;
	}
	strncpy(dst, src, dst_size - 1);
	dst[dst_size - 1] = '\0';
}

esp_err_t ws_room_cache_update_rooms(const ws_room_snapshot_t *snapshot)
{
	if(snapshot == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if(!ws_room_cache_take()) {
		return ESP_ERR_INVALID_STATE;
	}
	if(snapshot->revision != 0 && snapshot->revision <= s_rooms.revision) {
		LOG("ws room cache reject rooms: incoming_revision=%u current_revision=%u count=%u",
			(unsigned)snapshot->revision,
			(unsigned)s_rooms.revision,
			(unsigned)snapshot->count);
		ws_room_cache_give();
		return ESP_ERR_INVALID_STATE;
	}

	memset(&s_rooms, 0, sizeof(s_rooms));
	s_rooms.revision = snapshot->revision;
	s_rooms.truncated = snapshot->truncated;
	ws_room_cache_copy_text(s_rooms.server_time, sizeof(s_rooms.server_time), snapshot->server_time);
	s_rooms.count = snapshot->count > ROOM_LIST_MAX_COUNT ? ROOM_LIST_MAX_COUNT : snapshot->count;
	for(size_t i = 0; i < s_rooms.count; i++) {
		s_rooms.rooms[i] = snapshot->rooms[i];
		s_rooms.rooms[i].id[ROOM_ID_MAX_LEN] = '\0';
		s_rooms.rooms[i].name[ROOM_NAME_MAX_LEN] = '\0';
	}
	if(s_rooms.count == 0) {
		s_rooms.count = 1;
		ws_room_cache_copy_text(s_rooms.rooms[0].id, sizeof(s_rooms.rooms[0].id), "default");
		ws_room_cache_copy_text(s_rooms.rooms[0].name, sizeof(s_rooms.rooms[0].name), "大厅");
	}
	// LOG("ws room cache updated rooms: revision=%u count=%u truncated=%d",
	// 	(unsigned)s_rooms.revision,
	// 	(unsigned)s_rooms.count,
	// 	(int)s_rooms.truncated);

	ws_room_cache_give();
	return ESP_OK;
}

esp_err_t ws_room_cache_update_users(const ws_room_users_snapshot_t *snapshot)
{
	if(snapshot == NULL) {
		return ESP_ERR_INVALID_ARG;
	}
	if(!ws_room_cache_take()) {
		return ESP_ERR_INVALID_STATE;
	}
	if(snapshot->revision != 0 && snapshot->revision <= s_users.revision &&
	   strcmp(snapshot->room, s_users.room) == 0) {
		LOG("ws room cache reject users: room=%s incoming_revision=%u current_revision=%u count=%u",
			snapshot->room,
			(unsigned)snapshot->revision,
			(unsigned)s_users.revision,
			(unsigned)snapshot->count);
		ws_room_cache_give();
		return ESP_ERR_INVALID_STATE;
	}

	memset(&s_users, 0, sizeof(s_users));
	s_users.revision = snapshot->revision;
	s_users.truncated = snapshot->truncated;
	ws_room_cache_copy_text(s_users.room, sizeof(s_users.room), snapshot->room);
	ws_room_cache_copy_text(s_users.server_time, sizeof(s_users.server_time), snapshot->server_time);
	s_users.count = snapshot->count > ROOM_USERS_MAX_COUNT ? ROOM_USERS_MAX_COUNT : snapshot->count;
	for(size_t i = 0; i < s_users.count; i++) {
		s_users.users[i] = snapshot->users[i];
		s_users.users[i].device_id[ROOM_USER_DEVICE_ID_MAX_LEN] = '\0';
		s_users.users[i].callsign[ROOM_USER_CALLSIGN_MAX_LEN] = '\0';
		s_users.users[i].fw_version[ROOM_USER_FW_VERSION_MAX_LEN] = '\0';
	}
	LOG("ws room cache updated users: room=%s revision=%u count=%u truncated=%d",
		s_users.room,
		(unsigned)s_users.revision,
		(unsigned)s_users.count,
		(int)s_users.truncated);

	ws_room_cache_give();
	return ESP_OK;
}

size_t ws_room_cache_room_count(void)
{
	if(!ws_room_cache_take()) {
		return 0;
	}
	size_t count = s_rooms.count;
	ws_room_cache_give();
	return count;
}

size_t ws_room_cache_user_count(void)
{
	if(!ws_room_cache_take()) {
		return 0;
	}
	size_t count = s_users.count;
	ws_room_cache_give();
	return count;
}

uint32_t ws_room_cache_room_revision(void)
{
	if(!ws_room_cache_take()) {
		return 0;
	}
	uint32_t revision = s_rooms.revision;
	ws_room_cache_give();
	return revision;
}

uint32_t ws_room_cache_user_revision(void)
{
	if(!ws_room_cache_take()) {
		return 0;
	}
	uint32_t revision = s_users.revision;
	ws_room_cache_give();
	return revision;
}

bool ws_room_cache_rooms_truncated(void)
{
	if(!ws_room_cache_take()) {
		return false;
	}
	bool truncated = s_rooms.truncated;
	ws_room_cache_give();
	return truncated;
}

bool ws_room_cache_users_truncated(void)
{
	if(!ws_room_cache_take()) {
		return false;
	}
	bool truncated = s_users.truncated;
	ws_room_cache_give();
	return truncated;
}

bool ws_room_cache_get_room(size_t index, ws_room_record_t *out_room)
{
	if(out_room == NULL || !ws_room_cache_take()) {
		return false;
	}
	if(index >= s_rooms.count) {
		ws_room_cache_give();
		return false;
	}
	*out_room = s_rooms.rooms[index];
	ws_room_cache_give();
	return true;
}

bool ws_room_cache_get_user(size_t index, ws_room_user_record_t *out_user)
{
	if(out_user == NULL || !ws_room_cache_take()) {
		return false;
	}
	if(index >= s_users.count) {
		ws_room_cache_give();
		return false;
	}
	*out_user = s_users.users[index];
	ws_room_cache_give();
	return true;
}

bool ws_room_cache_find_room(const char *room_id, size_t *out_index, ws_room_record_t *out_room)
{
	if(room_id == NULL || room_id[0] == '\0' || !ws_room_cache_take()) {
		return false;
	}
	for(size_t i = 0; i < s_rooms.count; i++) {
		if(strcmp(s_rooms.rooms[i].id, room_id) == 0) {
			if(out_index != NULL) {
				*out_index = i;
			}
			if(out_room != NULL) {
				*out_room = s_rooms.rooms[i];
			}
			ws_room_cache_give();
			return true;
		}
	}
	ws_room_cache_give();
	return false;
}

bool ws_room_cache_current_users_room(char *out_room, size_t out_size)
{
	if(out_room == NULL || out_size == 0 || !ws_room_cache_take()) {
		return false;
	}
	ws_room_cache_copy_text(out_room, out_size, s_users.room);
	bool ok = s_users.room[0] != '\0';
	ws_room_cache_give();
	return ok;
}
