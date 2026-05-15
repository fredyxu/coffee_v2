#include "wifi.h"

#include <string.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#ifndef WIFI_AUTH_WPA2_PSK
#define WIFI_AUTH_WPA2_PSK WIFI_AUTH_WPA_PSK
#endif

#define WIFI_MODULE_SCAN_MAX_APS 20

typedef struct {
    bool ready;
    bool started;
    bool connected;
    bool auto_reconnect;
    char ssid[33];
    char password[65];
    wifi_module_event_cb_t cb;
    void *cb_user;
    esp_event_handler_instance_t wifi_evt_inst;
    esp_event_handler_instance_t ip_evt_inst;
} wifi_ctx_t;

static wifi_ctx_t s_wifi = {0};

static void wifi_emit_event_full(wifi_module_event_id_t id,
                                 int reason,
                                 int rssi,
                                 uint32_t ip,
                                 uint16_t ap_count,
                                 const char *ssid)
{
    if(s_wifi.cb == NULL) {
        return;
    }

    wifi_module_event_t evt = {
        .id = id,
        .reason = reason,
        .rssi = rssi,
        .ip = ip,
        .ap_count = ap_count,
    };
    if(ssid != NULL) {
        strncpy(evt.ssid, ssid, sizeof(evt.ssid) - 1);
        evt.ssid[sizeof(evt.ssid) - 1] = '\0';
    }
    s_wifi.cb(&evt, s_wifi.cb_user);
}

static void wifi_emit_event(wifi_module_event_id_t id, int reason, int rssi, uint32_t ip)
{
    wifi_emit_event_full(id, reason, rssi, ip, 0, NULL);
}

static void wifi_handle_scan_done(const wifi_event_sta_scan_done_t *scan_done)
{
    if(scan_done == NULL || scan_done->status != 0) {
        wifi_emit_event_full(WIFI_MOD_EVT_SCAN_FAILED, scan_done != NULL ? (int)scan_done->status : -1, 0, 0, 0, NULL);
        return;
    }

    uint16_t ap_count = 0;
    esp_err_t err = esp_wifi_scan_get_ap_num(&ap_count);
    if(err != ESP_OK) {
        wifi_emit_event_full(WIFI_MOD_EVT_SCAN_FAILED, (int)err, 0, 0, 0, NULL);
        return;
    }

    wifi_ap_record_t records[WIFI_MODULE_SCAN_MAX_APS] = {0};
    uint16_t record_count = ap_count;
    if(record_count > WIFI_MODULE_SCAN_MAX_APS) {
        record_count = WIFI_MODULE_SCAN_MAX_APS;
    }

    if(record_count > 0) {
        err = esp_wifi_scan_get_ap_records(&record_count, records);
        if(err != ESP_OK) {
            wifi_emit_event_full(WIFI_MOD_EVT_SCAN_FAILED, (int)err, 0, 0, 0, NULL);
            return;
        }
    }

    for(uint16_t i = 0; i < record_count; i++) {
        if(records[i].ssid[0] == '\0') {
            continue;
        }
        wifi_emit_event_full(WIFI_MOD_EVT_SCAN_AP_FOUND, 0, records[i].rssi, 0, ap_count, (const char *)records[i].ssid);
    }

    wifi_emit_event_full(WIFI_MOD_EVT_SCAN_DONE, 0, 0, 0, ap_count, NULL);
}

static esp_err_t wifi_apply_sta_config(void)
{
    wifi_config_t cfg = {0};

    strncpy((char *)cfg.sta.ssid, s_wifi.ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, s_wifi.password, sizeof(cfg.sta.password) - 1);

    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = true;
    cfg.sta.pmf_cfg.required = false;

    return esp_wifi_set_config(WIFI_IF_STA, &cfg);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if(event_base == WIFI_EVENT) {
        if(event_id == WIFI_EVENT_SCAN_DONE) {
            wifi_handle_scan_done((const wifi_event_sta_scan_done_t *)event_data);
            return;
        }

        if(event_id == WIFI_EVENT_STA_START) {
            s_wifi.started = true;
            wifi_emit_event(WIFI_MOD_EVT_STA_START, 0, 0, 0);
            return;
        }

        if(event_id == WIFI_EVENT_STA_CONNECTED) {
            s_wifi.connected = true;

            int rssi = 0;
            wifi_ap_record_t ap = {0};
            if(esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
                rssi = ap.rssi;
            }

            wifi_emit_event(WIFI_MOD_EVT_STA_CONNECTED, 0, rssi, 0);
            return;
        }

        if(event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_wifi.connected = false;

            int reason = 0;
            const wifi_event_sta_disconnected_t *disc = (const wifi_event_sta_disconnected_t *)event_data;
            if(disc != NULL) {
                reason = (int)disc->reason;
            }

            wifi_emit_event(WIFI_MOD_EVT_STA_DISCONNECTED, reason, 0, 0);

            if(s_wifi.auto_reconnect && s_wifi.started) {
                (void)esp_wifi_connect();
            }
            return;
        }
    }

    if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *got_ip = (const ip_event_got_ip_t *)event_data;
        uint32_t ip = (got_ip != NULL) ? got_ip->ip_info.ip.addr : 0;
        wifi_emit_event(WIFI_MOD_EVT_GOT_IP, 0, 0, ip);
    }
}

