#include "store_kv.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef ESP_PLATFORM
#include "nvs.h"
#include "nvs_flash.h"

#include "core/utils/log.h"

static esp_err_t store_kv_normalize_err(esp_err_t err)
{
    return err == ESP_ERR_NVS_NOT_FOUND ? ESP_ERR_NOT_FOUND : err;
}
#endif

static bool store_kv_item_valid(const store_kv_item_t *item)
{
    if(item == NULL || item->namespace_name == NULL || item->key == NULL || item->value == NULL) {
        return false;
    }

    if(item->type == STORE_KV_STR && item->value_size == 0) {
        return false;
    }

    return true;
}

esp_err_t store_kv_init(void)
{
#ifdef ESP_PLATFORM
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        LOG("NVS init failed without erase: %s", esp_err_to_name(err));
        return err;
    }
    if(err == ESP_ERR_INVALID_STATE) {
        return ESP_OK;
    }
    return err;
#else
    return ESP_OK;
#endif
}

esp_err_t store_kv_load(const store_kv_item_t *item)
{
    if(!store_kv_item_valid(item)) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(item->namespace_name, NVS_READONLY, &handle);
    if(err != ESP_OK) {
        return store_kv_normalize_err(err);
    }

    switch(item->type) {
        case STORE_KV_BOOL: {
            uint8_t value = 0;
            err = nvs_get_u8(handle, item->key, &value);
            if(err == ESP_OK) {
                *(bool *)item->value = (value != 0);
            }
            break;
        }
        case STORE_KV_I32:
            err = nvs_get_i32(handle, item->key, (int32_t *)item->value);
            break;
        case STORE_KV_STR:
            size_t str_size = item->value_size;
            err = nvs_get_str(handle, item->key, (char *)item->value, &str_size);
            break;
        default:
            err = ESP_ERR_INVALID_ARG;
            break;
    }

    nvs_close(handle);
    return store_kv_normalize_err(err);
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t store_kv_save(const store_kv_item_t *item)
{
    if(!store_kv_item_valid(item)) {
        return ESP_ERR_INVALID_ARG;
    }

#ifdef ESP_PLATFORM
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(item->namespace_name, NVS_READWRITE, &handle);
    if(err != ESP_OK) {
        return err;
    }

    switch(item->type) {
        case STORE_KV_BOOL:
            err = nvs_set_u8(handle, item->key, *(const bool *)item->value ? 1 : 0);
            break;
        case STORE_KV_I32:
            err = nvs_set_i32(handle, item->key, *(const int32_t *)item->value);
            break;
        case STORE_KV_STR:
            err = nvs_set_str(handle, item->key, (const char *)item->value);
            break;
        default:
            err = ESP_ERR_INVALID_ARG;
            break;
    }

    if(err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}
