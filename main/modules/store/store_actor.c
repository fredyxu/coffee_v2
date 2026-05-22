#include "store_actor.h"

#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"

#include "config/config_sys.h"
#include "core/utils/log.h"

typedef enum {
    STORE_ACTOR_CMD_SAVE_WIFI,
} store_actor_cmd_type_t;

typedef struct {
    store_actor_cmd_type_t type;
    store_wifi_settings_t wifi;
} store_actor_cmd_t;

typedef struct {
    QueueHandle_t cmd_q;
    TaskHandle_t task;
} store_actor_ctx_t;

static store_actor_ctx_t s_actor = {0};

static esp_err_t store_actor_post_cmd(const store_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void store_actor_task(void *arg)
{
    (void)arg;

    store_actor_cmd_t cmd;
    while(1) {
        if(xQueueReceive(s_actor.cmd_q, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch(cmd.type) {
            case STORE_ACTOR_CMD_SAVE_WIFI: {
                esp_err_t err = store_save_wifi_settings(&cmd.wifi);
                if(err != ESP_OK) {
                    LOG("store save wifi failed: %s", esp_err_to_name(err));
                }
                break;
            }
            default:
                break;
        }
    }
}

esp_err_t store_actor_init(void)
{
    if(s_actor.task != NULL) {
        return ESP_OK;
    }

    esp_err_t err = store_init();
    if(err != ESP_OK) {
        return err;
    }

    s_actor.cmd_q = xQueueCreate(8, sizeof(store_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        store_actor_task,
        "store_actor",
        4096,
        NULL,
        TASK_PRIO_CON,
        &s_actor.task,
        TASK_CORE_CON
    );
    if(ok != pdPASS) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        return ESP_FAIL;
    }

    // LOG("store actor init done");
    return ESP_OK;
}

esp_err_t store_actor_deinit(void)
{
    if(s_actor.task != NULL) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }

    if(s_actor.cmd_q != NULL) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
    }

    return store_deinit();
}

esp_err_t store_actor_load_wifi(store_wifi_settings_t *out_settings)
{
    if(out_settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(s_actor.task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return store_load_wifi_settings(out_settings);
}

esp_err_t store_actor_save_wifi(const store_wifi_settings_t *settings, TickType_t timeout_ticks)
{
    if(settings == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    store_actor_cmd_t cmd = {
        .type = STORE_ACTOR_CMD_SAVE_WIFI,
    };
    cmd.wifi = *settings;

    return store_actor_post_cmd(&cmd, timeout_ticks);
}

