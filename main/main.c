#include <stdio.h>
#include "esp_err.h"
#include "core/utils/log.h"
#include "app/app_init.h"


void app_main(void)
{
	esp_err_t err = app_startup();
	if (err != ESP_OK) {
		LOG("Failed to start application.");
	}
}	
