file(GLOB SRC_FILES
		"*.c" 
		"config/*.c"
		"app/*.c"
		"core/*/*.c"
		"core/*/*/*.c"
		"modules/*/*.c"
		"modules/*/*/*.c"
		"modules/*/*/*/*.c"
		"moudels/touch/*.c"
		"modules/ui/page/*/*.c"
		"modules/ui/page/*/*/*.c"
		"modules/ui/page/*/*/*/*.c"
		"modules/ui/font/*.c"
		"modules/ui/adapter/*.c"
		"tests/*.c"
		"tests/*/*.c"
)

list(APPEND SRC_FILES
		"core/msg/msg_sub.c"
		"modules/ws/ws_cw_cache.c"
)
list(REMOVE_DUPLICATES SRC_FILES)

idf_component_register(
	SRCS ${SRC_FILES}
	INCLUDE_DIRS 
		"."
		"modules"
		"core"

	REQUIRES esp_lcd driver esp_timer lvgl esp_wifi esp_event esp_netif nvs_flash esp_websocket_client mbedtls json
)
