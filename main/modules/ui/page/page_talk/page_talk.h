#pragma once

#include "lvgl.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const char *room_id;
	const char *room_name;
	int user_qty;
} room_type_t;

typedef struct {
	const char *callsign;

} user_type_t;

esp_err_t page_talk_show(lv_obj_t *p);


#ifdef __cplusplus
}
#endif