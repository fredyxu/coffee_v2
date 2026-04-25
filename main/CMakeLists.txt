file(GLOB SRC_FILES 
		"*.c" 
		"app/*.c"
		"core/*/*.c"
		"core/*/*/*.c"
		"modules/*/*.c"
		"modules/*/*/*.c"
		"modules/*/*/*/*.c"
		"moudels/touch/*.c"
		"modules/ui/page/*/*.c"
		"modules/ui/page/*/*/*.c"
		"modules/ui/font/*.c"
		"modules/ui/adapter/*.c"
		"tests/*.c"
		"tests/*/*.c"
)

idf_component_register(
	SRCS ${SRC_FILES}
	INCLUDE_DIRS 
		"."
		"modules"
		"core"

	REQUIRES esp_lcd driver esp_timer lvgl
)
