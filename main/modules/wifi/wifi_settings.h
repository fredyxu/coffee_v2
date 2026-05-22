#pragma once

#include <stddef.h>
#include "modules/ui/page/page_settings/page_settings_data.h"
#include "modules/wifi/wifi_scan_cache.h"

#ifdef __cplusplus
extern "C" {
#endif

settings_value_list_t *wifi_settings_ssid_list(void);
size_t *wifi_settings_ssid_count(void);
size_t wifi_settings_ssid_max(void);

void wifi_settings_ssid_clear(void);
void wifi_settings_ssid_set_status(const char *text);
void wifi_settings_ssid_add_ap(const wifi_scan_ap_t *ap);
void wifi_settings_ssid_set_empty_result(void);

#ifdef __cplusplus
}
#endif
