#include "app_settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config/config_sys.h"
#include "config/config_sys_key.h"
#include "core/store/store_kv.h"
#include "core/utils/log.h"

#ifdef ESP_PLATFORM
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "modules/audio/audio.h"
#include "modules/display/lcd_i80_8.h"
#endif

#define APP_SETTINGS_NS_WIFI "wifi"
#define APP_SETTINGS_NS_WS "ws"
#define APP_SETTINGS_NS_KEY "key"
#define APP_SETTINGS_NS_AUDIO "audio"
#define APP_SETTINGS_NS_DISPLAY "display"

#define APP_SETTINGS_SAVE_DEBOUNCE_TICKS pdMS_TO_TICKS(500)
#define APP_SETTINGS_SAVE_TASK_STACK 4096

typedef struct {
    app_setting_id_t id;
    store_kv_type_t type;
    const char *namespace_name;
    const char *key;
    void *target;
    size_t target_size;
    int32_t min_i32;
    int32_t max_i32;
    uint32_t dirty_bit;
	app_setting_value_t default_value;
} app_setting_desc_t;

app_settings_t app_settings = {0};

static const app_setting_desc_t s_setting_descs[APP_SETTING_ID_MAX] = {
    [APP_SETTING_ID_WIFI_ENABLE] = {
        .id = APP_SETTING_ID_WIFI_ENABLE,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_WIFI,
        .key = "enable",
        .target = &app_settings.wifi_enable,
        .target_size = sizeof(app_settings.wifi_enable),
        .dirty_bit = (1u << APP_SETTING_ID_WIFI_ENABLE),
		.default_value.b = true,
    },
    [APP_SETTING_ID_WIFI_SSID] = {
        .id = APP_SETTING_ID_WIFI_SSID,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_WIFI,
        .key = "ssid",
        .target = app_settings.wifi_ssid,
        .target_size = sizeof(app_settings.wifi_ssid),
        .dirty_bit = (1u << APP_SETTING_ID_WIFI_SSID),
		.default_value.str = "",
    },
    [APP_SETTING_ID_WIFI_PASSWORD] = {
        .id = APP_SETTING_ID_WIFI_PASSWORD,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_WIFI,
        .key = "password",
        .target = app_settings.wifi_password,
        .target_size = sizeof(app_settings.wifi_password),
        .dirty_bit = (1u << APP_SETTING_ID_WIFI_PASSWORD),
		.default_value.str = "",
    },
    [APP_SETTING_ID_WIFI_AUTO_RECONNECT] = {
        .id = APP_SETTING_ID_WIFI_AUTO_RECONNECT,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_WIFI,
        .key = "auto_reconnect",
        .target = &app_settings.wifi_auto_reconnect,
        .target_size = sizeof(app_settings.wifi_auto_reconnect),
        .dirty_bit = (1u << APP_SETTING_ID_WIFI_AUTO_RECONNECT),
		.default_value.b = true,
    },
    [APP_SETTING_ID_WIFI_WEAK_RSSI_THRESHOLD] = {
        .id = APP_SETTING_ID_WIFI_WEAK_RSSI_THRESHOLD,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_WIFI,
        .key = "weak_rssi",
        .target = &app_settings.wifi_weak_rssi_threshold,
        .target_size = sizeof(app_settings.wifi_weak_rssi_threshold),
        .min_i32 = -100,
        .max_i32 = -30,
        .dirty_bit = (1u << APP_SETTING_ID_WIFI_WEAK_RSSI_THRESHOLD),
		.default_value.i32 = -70,
    },
    [APP_SETTING_ID_WS_ENABLE] = {
        .id = APP_SETTING_ID_WS_ENABLE,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_WS,
        .key = "enable",
        .target = &app_settings.ws_enable,
        .target_size = sizeof(app_settings.ws_enable),
        .dirty_bit = (1u << APP_SETTING_ID_WS_ENABLE),
		.default_value.b = WS_DEFAULT_ENABLE != 0,
    },
    [APP_SETTING_ID_WS_URL] = {
        .id = APP_SETTING_ID_WS_URL,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_WS,
        .key = "url",
        .target = app_settings.ws_url,
        .target_size = sizeof(app_settings.ws_url),
        .dirty_bit = (1u << APP_SETTING_ID_WS_URL),
		.default_value.str = WS_DEFAULT_URL,
    },
    [APP_SETTING_ID_WS_ROOM] = {
        .id = APP_SETTING_ID_WS_ROOM,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_WS,
        .key = "room",
        .target = app_settings.ws_room,
        .target_size = sizeof(app_settings.ws_room),
        .dirty_bit = (1u << APP_SETTING_ID_WS_ROOM),
		.default_value.str = WS_DEFAULT_ROOM,
    },
    [APP_SETTING_ID_WS_CALLSIGN] = {
        .id = APP_SETTING_ID_WS_CALLSIGN,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_WS,
        .key = "callsign",
        .target = app_settings.ws_callsign,
        .target_size = sizeof(app_settings.ws_callsign),
        .dirty_bit = (1u << APP_SETTING_ID_WS_CALLSIGN),
		.default_value.str = WS_DEFAULT_CALLSIGN,
    },
    [APP_SETTING_ID_WS_AUTO_RECONNECT] = {
        .id = APP_SETTING_ID_WS_AUTO_RECONNECT,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_WS,
        .key = "auto_reconnect",
        .target = &app_settings.ws_auto_reconnect,
        .target_size = sizeof(app_settings.ws_auto_reconnect),
        .dirty_bit = (1u << APP_SETTING_ID_WS_AUTO_RECONNECT),
		.default_value.b = WS_DEFAULT_AUTO_RECONNECT != 0,
    },
    [APP_SETTING_ID_KEY_ENABLE] = {
        .id = APP_SETTING_ID_KEY_ENABLE,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "enable",
        .target = &app_settings.key_enable,
        .target_size = sizeof(app_settings.key_enable),
        .dirty_bit = (1u << APP_SETTING_ID_KEY_ENABLE),
		.default_value.b = KEY_DEFAULT_ENABLE != 0,
    },
    [APP_SETTING_ID_KEY_MODE] = {
        .id = APP_SETTING_ID_KEY_MODE,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "mode",
        .target = &app_settings.key_mode,
        .target_size = sizeof(app_settings.key_mode),
        .min_i32 = KEY_MODE_MANUAL,
        .max_i32 = KEY_MODE_AUTO,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_MODE),
		.default_value.i32 = KEY_DEFAULT_MODE,
    },
    [APP_SETTING_ID_KEY_ACTIVE_LEVEL] = {
        .id = APP_SETTING_ID_KEY_ACTIVE_LEVEL,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "active_level",
        .target = &app_settings.key_active_level,
        .target_size = sizeof(app_settings.key_active_level),
        .min_i32 = 0,
        .max_i32 = 1,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_ACTIVE_LEVEL),
		.default_value.i32 = KEY_DEFAULT_ACTIVE_LEVEL,
    },
    [APP_SETTING_ID_KEY_SWAP_AB] = {
        .id = APP_SETTING_ID_KEY_SWAP_AB,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "swap_ab",
        .target = &app_settings.key_swap_ab,
        .target_size = sizeof(app_settings.key_swap_ab),
        .dirty_bit = (1u << APP_SETTING_ID_KEY_SWAP_AB),
		.default_value.b = KEY_DEFAULT_SWAP_AB != 0,
    },
    [APP_SETTING_ID_KEY_DEBOUNCE_MS] = {
        .id = APP_SETTING_ID_KEY_DEBOUNCE_MS,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "debounce_ms",
        .target = &app_settings.key_debounce_ms,
        .target_size = sizeof(app_settings.key_debounce_ms),
        .min_i32 = 0,
        .max_i32 = INT32_MAX,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_DEBOUNCE_MS),
		.default_value.i32 = KEY_DEFAULT_DEBOUNCE_MS,
    },
    [APP_SETTING_ID_KEY_MANUAL_DI_MS] = {
        .id = APP_SETTING_ID_KEY_MANUAL_DI_MS,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "manual_di_ms",
        .target = &app_settings.key_manual_di_ms,
        .target_size = sizeof(app_settings.key_manual_di_ms),
        .min_i32 = 1,
        .max_i32 = INT32_MAX,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_MANUAL_DI_MS),
		.default_value.i32 = KEY_DEFAULT_MANUAL_DI_MS,
    },
    [APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE] = {
        .id = APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "manual_adaptive",
        .target = &app_settings.key_manual_adaptive_enable,
        .target_size = sizeof(app_settings.key_manual_adaptive_enable),
        .dirty_bit = (1u << APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE),
		.default_value.b = KEY_DEFAULT_MANUAL_ADAPTIVE_ENABLE != 0,
    },
    [APP_SETTING_ID_KEY_AUTO_DI_MS] = {
        .id = APP_SETTING_ID_KEY_AUTO_DI_MS,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "auto_di_ms",
        .target = &app_settings.key_auto_di_ms,
        .target_size = sizeof(app_settings.key_auto_di_ms),
        .min_i32 = 1,
        .max_i32 = INT32_MAX,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_AUTO_DI_MS),
		.default_value.i32 = KEY_DEFAULT_AUTO_DI_MS,
    },
    [APP_SETTING_ID_KEY_AUTO_DA_RATIO] = {
        .id = APP_SETTING_ID_KEY_AUTO_DA_RATIO,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "auto_da_ratio",
        .target = app_settings.key_auto_da_ratio,
        .target_size = sizeof(app_settings.key_auto_da_ratio),
        .dirty_bit = (1u << APP_SETTING_ID_KEY_AUTO_DA_RATIO),
		.default_value.str = KEY_DEFAULT_AUTO_DA_RATIO,
    },
    [APP_SETTING_ID_KEY_AUTO_GAP_RATIO] = {
        .id = APP_SETTING_ID_KEY_AUTO_GAP_RATIO,
        .type = STORE_KV_STR,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "auto_gap_ratio",
        .target = app_settings.key_auto_gap_ratio,
        .target_size = sizeof(app_settings.key_auto_gap_ratio),
        .dirty_bit = (1u << APP_SETTING_ID_KEY_AUTO_GAP_RATIO),
		.default_value.str = KEY_DEFAULT_AUTO_GAP_RATIO,
    },
    [APP_SETTING_ID_KEY_TONE_HZ] = {
        .id = APP_SETTING_ID_KEY_TONE_HZ,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_KEY,
        .key = "tone_hz",
        .target = &app_settings.key_tone_hz,
        .target_size = sizeof(app_settings.key_tone_hz),
        .min_i32 = 1,
        .max_i32 = INT32_MAX,
        .dirty_bit = (1u << APP_SETTING_ID_KEY_TONE_HZ),
		.default_value.i32 = KEY_DEFAULT_TONE_HZ,
    },
    [APP_SETTING_ID_AUDIO_VOLUME] = {
        .id = APP_SETTING_ID_AUDIO_VOLUME,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_AUDIO,
        .key = "volume",
        .target = &app_settings.audio_volume,
        .target_size = sizeof(app_settings.audio_volume),
        .min_i32 = 0,
        .max_i32 = 100,
        .dirty_bit = (1u << APP_SETTING_ID_AUDIO_VOLUME),
		.default_value.i32 = 50,
    },
    [APP_SETTING_ID_DISPLAY_BRIGHTNESS] = {
        .id = APP_SETTING_ID_DISPLAY_BRIGHTNESS,
        .type = STORE_KV_I32,
        .namespace_name = APP_SETTINGS_NS_DISPLAY,
        .key = "brightness",
        .target = &app_settings.display_brightness,
        .target_size = sizeof(app_settings.display_brightness),
        .min_i32 = 0,
        .max_i32 = 100,
        .dirty_bit = (1u << APP_SETTING_ID_DISPLAY_BRIGHTNESS),
		.default_value.i32 = 50,
    },
    [APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE] = {
        .id = APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE,
        .type = STORE_KV_BOOL,
        .namespace_name = APP_SETTINGS_NS_DISPLAY,
        .key = "cw_decode",
        .target = &app_settings.cw_decode_display_enable,
        .target_size = sizeof(app_settings.cw_decode_display_enable),
        .dirty_bit = (1u << APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE),
		.default_value.b = false,
    },
};

