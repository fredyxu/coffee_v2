#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "modules/wifi/wifi_scan_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_PROFILE_MAX 5u

typedef struct {
	bool valid;
	bool auto_connect;
	char ssid[33];
	char password[65];
	int32_t priority;
	uint32_t success_count;
	uint32_t last_connected_tick;
} wifi_profile_t;

esp_err_t wifi_profile_init(void);
esp_err_t wifi_profile_save_success(const char *ssid, const char *password);
bool wifi_profile_find(const char *ssid, wifi_profile_t *out_profile);
bool wifi_profile_select_best_from_scan(wifi_profile_t *out_profile);
esp_err_t wifi_profile_forget(const char *ssid);
size_t wifi_profile_count(void);

#ifdef __cplusplus
}
#endif
