#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct {
	char *wifi_ssid;
	char *wifi_password;
	bool wifi_enable;
} app_settings_t;

extern app_settings_t app_settings;

#ifdef __cplusplus
}
#endif