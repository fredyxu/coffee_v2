#include "modules/ui/ui_actor.h"

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config/config_sys.h"
#include "core/con/con.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"
#include "modules/ui/adapter/lvgl_port.h"
#include "modules/ui/page/page_init/page_init.h"

typedef struct {
    TaskHandle_t task;
} ui_actor_ctx_t;

typedef struct {
    char text[64];
} ui_text_payload_t;

static ui_actor_ctx_t s_ui_actor = {0};

static void ui_actor_append_init_text_cb(void *arg)
{
    ui_text_payload_t *payload = (ui_text_payload_t *)arg;
    if(payload == NULL) {
        return;
    }

    esp_err_t err = add_init_msg(payload->text);
    if(err != ESP_OK) {
        LOG("ui_actor add_init_msg failed: %s", esp_err_to_name(err));
    }
}

static void ui_actor_handle_cmd(const msg_t *cmd)
{
    if(cmd == NULL || cmd->type != MSG_TYPE_CMD) {
        return;
    }

    switch(cmd->event) {
        case CMD_UI_UPDATE_TEXT: {
            ui_text_payload_t payload = {0};
            (void)snprintf(payload.text, sizeof(payload.text), "%s", cmd->data.text.text);
            lvgl_port_run(ui_actor_append_init_text_cb, &payload);
            break;
        }
        case CMD_UI_NAV_STEP:
            LOG("ui_actor nav step=%d (handler not implemented yet)", cmd->data.value);
            break;
        case CMD_UI_SCROLL:
            LOG("ui_actor scroll=%d (handler not implemented yet)", cmd->data.value);
            break;
        default:
            LOG("ui_actor unsupported cmd event=%d", (int)cmd->event);
            break;
    }
}

static void ui_actor_task(void *arg)
{
    (void)arg;

    msg_t ui_cmd;
    while(1) {
        if(con_recv_ui_cmd(&ui_cmd, portMAX_DELAY) != ESP_OK) {
            continue;
        }
        ui_actor_handle_cmd(&ui_cmd);
    }
}

esp_err_t ui_actor_init(void)
{
    if(s_ui_actor.task != NULL) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        ui_actor_task,
        "ui_actor",
        4096,
        NULL,
        TASK_PRIO_LVGL,
        &s_ui_actor.task,
        TASK_CORE_LVGL
    );
    if(ok != pdPASS) {
        s_ui_actor.task = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}


