#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	STORE_KV_BOOL = 0,
	STORE_KV_I32,
	STORE_KV_STR,
} store_kv_type_t;

typedef struct {
	const char *namespace_name;
	const char *key;
	store_kv_type_t type;
	void *value;
	size_t value_size;
} store_kv_item_t;

esp_err_t store_kv_init(void);
esp_err_t store_kv_load(const store_kv_item_t *item);
esp_err_t store_kv_save(const store_kv_item_t *item);

#ifdef __cplusplus
}
#endif