#ifdef ESP_PLATFORM
static SemaphoreHandle_t s_settings_lock;
static TaskHandle_t s_save_task;
static uint32_t s_dirty_mask;
static bool s_save_in_progress;
static esp_err_t s_last_save_err = ESP_OK;
#endif




static const app_setting_desc_t *app_settings_get_desc(app_setting_id_t id)
{
    if(id < 0 || id >= APP_SETTING_ID_MAX) {
        return NULL;
    }

    const app_setting_desc_t *desc = &s_setting_descs[id];
    if(desc->id != id || desc->target == NULL || desc->namespace_name == NULL || desc->key == NULL) {
        return NULL;
    }

    return desc;
}

static int32_t app_settings_clamp_i32(const app_setting_desc_t *desc, int32_t value)
{
    if(value < desc->min_i32) {
        return desc->min_i32;
    }
    if(value > desc->max_i32) {
        return desc->max_i32;
    }
    return value;
}

static store_kv_item_t app_settings_make_store_item(const app_setting_desc_t *desc)
{
    return (store_kv_item_t) {
        .namespace_name = desc->namespace_name,
        .key = desc->key,
        .type = desc->type,
        .value = desc->target,
        .value_size = desc->target_size,
    };
}

static void app_settings_normalize_loaded_value(const app_setting_desc_t *desc)
{
    if(desc->type == STORE_KV_I32) {
        int32_t *value = (int32_t *)desc->target;
        *value = app_settings_clamp_i32(desc, *value);
    } else if(desc->type == STORE_KV_STR && desc->target_size > 0) {
        char *value = (char *)desc->target;
        value[desc->target_size - 1] = '\0';
    }
}

