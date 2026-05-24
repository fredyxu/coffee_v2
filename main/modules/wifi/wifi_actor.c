#include "wifi_actor.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "app/app_settings.h"
#include "config/config_sys_task.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "modules/wifi/wifi.h"
#include "modules/wifi/wifi_scan_cache.h"
#include "modules/wifi/wifi_settings.h"

#define WIFI_ACTOR_INBOX_Q_LEN 8

typedef struct {
    QueueHandle_t inbox_q;
    msg_sub_handle_t sub;
    TaskHandle_t task;
    bool weak_reported;
    bool connect_after_scan;
    int last_signal_level;
    int weak_rssi_threshold;
} wifi_actor_ctx_t;

static wifi_actor_ctx_t s_actor = {0};

static const TickType_t WIFI_ACTOR_LOOP_TICKS = pdMS_TO_TICKS(500);

static int wifi_actor_calc_signal_level(int rssi, int weak_threshold)
{
    if(rssi <= weak_threshold) {
        return 1;
    }
    if(rssi <= weak_threshold + 8) {
        return 2;
    }
    if(rssi <= weak_threshold + 15) {
        return 3;
    }
    return 4;
}

static void wifi_actor_emit_sys_event(const wifi_module_event_t *event)
{
    if(event == NULL) {
        return;
    }

	// LOG("EVENT %d:", event->id);

    switch(event->id) {
        case WIFI_MOD_EVT_GOT_IP:
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_CONNECTED, 1, 0);
            break;
        case WIFI_MOD_EVT_STA_DISCONNECTED:
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_DISCONNECTED, event->reason, 0);
            break;
        case WIFI_MOD_EVT_SCAN_AP_FOUND: {
            wifi_scan_ap_t ap = {
                .rssi = event->rssi,
                .authmode = event->authmode,
                .channel = event->channel,
            };
            (void)strncpy(ap.ssid, event->ssid, sizeof(ap.ssid) - 1);
            ap.ssid[sizeof(ap.ssid) - 1] = '\0';
            (void)wifi_scan_cache_add(&ap);
            wifi_settings_ssid_add_ap(&ap);

            msg_t msg = msg_make(MSG_SRC_WIFI, MSG_TYPE_SYS, MSG_EVT_SYS_WIFI_SCAN_AP_FOUND, (uint32_t)xTaskGetTickCount());
            (void)strncpy(msg.data.wifi_ap.ssid, ap.ssid, sizeof(msg.data.wifi_ap.ssid) - 1);
            msg.data.wifi_ap.ssid[sizeof(msg.data.wifi_ap.ssid) - 1] = '\0';
            msg.data.wifi_ap.rssi = ap.rssi;
            msg.data.wifi_ap.authmode = ap.authmode;
            msg.data.wifi_ap.channel = ap.channel;
            (void)msg_send_sys(&msg, 0);
            break;
        }
        case WIFI_MOD_EVT_SCAN_DONE:
            if(event->ap_count == 0) {
                wifi_settings_ssid_set_empty_result();
            }
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_DONE, event->ap_count, 0);
            if(s_actor.connect_after_scan) {
                s_actor.connect_after_scan = false;
                (void)wifi_module_connect();
            }
            break;
        case WIFI_MOD_EVT_SCAN_FAILED:
            wifi_settings_ssid_set_status("扫描失败");
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_FAILED, event->reason, 0);
            if(s_actor.connect_after_scan) {
                s_actor.connect_after_scan = false;
                (void)wifi_module_connect();
            }
            break;
        default:
            break;
    }
}

static void wifi_actor_wifi_event_cb(const wifi_module_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    wifi_actor_emit_sys_event(event);
}

static esp_err_t wifi_actor_update_enable_setting(bool enable)
{
    return app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_WIFI_ENABLE,
        .value.b = enable,
    });
}

static esp_err_t wifi_actor_update_credentials_setting(const char *ssid, const char *password)
{
    esp_err_t err = app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_WIFI_SSID,
        .value.str = ssid,
    });
    if(err != ESP_OK) {
        return err;
    }

    return app_settings_update(&(app_settings_update_t) {
        .id = APP_SETTING_ID_WIFI_PASSWORD,
        .value.str = password,
    });
}

