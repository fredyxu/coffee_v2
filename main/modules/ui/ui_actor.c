#include "modules/ui/ui_actor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "modules/ui/adapter/lvgl_port.h"

static const ui_page_ops_t *s_page_ops = NULL;


#define UI_ACTOR_INBOX_Q_LEN 16

typedef struct {
    TaskHandle_t task;
    QueueHandle_t inbox_q;
    msg_sub_handle_t sub;
} ui_actor_ctx_t;

static ui_actor_ctx_t s_ui_actor = {0};

typedef struct {
	const ui_page_ops_t *ops;
	const msg_t *msg;
} ui_actor_input_ctx_t;

static void ui_actor_dispatch_input(void *arg)
{
	ui_actor_input_ctx_t *ctx = (ui_actor_input_ctx_t *)arg;
	if(ctx == NULL || ctx->ops == NULL || ctx->msg == NULL) {
		return;
	}

	if(ctx->msg->type == MSG_TYPE_INPUT && ctx->ops->on_input != NULL) {
		ctx->ops->on_input(ctx->msg);
		return;
	}

	if(ctx->ops->on_msg != NULL) {
		ctx->ops->on_msg(ctx->msg);
	}
}

static void ui_actor_handle_msg(const msg_t *msg)
{
    if(msg == NULL) {
        return;
    }

	const ui_page_ops_t *ops = s_page_ops;
	if(ops != NULL && (ops->on_input != NULL || ops->on_msg != NULL)) {
		ui_actor_input_ctx_t ctx = {
			.ops = ops,
			.msg = msg,
		};
		lvgl_port_run(ui_actor_dispatch_input, &ctx);
	}
}

static void ui_actor_task(void *arg)
{
    (void)arg;

    msg_t msg;
    while(1) {
        if(xQueueReceive(s_ui_actor.inbox_q, &msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ui_actor_handle_msg(&msg);
    }
}

static esp_err_t ui_actor_subscribe(void)
{
    bool queue_created = false;

    if(s_ui_actor.inbox_q == NULL) {
        esp_err_t err = msg_actor_queue_create_with_len(UI_ACTOR_INBOX_Q_LEN, &s_ui_actor.inbox_q);
        if(err != ESP_OK) {
            return err;
        }
        queue_created = true;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_ENCODER_INPUT,
        MSG_TOPIC_WIFI_EVENT,
    };
    esp_err_t err = msg_sub(s_ui_actor.inbox_q, topics, sizeof(topics) / sizeof(topics[0]), &s_ui_actor.sub);
    if(err != ESP_OK && queue_created) {
        vQueueDelete(s_ui_actor.inbox_q);
        s_ui_actor.inbox_q = NULL;
    }

    return err;
}

esp_err_t ui_actor_init(void)
{
    if(s_ui_actor.task != NULL) {
        return ESP_OK;
    }

    esp_err_t err = ui_actor_subscribe();
    if(err != ESP_OK) {
        LOG("ui_actor subscribe failed: %s", esp_err_to_name(err));
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
        (void)msg_unsub(s_ui_actor.sub, NULL, 0);
        s_ui_actor.sub = MSG_SUB_HANDLE_INVALID;
        if(s_ui_actor.inbox_q != NULL) {
            vQueueDelete(s_ui_actor.inbox_q);
            s_ui_actor.inbox_q = NULL;
        }
        return ESP_FAIL;
    }

    return ESP_OK;
}


void ui_actor_set_ops(const ui_page_ops_t *ops) {
	s_page_ops = ops;
}
void ui_actor_clean_ops() {
	s_page_ops = NULL;
}
