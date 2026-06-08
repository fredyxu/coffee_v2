#include "modules/wifi/wifi_profile.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "core/store/store_kv.h"
#include "core/utils/log.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define WIFI_PROFILE_NS "wifi_profiles"

static wifi_profile_t s_profiles[WIFI_PROFILE_MAX];
static bool s_profile_init_done;

static void wifi_profile_key(char *buf, size_t buf_size, size_t index, const char *field)
{
	if(buf == NULL || buf_size == 0) {
		return;
	}

	(void)snprintf(buf, buf_size, "p%u_%s", (unsigned)index, field ? field : "");
}

static uint32_t wifi_profile_now_tick(void)
{
#ifdef ESP_PLATFORM
	return (uint32_t)xTaskGetTickCount();
#else
	return 0;
#endif
}

static esp_err_t wifi_profile_load_slot(size_t index)
{
	if(index >= WIFI_PROFILE_MAX) {
		return ESP_ERR_INVALID_ARG;
	}

	wifi_profile_t profile = {
		.auto_connect = true,
	};
	char key[16];

	wifi_profile_key(key, sizeof(key), index, "valid");
	store_kv_item_t item = {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_BOOL,
		.value = &profile.valid,
		.value_size = sizeof(profile.valid),
	};
	esp_err_t err = store_kv_load(&item);
	if(err == ESP_ERR_NOT_FOUND) {
		memset(&s_profiles[index], 0, sizeof(s_profiles[index]));
		return ESP_OK;
	}
	if(err != ESP_OK) {
		return err;
	}

	wifi_profile_key(key, sizeof(key), index, "auto");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_BOOL,
		.value = &profile.auto_connect,
		.value_size = sizeof(profile.auto_connect),
	};
	(void)store_kv_load(&item);

	wifi_profile_key(key, sizeof(key), index, "ssid");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_STR,
		.value = profile.ssid,
		.value_size = sizeof(profile.ssid),
	};
	(void)store_kv_load(&item);

	wifi_profile_key(key, sizeof(key), index, "pass");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_STR,
		.value = profile.password,
		.value_size = sizeof(profile.password),
	};
	(void)store_kv_load(&item);

	wifi_profile_key(key, sizeof(key), index, "prio");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_I32,
		.value = &profile.priority,
		.value_size = sizeof(profile.priority),
	};
	(void)store_kv_load(&item);

	int32_t success_count = 0;
	wifi_profile_key(key, sizeof(key), index, "count");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_I32,
		.value = &success_count,
		.value_size = sizeof(success_count),
	};
	(void)store_kv_load(&item);
	profile.success_count = success_count > 0 ? (uint32_t)success_count : 0;

	int32_t last_tick = 0;
	wifi_profile_key(key, sizeof(key), index, "last");
	item = (store_kv_item_t) {
		.namespace_name = WIFI_PROFILE_NS,
		.key = key,
		.type = STORE_KV_I32,
		.value = &last_tick,
		.value_size = sizeof(last_tick),
	};
	(void)store_kv_load(&item);
	profile.last_connected_tick = last_tick > 0 ? (uint32_t)last_tick : 0;

	if(profile.ssid[0] == '\0') {
		memset(&profile, 0, sizeof(profile));
	}

	s_profiles[index] = profile;
	return ESP_OK;
}

static esp_err_t wifi_profile_save_slot(size_t index)
{
	if(index >= WIFI_PROFILE_MAX) {
		return ESP_ERR_INVALID_ARG;
	}

	wifi_profile_t *profile = &s_profiles[index];
	char key[16];
	esp_err_t first_err = ESP_OK;

#define SAVE_FIELD(field_name, field_type, field_value, field_size) do { \
		wifi_profile_key(key, sizeof(key), index, (field_name)); \
		store_kv_item_t item = { \
			.namespace_name = WIFI_PROFILE_NS, \
			.key = key, \
			.type = (field_type), \
			.value = (field_value), \
			.value_size = (field_size), \
		}; \
		esp_err_t err = store_kv_save(&item); \
		if(err != ESP_OK && first_err == ESP_OK) { \
			first_err = err; \
		} \
	} while(0)

	SAVE_FIELD("valid", STORE_KV_BOOL, &profile->valid, sizeof(profile->valid));
	SAVE_FIELD("auto", STORE_KV_BOOL, &profile->auto_connect, sizeof(profile->auto_connect));
	SAVE_FIELD("ssid", STORE_KV_STR, profile->ssid, sizeof(profile->ssid));
	SAVE_FIELD("pass", STORE_KV_STR, profile->password, sizeof(profile->password));
	SAVE_FIELD("prio", STORE_KV_I32, &profile->priority, sizeof(profile->priority));

	int32_t success_count = profile->success_count > INT32_MAX ? INT32_MAX : (int32_t)profile->success_count;
	SAVE_FIELD("count", STORE_KV_I32, &success_count, sizeof(success_count));

	int32_t last_tick = profile->last_connected_tick > INT32_MAX ? INT32_MAX : (int32_t)profile->last_connected_tick;
	SAVE_FIELD("last", STORE_KV_I32, &last_tick, sizeof(last_tick));

