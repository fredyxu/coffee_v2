#include "wifi_actor.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"

#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"
#include "modules/store/store_actor.h"
#include "modules/wifi/wifi.h"

typedef enum {
    WIFI_ACTOR_CMD_START,
    WIFI_ACTOR_CMD_STOP,
    WIFI_ACTOR_CMD_CONNECT,
    WIFI_ACTOR_CMD_DISCONNECT,
    WIFI_ACTOR_CMD_SET_CREDENTIALS,
} wifi_actor_cmd_type_t;

typedef struct {
    wifi_actor_cmd_type_t type;
    char ssid[33];
    char password[65];
} wifi_actor_cmd_t;

typedef struct {
    QueueHandle_t cmd_q;
    TaskHandle_t task;
    bool weak_reported;
    int weak_rssi_threshold;
} wifi_actor_ctx_t;

static wifi_actor_ctx_t s_actor = {0};

static const TickType_t WIFI_ACTOR_LOOP_TICKS = pdMS_TO_TICKS(500);

static void wifi_actor_emit_sys_event(const wifi_module_event_t *event)
{
    if(event == NULL) {
        return;
    }

    switch(event->id) {
        case WIFI_MOD_EVT_GOT_IP:
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_CONNECTED, 1, 0);
            break;
        case WIFI_MOD_EVT_STA_DISCONNECTED:
            (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_DISCONNECTED, event->reason, 0);
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

wifi_actor_config_t wifi_actor_default_config(void)
{
    wifi_actor_config_t cfg = {
        .ssid = WIFI_STA_SSID,
        .password = WIFI_STA_PASSWORD,
        .auto_reconnect = (WIFI_AUTO_RECONNECT != 0),
        .weak_rssi_threshold = WIFI_WEAK_RSSI_THRESHOLD,
    };
    return cfg;
}

static esp_err_t wifi_actor_post_cmd(const wifi_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void wifi_actor_apply_cmd(const wifi_actor_cmd_t *cmd)
{
    if(cmd == NULL) {
        return;
    }

    switch(cmd->type) {
        case WIFI_ACTOR_CMD_START:
            (void)wifi_module_start();
            break;
        case WIFI_ACTOR_CMD_STOP:
            (void)wifi_module_stop();
            break;
        case WIFI_ACTOR_CMD_CONNECT:
            (void)wifi_module_connect();
            break;
        case WIFI_ACTOR_CMD_DISCONNECT:
            (void)wifi_module_disconnect();
            break;
        case WIFI_ACTOR_CMD_SET_CREDENTIALS:
            (void)wifi_module_set_credentials(cmd->ssid, cmd->password);
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
        return;
    }

    if(wifi_module_get_rssi(&rssi) != ESP_OK) {
        return;
    }

    if(rssi <= s_actor.weak_rssi_threshold) {
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

    wifi_actor_cmd_t cmd;

    while(1) {
        if(xQueueReceive(s_actor.cmd_q, &cmd, WIFI_ACTOR_LOOP_TICKS) == pdTRUE) {
            wifi_actor_apply_cmd(&cmd);
        }

        wifi_actor_poll_signal_quality();
    }
}

esp_err_t wifi_actor_init(void)
{
    if(s_actor.task != NULL) {
        return ESP_OK;
    }

    wifi_actor_config_t cfg = wifi_actor_default_config();
    store_wifi_settings_t persisted = store_default_wifi_settings();
    esp_err_t persisted_err = store_actor_load_wifi(&persisted);
    if(persisted_err == ESP_OK) {
        (void)strncpy(cfg.ssid, persisted.ssid, sizeof(cfg.ssid) - 1);
        cfg.ssid[sizeof(cfg.ssid) - 1] = '\0';
        (void)strncpy(cfg.password, persisted.password, sizeof(cfg.password) - 1);
        cfg.password[sizeof(cfg.password) - 1] = '\0';
        cfg.auto_reconnect = persisted.auto_reconnect;
        cfg.weak_rssi_threshold = persisted.weak_rssi_threshold;
    } else if(persisted_err != ESP_ERR_INVALID_STATE) {
        LOG("store load wifi failed: %s", esp_err_to_name(persisted_err));
    }

    wifi_module_config_t wifi_cfg = wifi_module_default_config();
    (void)strncpy(wifi_cfg.ssid, cfg.ssid, sizeof(wifi_cfg.ssid) - 1);
    wifi_cfg.ssid[sizeof(wifi_cfg.ssid) - 1] = '\0';
    (void)strncpy(wifi_cfg.password, cfg.password, sizeof(wifi_cfg.password) - 1);
    wifi_cfg.password[sizeof(wifi_cfg.password) - 1] = '\0';
    wifi_cfg.auto_reconnect = cfg.auto_reconnect;

    esp_err_t err = wifi_module_init(&wifi_cfg);
    if(err != ESP_OK) {
        LOG("wifi_module_init failed: %s", esp_err_to_name(err));
        return err;
    }

    (void)wifi_module_register_event_callback(wifi_actor_wifi_event_cb, NULL);

    s_actor.cmd_q = xQueueCreate(8, sizeof(wifi_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        (void)wifi_module_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_actor.weak_reported = false;
    s_actor.weak_rssi_threshold = cfg.weak_rssi_threshold;

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
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        (void)wifi_module_deinit();
        return ESP_FAIL;
    }

    (void)wifi_actor_start(0);
    if(cfg.ssid[0] != '\0') {
        (void)wifi_actor_connect(0);
    }

    LOG("wifi actor init done");
    return ESP_OK;
}

esp_err_t wifi_actor_deinit(void)
{
    if(s_actor.task != NULL) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }

    if(s_actor.cmd_q != NULL) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
    }

    s_actor.weak_reported = false;
    return wifi_module_deinit();
}

esp_err_t wifi_actor_start(TickType_t timeout_ticks)
{
    wifi_actor_cmd_t cmd = {
        .type = WIFI_ACTOR_CMD_START,
    };
    return wifi_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t wifi_actor_stop(TickType_t timeout_ticks)
{
    wifi_actor_cmd_t cmd = {
        .type = WIFI_ACTOR_CMD_STOP,
    };
    return wifi_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t wifi_actor_connect(TickType_t timeout_ticks)
{
    wifi_actor_cmd_t cmd = {
        .type = WIFI_ACTOR_CMD_CONNECT,
    };
    return wifi_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t wifi_actor_disconnect(TickType_t timeout_ticks)
{
    wifi_actor_cmd_t cmd = {
        .type = WIFI_ACTOR_CMD_DISCONNECT,
    };
    return wifi_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t wifi_actor_set_credentials(const char *ssid, const char *password, TickType_t timeout_ticks)
{
    if(ssid == NULL || password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(strlen(ssid) >= sizeof(((wifi_actor_cmd_t *)0)->ssid) || strlen(password) >= sizeof(((wifi_actor_cmd_t *)0)->password)) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_actor_cmd_t cmd = {
        .type = WIFI_ACTOR_CMD_SET_CREDENTIALS,
    };

    (void)strncpy(cmd.ssid, ssid, sizeof(cmd.ssid) - 1);
    cmd.ssid[sizeof(cmd.ssid) - 1] = '\0';
    (void)strncpy(cmd.password, password, sizeof(cmd.password) - 1);
    cmd.password[sizeof(cmd.password) - 1] = '\0';

    esp_err_t err = wifi_actor_post_cmd(&cmd, timeout_ticks);
    if(err != ESP_OK) {
        return err;
    }

    store_wifi_settings_t save = {
        .ssid = "",
        .password = "",
        .auto_reconnect = WIFI_AUTO_RECONNECT != 0,
        .weak_rssi_threshold = WIFI_WEAK_RSSI_THRESHOLD,
    };
    (void)strncpy(save.ssid, cmd.ssid, sizeof(save.ssid) - 1);
    save.ssid[sizeof(save.ssid) - 1] = '\0';
    (void)strncpy(save.password, cmd.password, sizeof(save.password) - 1);
    save.password[sizeof(save.password) - 1] = '\0';
    (void)store_actor_save_wifi(&save, 0);

    return ESP_OK;
}
