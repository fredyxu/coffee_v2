#include "app/app_boot_log.h"

#include <stdio.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

#define APP_BOOT_LOG_MAX_LINES 32
#define APP_BOOT_LOG_TEXT_LEN 64

static char s_boot_log[APP_BOOT_LOG_MAX_LINES][APP_BOOT_LOG_TEXT_LEN];
static size_t s_boot_log_start;
static size_t s_boot_log_count;

#ifdef ESP_PLATFORM
static SemaphoreHandle_t s_boot_log_lock;
static StaticSemaphore_t s_boot_log_lock_buf;
static portMUX_TYPE s_boot_log_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;

static void app_boot_log_lock_init(void)
{
	if(s_boot_log_lock != NULL) {
		return;
	}

	portENTER_CRITICAL(&s_boot_log_lock_init_mux);
	if(s_boot_log_lock == NULL) {
		s_boot_log_lock = xSemaphoreCreateMutexStatic(&s_boot_log_lock_buf);
	}
	portEXIT_CRITICAL(&s_boot_log_lock_init_mux);
}
#else
static void app_boot_log_lock_init(void)
{
}
#endif

void app_boot_log_add(const char *text)
{
	app_boot_log_lock_init();
#ifdef ESP_PLATFORM
	if(s_boot_log_lock == NULL) {
		return;
	}

	(void)xSemaphoreTake(s_boot_log_lock, portMAX_DELAY);
#endif

	size_t index = 0;
	if(s_boot_log_count < APP_BOOT_LOG_MAX_LINES) {
		index = (s_boot_log_start + s_boot_log_count) % APP_BOOT_LOG_MAX_LINES;
		s_boot_log_count++;
	} else {
		index = s_boot_log_start;
		s_boot_log_start = (s_boot_log_start + 1) % APP_BOOT_LOG_MAX_LINES;
	}

	(void)snprintf(s_boot_log[index], sizeof(s_boot_log[index]), "%s", text ? text : "");

#ifdef ESP_PLATFORM
	(void)xSemaphoreGive(s_boot_log_lock);
#endif
}

void app_boot_log_for_each(app_boot_log_iter_cb_t cb, void *user_data)
{
	if(cb == NULL) {
		return;
	}

	app_boot_log_lock_init();
#ifdef ESP_PLATFORM
	if(s_boot_log_lock == NULL) {
		return;
	}
#endif

	char snapshot[APP_BOOT_LOG_MAX_LINES][APP_BOOT_LOG_TEXT_LEN];
	size_t count = 0;

#ifdef ESP_PLATFORM
	(void)xSemaphoreTake(s_boot_log_lock, portMAX_DELAY);
#endif
	count = s_boot_log_count;
	for(size_t i = 0; i < count; i++) {
		size_t index = (s_boot_log_start + i) % APP_BOOT_LOG_MAX_LINES;
		(void)snprintf(snapshot[i], sizeof(snapshot[i]), "%s", s_boot_log[index]);
	}
#ifdef ESP_PLATFORM
	(void)xSemaphoreGive(s_boot_log_lock);
#endif

	for(size_t i = 0; i < count; i++) {
		cb(snapshot[i], user_data);
	}
}