static esp_err_t app_settings_load_all(void)
{
    esp_err_t first_err = ESP_OK;

    for(app_setting_id_t id = 0; id < APP_SETTING_ID_MAX; id++) {
        const app_setting_desc_t *desc = app_settings_get_desc(id);
        if(desc == NULL) {
            return ESP_ERR_INVALID_STATE;
        }

        store_kv_item_t item = app_settings_make_store_item(desc);
        esp_err_t err = store_kv_load(&item);
        if(err == ESP_OK) {
            app_settings_normalize_loaded_value(desc);
            continue;
        }

#ifdef ESP_PLATFORM
        if(err == ESP_ERR_NOT_FOUND) {
            continue;
        }
#else
        if(err == ESP_ERR_NOT_SUPPORTED) {
            continue;
        }
#endif

        if(first_err == ESP_OK) {
            first_err = err;
        }
    }

    return first_err;
}

static esp_err_t app_settings_apply_update(const app_setting_desc_t *desc, const app_setting_value_t *value)
{
    if(desc == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch(desc->type) {
        case STORE_KV_BOOL:
            *(bool *)desc->target = value->b;
            return ESP_OK;
        case STORE_KV_I32:
            *(int32_t *)desc->target = app_settings_clamp_i32(desc, value->i32);
            return ESP_OK;
        case STORE_KV_STR:
            snprintf((char *)desc->target, desc->target_size, "%s", value->str != NULL ? value->str : "");
            return ESP_OK;
        default:
            return ESP_ERR_INVALID_ARG;
    }
}

#ifdef ESP_PLATFORM
static void app_settings_apply_runtime(app_setting_id_t id)
{
    switch(id) {
        case APP_SETTING_ID_AUDIO_VOLUME:
            (void)audio_set_volume((uint8_t)app_settings.audio_volume);
            break;

        case APP_SETTING_ID_DISPLAY_BRIGHTNESS:
            (void)lcd_set_backlight((uint8_t)app_settings.display_brightness);
            break;

        default:
            break;
    }
}

static void *app_settings_snapshot_target(const app_settings_t *snapshot, app_setting_id_t id)
{
    switch(id) {
        case APP_SETTING_ID_WIFI_ENABLE:
            return (void *)&snapshot->wifi_enable;
        case APP_SETTING_ID_WIFI_SSID:
            return (void *)snapshot->wifi_ssid;
        case APP_SETTING_ID_WIFI_PASSWORD:
            return (void *)snapshot->wifi_password;
        case APP_SETTING_ID_WIFI_AUTO_RECONNECT:
            return (void *)&snapshot->wifi_auto_reconnect;
        case APP_SETTING_ID_WIFI_WEAK_RSSI_THRESHOLD:
            return (void *)&snapshot->wifi_weak_rssi_threshold;
        case APP_SETTING_ID_WS_ENABLE:
            return (void *)&snapshot->ws_enable;
        case APP_SETTING_ID_WS_URL:
            return (void *)snapshot->ws_url;
        case APP_SETTING_ID_WS_ROOM:
            return (void *)snapshot->ws_room;
        case APP_SETTING_ID_WS_CALLSIGN:
            return (void *)snapshot->ws_callsign;
        case APP_SETTING_ID_WS_AUTO_RECONNECT:
            return (void *)&snapshot->ws_auto_reconnect;
        case APP_SETTING_ID_KEY_ENABLE:
            return (void *)&snapshot->key_enable;
        case APP_SETTING_ID_KEY_MODE:
            return (void *)&snapshot->key_mode;
        case APP_SETTING_ID_KEY_ACTIVE_LEVEL:
            return (void *)&snapshot->key_active_level;
        case APP_SETTING_ID_KEY_SWAP_AB:
            return (void *)&snapshot->key_swap_ab;
        case APP_SETTING_ID_KEY_DEBOUNCE_MS:
            return (void *)&snapshot->key_debounce_ms;
        case APP_SETTING_ID_KEY_MANUAL_DI_MS:
            return (void *)&snapshot->key_manual_di_ms;
        case APP_SETTING_ID_KEY_MANUAL_ADAPTIVE_ENABLE:
            return (void *)&snapshot->key_manual_adaptive_enable;
        case APP_SETTING_ID_KEY_AUTO_DI_MS:
            return (void *)&snapshot->key_auto_di_ms;
        case APP_SETTING_ID_KEY_AUTO_DA_RATIO:
            return (void *)snapshot->key_auto_da_ratio;
        case APP_SETTING_ID_KEY_AUTO_GAP_RATIO:
            return (void *)snapshot->key_auto_gap_ratio;
        case APP_SETTING_ID_KEY_TONE_HZ:
            return (void *)&snapshot->key_tone_hz;
        case APP_SETTING_ID_AUDIO_VOLUME:
            return (void *)&snapshot->audio_volume;
        case APP_SETTING_ID_DISPLAY_BRIGHTNESS:
            return (void *)&snapshot->display_brightness;
        case APP_SETTING_ID_CW_DECODE_DISPLAY_ENABLE:
            return (void *)&snapshot->cw_decode_display_enable;
        default:
            return NULL;
    }
}

static esp_err_t app_settings_save_dirty(uint32_t dirty_mask, const app_settings_t *snapshot)
{
    esp_err_t first_err = ESP_OK;

    for(app_setting_id_t id = 0; id < APP_SETTING_ID_MAX; id++) {
        const app_setting_desc_t *desc = app_settings_get_desc(id);
        if(desc == NULL || (dirty_mask & desc->dirty_bit) == 0) {
            continue;
        }

        store_kv_item_t item = app_settings_make_store_item(desc);
        item.value = app_settings_snapshot_target(snapshot, id);
        if(item.value == NULL) {
            if(first_err == ESP_OK) {
                first_err = ESP_ERR_INVALID_STATE;
            }
            continue;
        }

        esp_err_t err = store_kv_save(&item);
        if(err != ESP_OK) {
            if(first_err == ESP_OK) {
                first_err = err;
            }
            LOG("app settings save failed: id=%d err=%s", (int)id, esp_err_to_name(err));
        }
    }

    return first_err;
}

static void app_settings_save_task(void *arg)
{
    (void)arg;

    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(APP_SETTINGS_SAVE_DEBOUNCE_TICKS);

        uint32_t dirty_mask = 0;
        app_settings_t snapshot = {0};
        if(xSemaphoreTake(s_settings_lock, portMAX_DELAY) == pdTRUE) {
            dirty_mask = s_dirty_mask;
            s_dirty_mask = 0;
            snapshot = app_settings;
            s_save_in_progress = (dirty_mask != 0);
            s_last_save_err = ESP_OK;
            xSemaphoreGive(s_settings_lock);
        }

        if(dirty_mask != 0) {
            esp_err_t err = app_settings_save_dirty(dirty_mask, &snapshot);
            if(xSemaphoreTake(s_settings_lock, portMAX_DELAY) == pdTRUE) {
                s_save_in_progress = false;
                s_last_save_err = err;
                if(err != ESP_OK) {
                    s_dirty_mask |= dirty_mask;
                }
                xSemaphoreGive(s_settings_lock);
            }
        }
    }
}

