#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define WS_CW_CACHE_MAX_RECORDS 50

typedef struct {
	uint32_t seq;
	char room[17];
	char room_name[17];
	char callsign[65];
	char code[257];
	char from[9];
	char sender_id[33];
	char date[11];
	char time[9];
	char role[17];
	int level;
} ws_cw_record_t;

typedef void (*ws_cw_cache_iter_cb_t)(const ws_cw_record_t *record, void *user_data);

esp_err_t ws_cw_cache_add(const ws_cw_record_t *record, uint32_t *out_seq);
size_t ws_cw_cache_count(void);
bool ws_cw_cache_get(size_t index, ws_cw_record_t *out_record);
bool ws_cw_cache_get_by_seq(uint32_t seq, ws_cw_record_t *out_record);

