#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_boot_log_iter_cb_t)(const char *text, void *user_data);

void app_boot_log_add(const char *text);
void app_boot_log_for_each(app_boot_log_iter_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