static esp_err_t app_settings_start_save_task(void)
{
    if(s_settings_lock == NULL) {
        s_settings_lock = xSemaphoreCreateMutex();
        if(s_settings_lock == NULL) {
            return ESP_FAIL;
        }
    }

    if(s_save_task != NULL) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(
        app_settings_save_task,
        "app_settings",
        APP_SETTINGS_SAVE_TASK_STACK,
        NULL,
        TASK_PRIO_CON,
        &s_save_task
    );

    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}
#endif

static void app_settings_load_defaults(void)
{
    for(app_setting_id_t id = 0; id < APP_SETTING_ID_MAX; id++) {
        const app_setting_desc_t *desc = app_settings_get_desc(id);
        if(desc == NULL) {
            continue;
        }

        switch(desc->type) {
            case STORE_KV_BOOL:
                *(bool *)desc->target = desc->default_value.b;
                break;

            case STORE_KV_I32:
                *(int32_t *)desc->target =
                    app_settings_clamp_i32(desc, desc->default_value.i32);
                break;

            case STORE_KV_STR:
                snprintf((char *)desc->target,
                         desc->target_size,
                         "%s",
                         desc->default_value.str ? desc->default_value.str : "");
                break;

            default:
                break;
        }
    }
}
esp_err_t app_settings_init(void)
{
    esp_err_t err = store_kv_init();
    if(err != ESP_OK) {
        return err;
    }

    app_settings_load_defaults();

    err = app_settings_load_all();
    if(err != ESP_OK) {
        return err;
    }

#ifdef ESP_PLATFORM
    return app_settings_start_save_task();
#else
    return ESP_OK;
#endif
}

