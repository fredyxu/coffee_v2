#include "modules/wifi/wifi_scan_cache.h"

#include <string.h>
#include "core/utils/log.h"
#include "app/app_settings.h"

static wifi_scan_ap_t s_scan_cache[WIFI_SCAN_CACHE_MAX_APS];
static size_t s_scan_count;

void wifi_scan_cache_clear(void)
{
    memset(s_scan_cache, 0, sizeof(s_scan_cache));
    s_scan_count = 0;
}

bool wifi_scan_cache_add(const wifi_scan_ap_t *ap)
{
	
    if(ap == NULL || ap->ssid[0] == '\0') {
        return false;
    }

    for(size_t i = 0; i < s_scan_count; i++) {
        if(strcmp(s_scan_cache[i].ssid, ap->ssid) == 0) {
            s_scan_cache[i] = *ap;
			// LOG("updated AP in cache: %s (rssi=%d)", ap->ssid, ap->rssi);
            return true;
        }
    }

    if(s_scan_count < WIFI_SCAN_CACHE_MAX_APS) {
        s_scan_cache[s_scan_count++] = *ap;
        return true;
    }

    size_t weakest_index = 0;
    for(size_t i = 1; i < WIFI_SCAN_CACHE_MAX_APS; i++) {
        if(s_scan_cache[i].rssi < s_scan_cache[weakest_index].rssi) {
            weakest_index = i;
        }
    }

    if(ap->rssi <= s_scan_cache[weakest_index].rssi) {
        return false;
    }

    s_scan_cache[weakest_index] = *ap;
    return true;
}

size_t wifi_scan_cache_count(void)
{
    return s_scan_count;
}

bool wifi_scan_cache_get(size_t index, wifi_scan_ap_t *out_ap)
{
    if(out_ap == NULL || index >= s_scan_count) {
        return false;
    }

    *out_ap = s_scan_cache[index];
    return true;
}