wifi_module_config_t wifi_module_default_config(void)
{
    wifi_module_config_t cfg = {
        .ssid = "",
        .password = "",
        .auto_reconnect = true,
    };
    return cfg;
}

esp_err_t wifi_module_register_event_callback(wifi_module_event_cb_t cb, void *user_ctx)
{
    s_wifi.cb = cb;
    s_wifi.cb_user = user_ctx;
    return ESP_OK;
}

esp_err_t wifi_module_set_credentials(const char *ssid, const char *password)
{
    if(ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(strlen(ssid) >= sizeof(s_wifi.ssid) || strlen(password) >= sizeof(s_wifi.password)) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_wifi.ssid, ssid, sizeof(s_wifi.ssid) - 1);
    s_wifi.ssid[sizeof(s_wifi.ssid) - 1] = '\0';

    strncpy(s_wifi.password, password, sizeof(s_wifi.password) - 1);
    s_wifi.password[sizeof(s_wifi.password) - 1] = '\0';

    if(s_wifi.ready) {
        return wifi_apply_sta_config();
    }

    return ESP_OK;
}

esp_err_t wifi_module_init(const wifi_module_config_t *cfg)
{
    if(s_wifi.ready) {
        return ESP_OK;
    }

    wifi_module_config_t local_cfg = (cfg != NULL) ? *cfg : wifi_module_default_config();

    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        (void)nvs_flash_erase();
        err = nvs_flash_init();
    }
    if(err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF") == NULL) {
        (void)esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if(err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, &s_wifi.wifi_evt_inst);
    if(err != ESP_OK) {
        (void)esp_wifi_deinit();
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, &s_wifi.ip_evt_inst);
    if(err != ESP_OK) {
        (void)esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi.wifi_evt_inst);
        s_wifi.wifi_evt_inst = NULL;
        (void)esp_wifi_deinit();
        return err;
    }

    (void)esp_wifi_set_storage(WIFI_STORAGE_RAM);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if(err != ESP_OK) {
        (void)esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_wifi.ip_evt_inst);
        (void)esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi.wifi_evt_inst);
        s_wifi.ip_evt_inst = NULL;
        s_wifi.wifi_evt_inst = NULL;
        (void)esp_wifi_deinit();
        return err;
    }

    s_wifi.auto_reconnect = local_cfg.auto_reconnect;
    (void)wifi_module_set_credentials(local_cfg.ssid, local_cfg.password);

    s_wifi.ready = true;
    return ESP_OK;
}

esp_err_t wifi_module_deinit(void)
{
    if(!s_wifi.ready) {
        return ESP_OK;
    }

    (void)wifi_module_stop();

    if(s_wifi.ip_evt_inst != NULL) {
        (void)esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_wifi.ip_evt_inst);
        s_wifi.ip_evt_inst = NULL;
    }

    if(s_wifi.wifi_evt_inst != NULL) {
        (void)esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi.wifi_evt_inst);
        s_wifi.wifi_evt_inst = NULL;
    }

    (void)esp_wifi_deinit();

    s_wifi.ready = false;
    s_wifi.started = false;
    s_wifi.connected = false;
    return ESP_OK;
}

esp_err_t wifi_module_start(void)
{
    if(!s_wifi.ready) {
        return ESP_ERR_INVALID_STATE;
    }

    if(s_wifi.started) {
        return ESP_OK;
    }

    return esp_wifi_start();
}

esp_err_t wifi_module_stop(void)
{
    if(!s_wifi.ready) {
        return ESP_ERR_INVALID_STATE;
    }

    if(!s_wifi.started) {
        return ESP_OK;
    }

    s_wifi.started = false;
    s_wifi.connected = false;
    return esp_wifi_stop();
}

esp_err_t wifi_module_connect(void)
{
    if(!s_wifi.ready || !s_wifi.started) {
        return ESP_ERR_INVALID_STATE;
    }

    if(s_wifi.ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_wifi_connect();
}

esp_err_t wifi_module_disconnect(void)
{
    if(!s_wifi.ready || !s_wifi.started) {
        return ESP_ERR_INVALID_STATE;
    }

    return esp_wifi_disconnect();
}

esp_err_t wifi_module_scan(void)
{
    if(!s_wifi.ready || !s_wifi.started) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };

    return esp_wifi_scan_start(&scan_cfg, false);
}

bool wifi_module_is_ready(void)
{
    return s_wifi.ready;
}

bool wifi_module_is_connected(void)
{
    return s_wifi.connected;
}

esp_err_t wifi_module_get_rssi(int *out_rssi)
{
    if(out_rssi == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!s_wifi.connected) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_ap_record_t ap = {0};
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap);
    if(err != ESP_OK) {
        return err;
    }

    *out_rssi = ap.rssi;
    return ESP_OK;
}