static esp_err_t wifi_actor_scan_networks(void)
{
    if(!app_settings.wifi_enable) {
        wifi_scan_cache_clear();
        wifi_settings_ssid_set_status("WIFI已关闭");
        (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_FAILED, ESP_ERR_INVALID_STATE, 0);
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_cache_clear();
    wifi_settings_ssid_set_status("扫描中...");
    (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_STARTED, 1, 0);

    esp_err_t err = wifi_module_scan();
    if(err != ESP_OK) {
        wifi_settings_ssid_set_status("扫描失败");
        (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_FAILED, (int)err, 0);
    }

    return err;
}

static esp_err_t wifi_actor_set_enable(bool enable)
{
    esp_err_t err = wifi_actor_update_enable_setting(enable);
    if(err != ESP_OK) {
        return err;
    }

    if(enable) {
        err = wifi_module_start();
        if(err != ESP_OK) {
            return err;
        }

        if(app_settings.wifi_ssid[0] != '\0') {
            (void)wifi_module_set_credentials(app_settings.wifi_ssid, app_settings.wifi_password);
            s_actor.connect_after_scan = true;
        } else {
            s_actor.connect_after_scan = false;
        }

        err = wifi_actor_scan_networks();
        if(err != ESP_OK && s_actor.connect_after_scan) {
            s_actor.connect_after_scan = false;
            (void)wifi_module_connect();
        }
        return ESP_OK;
    }

    s_actor.connect_after_scan = false;
    (void)wifi_module_disconnect();
    err = wifi_module_stop();
    wifi_scan_cache_clear();
    wifi_settings_ssid_clear();
    (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_DONE, 0, 0);
    return err;
}

static esp_err_t wifi_actor_set_credentials(const char *ssid, const char *password)
{
    esp_err_t err = wifi_actor_update_credentials_setting(ssid, password);
    if(err != ESP_OK) {
        return err;
    }

    err = wifi_module_set_credentials(app_settings.wifi_ssid, app_settings.wifi_password);
    if(err != ESP_OK) {
        return err;
    }

    if(app_settings.wifi_enable) {
        (void)wifi_module_disconnect();
        (void)wifi_module_connect();
    }

    return ESP_OK;
}

static void wifi_actor_apply_msg(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_CMD) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_CMD_WIFI_START:
            (void)wifi_module_start();
            break;
        case MSG_EVT_CMD_WIFI_STOP:
			LOG("MSG_EVT_CMD_WIFI_STOP");
            (void)wifi_module_stop();
            break;
        case MSG_EVT_CMD_WIFI_CONNECT:
            (void)wifi_module_connect();
            break;
        case MSG_EVT_CMD_WIFI_DISCONNECT:
            (void)wifi_module_disconnect();
            break;
        case MSG_EVT_CMD_WIFI_SET_ENABLE:
            (void)wifi_actor_set_enable(msg->data.value != 0);
            break;
        case MSG_EVT_CMD_WIFI_SCAN:
            (void)wifi_actor_scan_networks();
            break;
        case MSG_EVT_CMD_WIFI_SET_CREDENTIALS:
            (void)wifi_actor_set_credentials(
                msg->data.wifi_credentials.ssid,
                msg->data.wifi_credentials.password
            );
            break;
        default:
            break;
    }
}

static void wifi_actor_poll_signal_quality(void)
{
    int rssi = 0;
    if(!wifi_module_is_connected()) {
        s_actor.weak_reported = false;
        s_actor.last_signal_level = 0;
        return;
    }

    if(wifi_module_get_rssi(&rssi) != ESP_OK) {
        return;
    }

    const int level = wifi_actor_calc_signal_level(rssi, s_actor.weak_rssi_threshold);
    if(level != s_actor.last_signal_level) {
        s_actor.last_signal_level = level;
        (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SIGNAL_LEVEL, level, 0);
    }

    if(level == 1) {
        if(!s_actor.weak_reported) {
            s_actor.weak_reported = true;
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SIGNAL_WEAK, rssi, 0);
        }
        return;
    }

    s_actor.weak_reported = false;
}

