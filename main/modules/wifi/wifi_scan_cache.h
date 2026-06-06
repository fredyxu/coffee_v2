#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SCAN_CACHE_SSID_MAX 32u
#define WIFI_SCAN_CACHE_MAX_APS 16u

typedef struct {
	char title[33];
    char ssid[WIFI_SCAN_CACHE_SSID_MAX + 1u];
    int rssi;
    uint8_t authmode;
    uint8_t channel;
} wifi_scan_ap_t;

void wifi_scan_cache_clear(void);
bool wifi_scan_cache_add(const wifi_scan_ap_t *ap);
size_t wifi_scan_cache_count(void);
bool wifi_scan_cache_get(size_t index, wifi_scan_ap_t *out_ap);

#ifdef __cplusplus
}
#endif
