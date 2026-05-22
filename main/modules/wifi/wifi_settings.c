#include "modules/wifi/wifi_settings.h"

#include <stdio.h>
#include <string.h>

#define WIFI_SETTINGS_SSID_MAX 16

static settings_value_list_t s_wifi_ssid_list[WIFI_SETTINGS_SSID_MAX];
static size_t s_wifi_ssid_count;

static void wifi_settings_set_item(settings_value_list_t *item,
								   const char *title,
								   int value_int,
								   bool disabled,
								   settings_list_item_type_t type)
{
	if(item == NULL) {
		return;
	}

	memset(item, 0, sizeof(*item));
	(void)snprintf(item->title, sizeof(item->title), "%s", title ? title : "");
	item->value_int = value_int;
	item->disabled = disabled;
	item->type = type;
}

settings_value_list_t *wifi_settings_ssid_list(void)
{
	return s_wifi_ssid_list;
}

size_t *wifi_settings_ssid_count(void)
{
	return &s_wifi_ssid_count;
}

size_t wifi_settings_ssid_max(void)
{
	return WIFI_SETTINGS_SSID_MAX;
}

void wifi_settings_ssid_clear(void)
{
	memset(s_wifi_ssid_list, 0, sizeof(s_wifi_ssid_list));
	s_wifi_ssid_count = 0;
}

void wifi_settings_ssid_set_status(const char *text)
{
	wifi_settings_ssid_clear();
	wifi_settings_set_item(
		&s_wifi_ssid_list[0],
		text,
		0,
		true,
		SETTINGS_LIST_ITEM_STATUS
	);
	s_wifi_ssid_count = 1;
}

void wifi_settings_ssid_add_ap(const wifi_scan_ap_t *ap)
{
	if(ap == NULL) {
		return;
	}

	if(s_wifi_ssid_count == 1 && s_wifi_ssid_list[0].type == SETTINGS_LIST_ITEM_STATUS) {
		wifi_settings_ssid_clear();
	}

	if(s_wifi_ssid_count >= WIFI_SETTINGS_SSID_MAX) {
		return;
	}

	wifi_settings_set_item(
		&s_wifi_ssid_list[s_wifi_ssid_count],
		ap->ssid,
		ap->rssi,
		false,
		SETTINGS_LIST_ITEM_NORMAL
	);
	s_wifi_ssid_count++;
}

void wifi_settings_ssid_set_empty_result(void)
{
	wifi_settings_ssid_set_status("未发现网络");
}