static void wifi_actor_task(void *arg)
{
    (void)arg;

    msg_t msg;

    while(1) {
        if(xQueueReceive(s_actor.inbox_q, &msg, WIFI_ACTOR_LOOP_TICKS) == pdTRUE) {
            wifi_actor_apply_msg(&msg);
        }

        wifi_actor_poll_signal_quality();
    }
}

static esp_err_t wifi_actor_subscribe(void)
{
    bool queue_created = false;

    if(s_actor.inbox_q == NULL) {
        esp_err_t err = msg_actor_queue_create_with_len(WIFI_ACTOR_INBOX_Q_LEN, &s_actor.inbox_q);
        if(err != ESP_OK) {
            return err;
        }
        queue_created = true;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_WIFI_CMD,
    };
    esp_err_t err = msg_sub(s_actor.inbox_q, topics, sizeof(topics) / sizeof(topics[0]), &s_actor.sub);
    if(err != ESP_OK && queue_created) {
        vQueueDelete(s_actor.inbox_q);
        s_actor.inbox_q = NULL;
    }

    return err;
}

esp_err_t wifi_actor_init(void)
{
    if(s_actor.task != NULL) {
        return ESP_OK;
    }

    wifi_module_config_t wifi_cfg = wifi_module_default_config();
    (void)strncpy(wifi_cfg.ssid, app_settings.wifi_ssid, sizeof(wifi_cfg.ssid) - 1);
    wifi_cfg.ssid[sizeof(wifi_cfg.ssid) - 1] = '\0';
    (void)strncpy(wifi_cfg.password, app_settings.wifi_password, sizeof(wifi_cfg.password) - 1);
    wifi_cfg.password[sizeof(wifi_cfg.password) - 1] = '\0';
    wifi_cfg.auto_reconnect = app_settings.wifi_auto_reconnect;

    esp_err_t err = wifi_module_init(&wifi_cfg);
    if(err != ESP_OK) {
        LOG("wifi_module_init failed: %s", esp_err_to_name(err));
        return err;
    }

    (void)wifi_module_register_event_callback(wifi_actor_wifi_event_cb, NULL);

    err = wifi_actor_subscribe();
    if(err != ESP_OK) {
        (void)wifi_module_deinit();
        return err;
    }

    s_actor.weak_reported = false;
    s_actor.connect_after_scan = false;
    s_actor.last_signal_level = 0;
    s_actor.weak_rssi_threshold = app_settings.wifi_weak_rssi_threshold;

    BaseType_t ok = xTaskCreatePinnedToCore(
        wifi_actor_task,
        "wifi_actor",
        4096,
        NULL,
        TASK_PRIO_CON,
        &s_actor.task,
        TASK_CORE_CON
    );

    if(ok != pdPASS) {
        (void)msg_unsub(s_actor.sub, NULL, 0);
        s_actor.sub = MSG_SUB_HANDLE_INVALID;
        vQueueDelete(s_actor.inbox_q);
        s_actor.inbox_q = NULL;
        (void)wifi_module_deinit();
        return ESP_FAIL;
    }

    (void)wifi_module_start();
    if(app_settings.wifi_enable && app_settings.wifi_ssid[0] != '\0') {
        (void)wifi_module_connect();
    }

    // LOG("wifi actor init done");
    return ESP_OK;
}

esp_err_t wifi_actor_deinit(void)
{
    if(s_actor.task != NULL) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }

    if(s_actor.sub != MSG_SUB_HANDLE_INVALID) {
        (void)msg_unsub(s_actor.sub, NULL, 0);
        s_actor.sub = MSG_SUB_HANDLE_INVALID;
    }

    if(s_actor.inbox_q != NULL) {
        vQueueDelete(s_actor.inbox_q);
        s_actor.inbox_q = NULL;
    }

    s_actor.weak_reported = false;
    s_actor.connect_after_scan = false;
    s_actor.last_signal_level = 0;
    return wifi_module_deinit();
}
