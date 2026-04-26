#include "store.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "config/config_sys.h"
#include "nvs.h"
#include "nvs_flash.h"

#define STORE_NAMESPACE "app_store"
#define STORE_KEY_WIFI_SSID "wifi_ssid"
#define STORE_KEY_WIFI_PWD "wifi_pwd"
#define STORE_KEY_WIFI_AUTO "wifi_auto"
#define STORE_KEY_WIFI_RSSI "wifi_rssi"

typedef struct {
    bool inited;
    SemaphoreHandle_t lock;
} store_ctx_t;

static store_ctx_t s_store = {0};

store_wifi_settings_t store_default_wifi_settings(void)
{
    store_wifi_settings_t out = {
        .ssid = WIFI_STA_SSID,
        .password = WIFI_STA_PASSWORD,
        .auto_reconnect = (WIFI_AUTO_RECONNECT != 0),
        .weak_rssi_threshold = WIFI_WEAK_RSSI_THRESHOLD,
    };
    return out;
}

static esp_err_t store_ensure_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        (void)nvs_flash_erase();
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t store_init(void)
{
    if(s_store.inited) {
        return ESP_OK;
    }

    esp_err_t err = store_ensure_nvs();
    if(err != ESP_OK) {
        return err;
    }

    s_store.lock = xSemaphoreCreateMutex();
    if(s_store.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_store.inited = true;
    return ESP_OK;
}

esp_err_t store_deinit(void)
{
    if(!s_store.inited) {
        return ESP_OK;
    }

    if(s_store.lock != NULL) {
        vSemaphoreDelete(s_store.lock);
        s_store.lock = NULL;
    }

    s_store.inited = false;
    return ESP_OK;
}

esp_err_t store_load_wifi_settings(store_wifi_settings_t *out_settings)
{
    if(!s_store.inited || out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_settings = store_default_wifi_settings();

    (void)xSemaphoreTake(s_store.lock, portMAX_DELAY);

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(STORE_NAMESPACE, NVS_READONLY, &nvs);
    if(err == ESP_ERR_NVS_NOT_FOUND) {
        (void)xSemaphoreGive(s_store.lock);
        return ESP_OK;
    }
    if(err != ESP_OK) {
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }

    size_t ssid_len = sizeof(out_settings->ssid);
    err = nvs_get_str(nvs, STORE_KEY_WIFI_SSID, out_settings->ssid, &ssid_len);
    if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }

    size_t pwd_len = sizeof(out_settings->password);
    err = nvs_get_str(nvs, STORE_KEY_WIFI_PWD, out_settings->password, &pwd_len);
    if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }

    uint8_t auto_rc = out_settings->auto_reconnect ? 1 : 0;
    err = nvs_get_u8(nvs, STORE_KEY_WIFI_AUTO, &auto_rc);
    if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }
    out_settings->auto_reconnect = (auto_rc != 0);

    int32_t weak_rssi = out_settings->weak_rssi_threshold;
    err = nvs_get_i32(nvs, STORE_KEY_WIFI_RSSI, &weak_rssi);
    if(err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(nvs);
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }
    out_settings->weak_rssi_threshold = (int)weak_rssi;

    nvs_close(nvs);
    (void)xSemaphoreGive(s_store.lock);
    return ESP_OK;
}

esp_err_t store_save_wifi_settings(const store_wifi_settings_t *settings)
{
    if(!s_store.inited || settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)xSemaphoreTake(s_store.lock, portMAX_DELAY);

    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(STORE_NAMESPACE, NVS_READWRITE, &nvs);
    if(err != ESP_OK) {
        (void)xSemaphoreGive(s_store.lock);
        return err;
    }

    err = nvs_set_str(nvs, STORE_KEY_WIFI_SSID, settings->ssid);
    if(err == ESP_OK) {
        err = nvs_set_str(nvs, STORE_KEY_WIFI_PWD, settings->password);
    }
    if(err == ESP_OK) {
        err = nvs_set_u8(nvs, STORE_KEY_WIFI_AUTO, settings->auto_reconnect ? 1 : 0);
    }
    if(err == ESP_OK) {
        err = nvs_set_i32(nvs, STORE_KEY_WIFI_RSSI, (int32_t)settings->weak_rssi_threshold);
    }
    if(err == ESP_OK) {
        err = nvs_commit(nvs);
    }

    nvs_close(nvs);
    (void)xSemaphoreGive(s_store.lock);
    return err;
}

esp_err_t store_load_settings(store_settings_t *out_settings)
{
    if(out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    out_settings->wifi = store_default_wifi_settings();
    return store_load_wifi_settings(&out_settings->wifi);
}

esp_err_t store_save_settings(const store_settings_t *settings)
{
    if(settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return store_save_wifi_settings(&settings->wifi);
}