esp_err_t app_settings_update(const app_settings_update_t *update)
{
    if(update == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

	// LOG("app_settings_update: id=%d", (int)update->id);	

    const app_setting_desc_t *desc = app_settings_get_desc(update->id);
    if(desc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    if(s_settings_lock != NULL && xSemaphoreTake(s_settings_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }
#endif

    esp_err_t err = app_settings_apply_update(desc, &update->value);

#ifdef ESP_PLATFORM
    if(err == ESP_OK) {
        s_dirty_mask |= desc->dirty_bit;
        app_settings_apply_runtime(update->id);
    }
    if(s_settings_lock != NULL) {
        xSemaphoreGive(s_settings_lock);
    }
    if(err == ESP_OK && s_save_task != NULL) {
        xTaskNotifyGive(s_save_task);
    }
#endif

    return err;
}

esp_err_t app_settings_flush(TickType_t timeout_ticks)
{
#ifdef ESP_PLATFORM
    if(s_save_task != NULL) {
        xTaskNotifyGive(s_save_task);
    }

    TickType_t start = xTaskGetTickCount();
    while(1) {
        uint32_t dirty_mask = 0;
        bool save_in_progress = false;
        esp_err_t last_err = ESP_OK;

        if(s_settings_lock != NULL && xSemaphoreTake(s_settings_lock, portMAX_DELAY) == pdTRUE) {
            dirty_mask = s_dirty_mask;
            save_in_progress = s_save_in_progress;
            last_err = s_last_save_err;
            xSemaphoreGive(s_settings_lock);
        }

        if(dirty_mask == 0 && !save_in_progress) {
            return last_err;
        }

        if(timeout_ticks == 0 || (xTaskGetTickCount() - start) >= timeout_ticks) {
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
#else
    (void)timeout_ticks;
    return ESP_OK;
#endif
}
