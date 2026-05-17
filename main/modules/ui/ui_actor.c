#include "modules/ui/ui_actor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"

#define UI_ACTOR_INPUT_Q_LEN 16

typedef struct {
    TaskHandle_t task;
    QueueHandle_t input_q;
    msg_sub_handle_t encoder_sub;
} ui_actor_ctx_t;

static ui_actor_ctx_t s_ui_actor = {0};

static void ui_actor_handle_encoder_input(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_INPUT) {
        return;
    }

    switch(msg->event) {
        case EVENT_ENCODER_CW:
            LOG("ui_actor encoder cw");
            break;
        case EVENT_ENCODER_CCW:
            LOG("ui_actor encoder ccw");
            break;
        case EVENT_ENCODER_PRESS:
            LOG("ui_actor encoder press");
            break;
        default:
            break;
    }
}

static void ui_actor_task(void *arg)
{
    (void)arg;

    msg_t msg;
    while(1) {
        if(xQueueReceive(s_ui_actor.input_q, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ui_actor_handle_encoder_input(&msg);
    }
}

static esp_err_t ui_actor_subscribe_encoder(void)
{
    if(s_ui_actor.input_q == NULL) {
        s_ui_actor.input_q = xQueueCreate(UI_ACTOR_INPUT_Q_LEN, sizeof(msg_t));
        if(s_ui_actor.input_q == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    return msg_sub_queue(MSG_TOPIC_ENCODER_INPUT, s_ui_actor.input_q, &s_ui_actor.encoder_sub);
}

esp_err_t ui_actor_init(void)
{
    if(s_ui_actor.task != NULL) {
        return ESP_OK;
    }

    esp_err_t err = ui_actor_subscribe_encoder();
    if(err != ESP_OK) {
        LOG("ui_actor subscribe encoder failed: %s", esp_err_to_name(err));
        return err;
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
