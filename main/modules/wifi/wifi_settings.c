#include "wifi_settings.h"

#include <stdio.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "app/app_settings.h"
#include "wifi.h"
#endif

#define WIFI_SETTINGS_SSID_MAX 16

static settings_value_list_t s_wifi_ssid_list[WIFI_SETTINGS_SSID_MAX] = {
	0
	// {
	// 	.type = SETTINGS_LIST_ITEM_STATUS,
	// 	.disabled = false,
	// 	.title = "SSID_1",
	// 	.selected = true,
	// },
	// {
	// 	.type = SETTINGS_LIST_ITEM_STATUS,
	// 	.disabled = false,
	// 	.title = "SSID_2",
	// 	.selected = false,
	// }
};

static size_t s_wifi_ssid_count;

static void wifi_settings_ssid_disconnect_action(const settings_value_list_t *item, void *user_data)
{
	(void)item;
	(void)user_data;

#ifdef ESP_PLATFORM
	(void)msg_send_cmd_value(MSG_SRC_LVGL, MSG_EVT_CMD_WIFI_DISCONNECT, 0, 0);
#endif
}

const settings_value_list_source_t wifi_settings_ssid_source = {
	.list = s_wifi_ssid_list,
	.count = &s_wifi_ssid_count,
	.max = WIFI_SETTINGS_SSID_MAX,
};

static void wifi_settings_set_item(settings_value_list_t *item,
								   const char *title,
								   const char *value_str,
								   int value_int,
								   bool disabled,
								   bool selected,
								   settings_list_item_type_t type)
{
	if(item == NULL) {
		return;
	}

	memset(item, 0, sizeof(*item));
	(void)snprintf(item->title, sizeof(item->title), "%s", title ? title : "");
	(void)snprintf(item->value_str, sizeof(item->value_str), "%s", value_str ? value_str : "");
	item->value_int = value_int;
	item->disabled = disabled;
	item->selected = selected;
	item->type = type;
}

static settings_value_list_t *wifi_settings_find_ssid(const char *ssid)
{
	if(ssid == NULL || ssid[0] == '\0') {
		return NULL;
	}

	for(size_t i = 0; i < s_wifi_ssid_count; i++) {
		settings_value_list_t *item = &s_wifi_ssid_list[i];
		if(item->type == SETTINGS_LIST_ITEM_NORMAL && strcmp(item->value_str, ssid) == 0) {
			return item;
		}
	}

	return NULL;
}

static void wifi_settings_copy_title(settings_value_list_t *item, const char *title)
{
	if(item == NULL) {
		return;
	}

	(void)strncpy(item->title, title ? title : "", sizeof(item->title) - 1);
	item->title[sizeof(item->title) - 1] = '\0';
}

static void wifi_settings_set_connected_title(settings_value_list_t *item, const char *ssid)
{
	if(item == NULL) {
		return;
	}

	const char suffix[] = " 已连接";
	const size_t suffix_len = strlen(suffix);
	size_t ssid_len = ssid != NULL ? strlen(ssid) : 0;
	const size_t max_ssid_len = sizeof(item->title) > suffix_len + 1
		? sizeof(item->title) - suffix_len - 1
		: 0;

	if(ssid_len > max_ssid_len) {
		ssid_len = max_ssid_len;
	}

	memcpy(item->title, ssid ? ssid : "", ssid_len);
	memcpy(item->title + ssid_len, suffix, suffix_len);
	item->title[ssid_len + suffix_len] = '\0';
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
		text,
		0,
		true,
		false,
		SETTINGS_LIST_ITEM_STATUS
	);
	s_wifi_ssid_count = 1;
}

void wifi_settings_ssid_add_ap(const wifi_scan_ap_t *ap)
{
	if(ap == NULL || ap->ssid[0] == '\0') {
		return;
	}

	if(s_wifi_ssid_count == 1 && s_wifi_ssid_list[0].type == SETTINGS_LIST_ITEM_STATUS) {
		wifi_settings_ssid_clear();
	}

	settings_value_list_t *existing = wifi_settings_find_ssid(ap->ssid);
	if(existing != NULL) {
		if(ap->rssi > existing->value_int) {
			existing->value_int = ap->rssi;
		}
		return;
	}

	if(s_wifi_ssid_count >= WIFI_SETTINGS_SSID_MAX) {
		return;
	}

	wifi_settings_set_item(
		&s_wifi_ssid_list[s_wifi_ssid_count],
		ap->ssid,
		ap->ssid,
		ap->rssi,
		false,
		false,
		SETTINGS_LIST_ITEM_NORMAL
	);
	s_wifi_ssid_count++;
}

void wifi_settings_ssid_refresh_connected(void)
{
	size_t connected_index = s_wifi_ssid_count;

	for(size_t i = 0; i < s_wifi_ssid_count; i++) {
		settings_value_list_t *item = &s_wifi_ssid_list[i];
		if(item->type != SETTINGS_LIST_ITEM_NORMAL) {
			continue;
			}
	
			item->selected = false;
			item->on_action = NULL;
			item->user_data = NULL;
			char value_str[sizeof(item->value_str)];
			(void)strncpy(value_str, item->value_str, sizeof(value_str) - 1);
			value_str[sizeof(value_str) - 1] = '\0';
		wifi_settings_copy_title(item, value_str);

#ifdef ESP_PLATFORM
		if(connected_index == s_wifi_ssid_count &&
		   app_settings.wifi_ssid[0] != '\0' &&
		   wifi_module_is_connected() &&
		   strcmp(value_str, app_settings.wifi_ssid) == 0) {
			item->selected = true;
			item->on_action = wifi_settings_ssid_disconnect_action;
			wifi_settings_set_connected_title(item, value_str);
			connected_index = i;
		}
#endif
	}

	if(connected_index == s_wifi_ssid_count || connected_index == 0) {
		return;
	}

	settings_value_list_t connected_item = s_wifi_ssid_list[connected_index];
	for(size_t i = connected_index; i > 0; i--) {
		s_wifi_ssid_list[i] = s_wifi_ssid_list[i - 1];
	}
	s_wifi_ssid_list[0] = connected_item;
}

void wifi_settings_ssid_set_empty_result(void)
{
	wifi_settings_ssid_set_status("未发现网络");
}
