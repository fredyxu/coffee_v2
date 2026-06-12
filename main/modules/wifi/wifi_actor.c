#include "wifi_actor.h"

#include <stdbool.h>
#include <stdio.h>
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
#include "modules/wifi/wifi_profile.h"
#include "modules/wifi/wifi_scan_cache.h"
#include "modules/wifi/wifi_settings.h"
#include "modules/ws/ws_actor.h"

#define WIFI_ACTOR_INBOX_Q_LEN 8
#define WIFI_ACTOR_CONNECT_TIMEOUT_TICKS pdMS_TO_TICKS(15000)
#define WIFI_ACTOR_SIGNAL_POLL_TICKS pdMS_TO_TICKS(5000)

typedef enum {
    WIFI_ACTOR_CONN_IDLE = 0,
    WIFI_ACTOR_CONN_CONNECTING,
    WIFI_ACTOR_CONN_CONNECTED,
    WIFI_ACTOR_CONN_DISCONNECTING,
} wifi_actor_conn_state_t;

typedef struct {
    QueueHandle_t inbox_q;
    msg_sub_handle_t sub;
    TaskHandle_t task;
    wifi_actor_conn_state_t conn_state;
    bool connect_after_disconnect;
    TickType_t connect_deadline;
    TickType_t next_signal_poll_tick;
    int last_signal_level;
    int weak_rssi_threshold;
    bool has_pending_credentials;
    char pending_ssid[33];
    char pending_password[65];
    char active_ssid[33];
    char active_password[65];
} wifi_actor_ctx_t;

static wifi_actor_ctx_t s_actor = {0};

static const TickType_t WIFI_ACTOR_LOOP_TICKS = pdMS_TO_TICKS(500);

