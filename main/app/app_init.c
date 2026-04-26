#include "app_init.h"
#include "core/utils/log.h"
#include "esp_err.h"
#include "core/con/con.h"
#include "core/msg/msg.h"
#include "core/state/state.h"
#include "modules/input/touch/touch.h"
#include "modules/input/encoder/encoder_actor.h"
#include "modules/audio/audio_actor.h"
#include "modules/mic/mic_actor.h"
#include "modules/store/store_actor.h"
#include "modules/wifi/wifi_actor.h"
#include "modules/display/lcd_i80_8.h"
#include "modules/ui/adapter/lvgl_port.h"
#include "modules/ui/ui.h"
#include "modules/ui/ui_actor.h"

#include "tests/tests.h"


esp_err_t app_startup(void) {
	esp_err_t err = con_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: con actor init failed");
		return err;
	}

	// 初始化state
	err = state_init();
	if(err != ESP_OK) {
		LOG("STATE INIT FAILED");
		return err;
	}

	// 初始化LCD
	err = lcd_i80_8_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: LCD initialization failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "屏幕初始化完成.", 0);


	// 初始化屏幕触控
	err = touch_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: Touch initialization failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "触控初始化完成.", 0);

	// 初始化LVGL
	esp_lcd_panel_io_handle_t io = lcd_get_io();
	err = lvgl_port_init(io);
	if(err != ESP_OK) {
		LOG("LVGL INIT FAILED");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "LVGL初始化完成.", 0);

	vTaskDelay(pdMS_TO_TICKS(200));

	err = ui_init();
	if(err != ESP_OK) {
		LOG("UI init error");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "UI初始化完成.", 0);

	// 初始化Store Actor
	err = store_actor_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: store actor init failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "Store初始化完成.", 0);

	// 初始化WiFi Actor
	err = wifi_actor_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: wifi actor init failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "WiFi初始化完成.", 0);

	// 初始化旋转编码器
	err = encoder_actor_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: encoder actor init failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "旋转编码器初始化完成.", 0);
	// 初始化音频
	err = audio_actor_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: audio actor init failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "音频初始化完成.", 0);
	// 初始化麦克风
	err = mic_actor_init();
	if(err != ESP_OK) {
		LOG("APP_INIT: mic actor init failed");
		return err;
	}
	(void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "麦克风初始化完成.", 0);

	

	


	// tests();

	return ESP_OK;
}