#undef SAVE_FIELD

	return first_err;
}

static int wifi_profile_find_index(const char *ssid)
{
	if(ssid == NULL || ssid[0] == '\0') {
		return -1;
	}

	for(size_t i = 0; i < WIFI_PROFILE_MAX; i++) {
		if(s_profiles[i].valid && strcmp(s_profiles[i].ssid, ssid) == 0) {
			return (int)i;
		}
	}

	return -1;
}

static size_t wifi_profile_choose_replacement_slot(void)
{
	for(size_t i = 0; i < WIFI_PROFILE_MAX; i++) {
		if(!s_profiles[i].valid) {
			return i;
		}
	}

	size_t selected = 0;
	for(size_t i = 1; i < WIFI_PROFILE_MAX; i++) {
		const wifi_profile_t *cur = &s_profiles[i];
		const wifi_profile_t *best = &s_profiles[selected];
		if(cur->priority < best->priority ||
		   (cur->priority == best->priority && cur->last_connected_tick < best->last_connected_tick)) {
			selected = i;
		}
	}

	return selected;
}

esp_err_t wifi_profile_init(void)
{
	if(s_profile_init_done) {
		return ESP_OK;
	}

	esp_err_t err = store_kv_init();
	if(err != ESP_OK) {
		return err;
	}

	esp_err_t first_err = ESP_OK;
	for(size_t i = 0; i < WIFI_PROFILE_MAX; i++) {
		err = wifi_profile_load_slot(i);
		if(err != ESP_OK && first_err == ESP_OK) {
			first_err = err;
		}
	}

	s_profile_init_done = (first_err == ESP_OK);
	return first_err;
}

esp_err_t wifi_profile_save_success(const char *ssid, const char *password)
{
	if(ssid == NULL || password == NULL || ssid[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}
	if(strlen(ssid) >= sizeof(s_profiles[0].ssid) || strlen(password) >= sizeof(s_profiles[0].password)) {
		return ESP_ERR_INVALID_ARG;
	}

	esp_err_t err = wifi_profile_init();
	if(err != ESP_OK) {
		return err;
	}

	int index = wifi_profile_find_index(ssid);
	if(index < 0) {
		index = (int)wifi_profile_choose_replacement_slot();
		memset(&s_profiles[index], 0, sizeof(s_profiles[index]));
	}

	wifi_profile_t *profile = &s_profiles[index];
	profile->valid = true;
	profile->auto_connect = true;
	(void)snprintf(profile->ssid, sizeof(profile->ssid), "%s", ssid);
	(void)snprintf(profile->password, sizeof(profile->password), "%s", password);
	profile->success_count++;
	profile->last_connected_tick = wifi_profile_now_tick();

	err = wifi_profile_save_slot((size_t)index);
	if(err != ESP_OK) {
		LOG("wifi profile save failed: ssid=%s err=%s", ssid, esp_err_to_name(err));
	}
	return err;
}

bool wifi_profile_find(const char *ssid, wifi_profile_t *out_profile)
{
	(void)wifi_profile_init();

	int index = wifi_profile_find_index(ssid);
	if(index < 0) {
		return false;
	}

	if(out_profile != NULL) {
		*out_profile = s_profiles[index];
	}
	return true;
}

bool wifi_profile_select_best_from_scan(wifi_profile_t *out_profile)
{
	(void)wifi_profile_init();

	bool found = false;
	wifi_profile_t best_profile = {0};
	int best_rssi = -128;

	for(size_t i = 0; i < wifi_scan_cache_count(); i++) {
		wifi_scan_ap_t ap = {0};
		if(!wifi_scan_cache_get(i, &ap)) {
			continue;
		}

		int profile_index = wifi_profile_find_index(ap.ssid);
		if(profile_index < 0) {
			continue;
		}

		wifi_profile_t *profile = &s_profiles[profile_index];
		if(!profile->auto_connect) {
			continue;
		}

		if(!found ||
		   profile->priority > best_profile.priority ||
		   (profile->priority == best_profile.priority && ap.rssi > best_rssi) ||
		   (profile->priority == best_profile.priority && ap.rssi == best_rssi &&
		    profile->success_count > best_profile.success_count)) {
			best_profile = *profile;
			best_rssi = ap.rssi;
			found = true;
		}
	}

	if(found && out_profile != NULL) {
		*out_profile = best_profile;
	}
	return found;
}

esp_err_t wifi_profile_forget(const char *ssid)
{
	esp_err_t err = wifi_profile_init();
	if(err != ESP_OK) {
		return err;
	}

	int index = wifi_profile_find_index(ssid);
	if(index < 0) {
		return ESP_ERR_NOT_FOUND;
	}

	memset(&s_profiles[index], 0, sizeof(s_profiles[index]));
	return wifi_profile_save_slot((size_t)index);
}

size_t wifi_profile_count(void)
{
	(void)wifi_profile_init();

	size_t count = 0;
	for(size_t i = 0; i < WIFI_PROFILE_MAX; i++) {
		if(s_profiles[i].valid) {
			count++;
		}
	}
	return count;
}