static bool wifi_actor_tick_reached(TickType_t now, TickType_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static int wifi_actor_calc_signal_level(int rssi, int weak_threshold)
{
    (void)weak_threshold;

    if(rssi <= -85) {
        return 1;
    }
    if(rssi <= -75) {
        return 2;
    }
    if(rssi <= -65) {
        return 3;
    }
    return 4;
}

static void wifi_actor_clear_pending_credentials(void)
{
	s_actor.has_pending_credentials = false;
	s_actor.pending_ssid[0] = '\0';
	s_actor.pending_password[0] = '\0';
}

static void wifi_actor_set_active_credentials(const char *ssid, const char *password)
{
	(void)snprintf(s_actor.active_ssid, sizeof(s_actor.active_ssid), "%s", ssid ? ssid : "");
	(void)snprintf(s_actor.active_password, sizeof(s_actor.active_password), "%s", password ? password : "");
}

static void wifi_actor_set_pending_credentials(const char *ssid, const char *password)
{
	s_actor.has_pending_credentials = true;
	(void)snprintf(s_actor.pending_ssid, sizeof(s_actor.pending_ssid), "%s", ssid ? ssid : "");
	(void)snprintf(s_actor.pending_password, sizeof(s_actor.pending_password), "%s", password ? password : "");
	wifi_actor_set_active_credentials(s_actor.pending_ssid, s_actor.pending_password);
}

static esp_err_t wifi_actor_persist_connected_credentials(const char *ssid, const char *password)
{
	if(ssid == NULL || password == NULL || ssid[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	esp_err_t err = ESP_OK;
	if(strcmp(app_settings.wifi_ssid, ssid) != 0) {
		err = app_settings_update(&(app_settings_update_t) {
			.id = APP_SETTING_ID_WIFI_SSID,
			.value.str = ssid,
		});
		if(err != ESP_OK) {
			return err;
		}
	}

	if(strcmp(app_settings.wifi_password, password) != 0) {
		err = app_settings_update(&(app_settings_update_t) {
			.id = APP_SETTING_ID_WIFI_PASSWORD,
			.value.str = password,
		});
	}

	return err;
}

static esp_err_t wifi_actor_request_connect(void)
{
    if(!app_settings.wifi_enable) {
        return ESP_ERR_INVALID_STATE;
    }

    if(s_actor.conn_state == WIFI_ACTOR_CONN_CONNECTED ||
       s_actor.conn_state == WIFI_ACTOR_CONN_CONNECTING) {
        return ESP_OK;
    }

    if(s_actor.conn_state == WIFI_ACTOR_CONN_DISCONNECTING) {
        s_actor.connect_after_disconnect = true;
        return ESP_OK;
    }

    esp_err_t err = wifi_module_connect();
    if(err == ESP_OK) {
        s_actor.conn_state = WIFI_ACTOR_CONN_CONNECTING;
        s_actor.connect_deadline = xTaskGetTickCount() + WIFI_ACTOR_CONNECT_TIMEOUT_TICKS;
        return ESP_OK;
    }

    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
    s_actor.connect_after_disconnect = false;
    s_actor.connect_deadline = 0;
    LOG("wifi connect request failed: %s", esp_err_to_name(err));
    return err;
}

static esp_err_t wifi_actor_request_disconnect(bool connect_after_disconnect)
{
    s_actor.connect_after_disconnect = connect_after_disconnect;
    s_actor.connect_deadline = 0;

    if(s_actor.conn_state == WIFI_ACTOR_CONN_IDLE) {
        return connect_after_disconnect ? wifi_actor_request_connect() : ESP_OK;
    }

    if(s_actor.conn_state == WIFI_ACTOR_CONN_DISCONNECTING) {
        return ESP_OK;
    }

    s_actor.conn_state = WIFI_ACTOR_CONN_DISCONNECTING;
    esp_err_t err = wifi_module_disconnect();
    if(err == ESP_OK) {
        return ESP_OK;
    }

    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
    LOG("wifi disconnect request failed: %s", esp_err_to_name(err));
    return connect_after_disconnect ? wifi_actor_request_connect() : err;
}

static void wifi_actor_handle_connect_failed(int reason, bool abort_driver)
{
	    LOG("wifi connect failed: reason=%d", reason);
	    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
	    s_actor.connect_after_disconnect = false;
	    s_actor.connect_deadline = 0;
	    s_actor.last_signal_level = 0;
	    wifi_actor_clear_pending_credentials();
	    if(abort_driver) {
	        (void)wifi_module_disconnect();
	    }
	}

static void wifi_actor_check_connect_timeout(void)
{
    if(s_actor.conn_state != WIFI_ACTOR_CONN_CONNECTING || s_actor.connect_deadline == 0) {
        return;
    }

    if(wifi_actor_tick_reached(xTaskGetTickCount(), s_actor.connect_deadline)) {
        wifi_actor_handle_connect_failed(ESP_ERR_TIMEOUT, true);
	    }
	}

static esp_err_t wifi_actor_connect_profile(const wifi_profile_t *profile)
{
	if(profile == NULL || !profile->valid || profile->ssid[0] == '\0') {
		return ESP_ERR_INVALID_ARG;
	}

	esp_err_t err = wifi_module_set_credentials(profile->ssid, profile->password);
	if(err != ESP_OK) {
		LOG("wifi profile credentials apply failed: ssid=%s err=%s", profile->ssid, esp_err_to_name(err));
		return err;
	}

	wifi_actor_set_active_credentials(profile->ssid, profile->password);
	return wifi_actor_request_connect();
}

static bool wifi_actor_can_auto_connect(void)
{
	return app_settings.wifi_enable &&
	       !s_actor.has_pending_credentials &&
	       s_actor.conn_state == WIFI_ACTOR_CONN_IDLE &&
	       !wifi_module_is_connected();
}

static void wifi_actor_try_auto_connect_from_scan(void)
{
	if(!wifi_actor_can_auto_connect()) {
		return;
	}

	wifi_profile_t profile = {0};
	if(!wifi_profile_select_best_from_scan(&profile)) {
		return;
	}

	// LOG("wifi auto connect profile: %s", profile.ssid);
	(void)wifi_actor_connect_profile(&profile);
}

static void wifi_actor_emit_sys_event(const wifi_module_event_t *event)
{
    if(event == NULL) {
        return;
    }

	// LOG("EVENT %d:", event->id);

	    switch(event->id) {
	        case WIFI_MOD_EVT_GOT_IP:
	            s_actor.conn_state = WIFI_ACTOR_CONN_CONNECTED;
	            s_actor.connect_after_disconnect = false;
	            s_actor.connect_deadline = 0;
	            s_actor.next_signal_poll_tick = 0;
	            if(s_actor.has_pending_credentials) {
	                (void)wifi_profile_save_success(s_actor.pending_ssid, s_actor.pending_password);
	                (void)wifi_actor_persist_connected_credentials(s_actor.pending_ssid, s_actor.pending_password);
	                wifi_actor_clear_pending_credentials();
	            } else if(s_actor.active_ssid[0] != '\0') {
	                (void)wifi_profile_save_success(s_actor.active_ssid, s_actor.active_password);
	                (void)wifi_actor_persist_connected_credentials(s_actor.active_ssid, s_actor.active_password);
	            }
	            wifi_settings_ssid_refresh_connected();
	            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_CONNECTED, 1, 0);
	            break;
        case WIFI_MOD_EVT_STA_DISCONNECTED:
            if(s_actor.conn_state == WIFI_ACTOR_CONN_CONNECTING) {
                wifi_actor_handle_connect_failed(event->reason, false);
            } else {
                s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
                s_actor.connect_deadline = 0;
                s_actor.last_signal_level = 0;
            }
            wifi_settings_ssid_refresh_connected();
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_DISCONNECTED, event->reason, 0);
            if(s_actor.connect_after_disconnect) {
                s_actor.connect_after_disconnect = false;
                (void)wifi_actor_request_connect();
            }
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
            wifi_settings_ssid_refresh_connected();

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
	            wifi_settings_ssid_refresh_connected();
	            wifi_actor_try_auto_connect_from_scan();
	            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_DONE, event->ap_count, 0);
	            break;
        case WIFI_MOD_EVT_SCAN_FAILED:
            wifi_settings_ssid_set_status("扫描失败");
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_FAILED, event->reason, 0);
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

static esp_err_t wifi_actor_start_auto_flow(void)
{
	esp_err_t err = wifi_module_start();
	if(err != ESP_OK) {
		LOG("wifi auto flow start failed: %s", esp_err_to_name(err));
		return err;
	}

	size_t profile_count = wifi_profile_count();
	// LOG("wifi auto flow: enable=%d profile_count=%u saved_ssid=%s",
	    // app_settings.wifi_enable ? 1 : 0,
	    // (unsigned)profile_count,
	    // app_settings.wifi_ssid[0] != '\0' ? app_settings.wifi_ssid : "(empty)");

	if(profile_count > 0) {
		// LOG("wifi auto flow: scan saved profiles");
		return wifi_actor_scan_networks();
	}

	if(app_settings.wifi_ssid[0] != '\0') {
		LOG("wifi auto flow: connect saved ssid=%s", app_settings.wifi_ssid);
		wifi_actor_set_active_credentials(app_settings.wifi_ssid, app_settings.wifi_password);
		return wifi_actor_request_connect();
	}

	// LOG("wifi auto flow: no saved profile, scan only");
	return wifi_actor_scan_networks();
}

static esp_err_t wifi_actor_set_enable(bool enable)
{
    if(app_settings.wifi_enable == enable) {
        return ESP_OK;
    }

    esp_err_t err = wifi_actor_update_enable_setting(enable);
    if(err != ESP_OK) {
        return err;
    }

	    if(enable) {
	        return wifi_actor_start_auto_flow();
	    }

	    s_actor.connect_after_disconnect = false;
	    s_actor.connect_deadline = 0;
	    wifi_actor_clear_pending_credentials();
	    (void)ws_actor_prepare_wifi_stop(pdMS_TO_TICKS(8000));
	    (void)wifi_actor_request_disconnect(false);
	    err = wifi_module_stop();
	    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
    wifi_scan_cache_clear();
    wifi_settings_ssid_clear();
    (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SCAN_DONE, 0, 0);
    return err;
}

static esp_err_t wifi_actor_set_credentials(const char *ssid, const char *password)
{
	    if(!app_settings.wifi_enable) {
	        wifi_actor_clear_pending_credentials();
	        return ESP_ERR_INVALID_STATE;
	    }

	    esp_err_t err = wifi_module_set_credentials(ssid, password);
	    if(err != ESP_OK) {
	        return err;
	    }

	    wifi_actor_set_pending_credentials(ssid, password);
	    return wifi_actor_request_disconnect(true);
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
            s_actor.connect_after_disconnect = false;
            s_actor.connect_deadline = 0;
            (void)wifi_actor_request_disconnect(false);
            (void)wifi_module_stop();
            s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
            break;
        case MSG_EVT_CMD_WIFI_CONNECT:
            (void)wifi_actor_request_connect();
            break;
	        case MSG_EVT_CMD_WIFI_DISCONNECT:
	            wifi_actor_clear_pending_credentials();
	            (void)wifi_actor_request_disconnect(false);
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
    if(s_actor.conn_state != WIFI_ACTOR_CONN_CONNECTED || !wifi_module_is_connected()) {
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
}

static void wifi_actor_task(void *arg)
{
    (void)arg;

    msg_t msg;

    while(1) {
        if(xQueueReceive(s_actor.inbox_q, &msg, WIFI_ACTOR_LOOP_TICKS) == pdTRUE) {
            wifi_actor_apply_msg(&msg);
        }

        wifi_actor_check_connect_timeout();

        TickType_t now = xTaskGetTickCount();
        if(s_actor.next_signal_poll_tick == 0 ||
           wifi_actor_tick_reached(now, s_actor.next_signal_poll_tick)) {
            s_actor.next_signal_poll_tick = now + WIFI_ACTOR_SIGNAL_POLL_TICKS;
            wifi_actor_poll_signal_quality();
        }
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

	    err = wifi_profile_init();
	    if(err != ESP_OK) {
	        LOG("wifi_profile_init failed: %s", esp_err_to_name(err));
	    }

    (void)wifi_module_register_event_callback(wifi_actor_wifi_event_cb, NULL);

    err = wifi_actor_subscribe();
    if(err != ESP_OK) {
        (void)wifi_module_deinit();
        return err;
    }

    s_actor.last_signal_level = 0;
    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
    s_actor.connect_after_disconnect = false;
    s_actor.connect_deadline = 0;
    s_actor.next_signal_poll_tick = 0;
    s_actor.weak_rssi_threshold = app_settings.wifi_weak_rssi_threshold;
    wifi_actor_set_active_credentials(app_settings.wifi_ssid, app_settings.wifi_password);

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

	    if(app_settings.wifi_enable) {
	        (void)wifi_actor_start_auto_flow();
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

    s_actor.last_signal_level = 0;
    s_actor.conn_state = WIFI_ACTOR_CONN_IDLE;
    s_actor.connect_after_disconnect = false;
    s_actor.connect_deadline = 0;
    s_actor.next_signal_poll_tick = 0;
    return wifi_module_deinit();
}
