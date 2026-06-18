#include "modules/ws/ws_cw_cache.h"

#include <string.h>

#if defined(__has_include)
#if __has_include("freertos/FreeRTOS.h")
#define WS_CW_CACHE_HAS_FREERTOS 1
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#define WS_CW_CACHE_HAS_FREERTOS 0
#endif
#else
#define WS_CW_CACHE_HAS_FREERTOS 1
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

static ws_cw_record_t s_records[WS_CW_CACHE_MAX_RECORDS];
static size_t s_start;
static size_t s_count;
static uint32_t s_next_seq = 1;

#if WS_CW_CACHE_HAS_FREERTOS
static SemaphoreHandle_t s_lock;
static StaticSemaphore_t s_lock_buf;
static portMUX_TYPE s_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;
#endif

static esp_err_t ws_cw_cache_lock_init(void)
{
#if WS_CW_CACHE_HAS_FREERTOS
	if(s_lock != NULL) {
		return ESP_OK;
	}

	portENTER_CRITICAL(&s_lock_init_mux);
	if(s_lock == NULL) {
		s_lock = xSemaphoreCreateMutexStatic(&s_lock_buf);
	}
	portEXIT_CRITICAL(&s_lock_init_mux);

	return s_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM;
#else
	return ESP_OK;
#endif
}

static bool ws_cw_cache_take(void)
{
	if(ws_cw_cache_lock_init() != ESP_OK) {
		return false;
	}

#if WS_CW_CACHE_HAS_FREERTOS
	return xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE;
#else
	return true;
#endif
}

static void ws_cw_cache_give(void)
{
#if WS_CW_CACHE_HAS_FREERTOS
	xSemaphoreGive(s_lock);
#endif
}

esp_err_t ws_cw_cache_add(const ws_cw_record_t *record, uint32_t *out_seq)
{
	if(record == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	if(!ws_cw_cache_take()) {
		return ESP_ERR_INVALID_STATE;
	}

	size_t index;
	if(s_count < WS_CW_CACHE_MAX_RECORDS) {
		index = (s_start + s_count) % WS_CW_CACHE_MAX_RECORDS;
		s_count++;
	} else {
		index = s_start;
		s_start = (s_start + 1) % WS_CW_CACHE_MAX_RECORDS;
	}

	s_records[index] = *record;
	s_records[index].seq = s_next_seq++;
	if(s_next_seq == 0) {
		s_next_seq = 1;
	}

	if(out_seq != NULL) {
		*out_seq = s_records[index].seq;
	}

	ws_cw_cache_give();
	return ESP_OK;
}

size_t ws_cw_cache_count(void)
{
	if(!ws_cw_cache_take()) {
		return 0;
	}

	size_t count = s_count;
	ws_cw_cache_give();
	return count;
}

bool ws_cw_cache_get(size_t index, ws_cw_record_t *out_record)
{
	if(out_record == NULL || !ws_cw_cache_take()) {
		return false;
	}

	if(index >= s_count) {
		ws_cw_cache_give();
		return false;
	}

	size_t physical = (s_start + index) % WS_CW_CACHE_MAX_RECORDS;
	*out_record = s_records[physical];
	ws_cw_cache_give();
	return true;
}

bool ws_cw_cache_get_by_seq(uint32_t seq, ws_cw_record_t *out_record)
{
	if(seq == 0 || out_record == NULL || !ws_cw_cache_take()) {
		return false;
	}

	for(size_t i = 0; i < s_count; i++) {
		size_t physical = (s_start + i) % WS_CW_CACHE_MAX_RECORDS;
		if(s_records[physical].seq == seq) {
			*out_record = s_records[physical];
			ws_cw_cache_give();
			return true;
		}
	}

	ws_cw_cache_give();
	return false;
}
