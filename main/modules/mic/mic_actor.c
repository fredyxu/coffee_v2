#include "mic_actor.h"

#include <stdbool.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/mic/mic.h"

typedef enum {
    MIC_ACTOR_CMD_START,
    MIC_ACTOR_CMD_STOP,
    MIC_ACTOR_CMD_SET_SR,
} mic_actor_cmd_type_t;

typedef struct {
    mic_actor_cmd_type_t type;
    uint32_t sample_rate;
} mic_actor_cmd_t;

typedef struct {
    QueueHandle_t cmd_q;
    QueueHandle_t frame_q;
    TaskHandle_t task;
    bool capturing;
} mic_actor_ctx_t;

static mic_actor_ctx_t s_actor = {0};
static const TickType_t MIC_ACTOR_POLL_TICKS = pdMS_TO_TICKS(5);
static const TickType_t MIC_ACTOR_ERROR_BACKOFF_TICKS = pdMS_TO_TICKS(10);

static esp_err_t mic_actor_post_cmd(const mic_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void mic_actor_task(void *arg)
{
    (void)arg;

    mic_actor_cmd_t cmd;
    mic_frame_t frame;
    size_t frame_fill = 0;

    while(1) {
        if(s_actor.capturing) {
            // 先以超时方式等待控制命令，避免采集循环高频抢占 CPU0 导致 IDLE0 饥饿。
            if(xQueueReceive(s_actor.cmd_q, &cmd, MIC_ACTOR_POLL_TICKS) == pdTRUE) {
                if(cmd.type == MIC_ACTOR_CMD_STOP) {
                    s_actor.capturing = false;
                    frame_fill = 0;
                } else if(cmd.type == MIC_ACTOR_CMD_SET_SR && cmd.sample_rate > 0) {
                    (void)mic_set_sample_rate(cmd.sample_rate);
                } else if(cmd.type == MIC_ACTOR_CMD_START) {
                    s_actor.capturing = true;
                }
                continue;
            }

            if(frame_fill == 0) {
                frame.sample_rate = mic_get_sample_rate();
                frame.sample_count = MIC_ACTOR_FRAME_MAX_SAMPLES;
                frame.timestamp = (uint32_t)xTaskGetTickCount();
            }

            size_t got = 0;
            esp_err_t err = mic_read_pcm16_some(
                &frame.samples[frame_fill],
                MIC_ACTOR_FRAME_MAX_SAMPLES - frame_fill,
                &got
            );
            if(err == ESP_OK) {
                frame_fill += got;
                if(frame_fill >= MIC_ACTOR_FRAME_MAX_SAMPLES) {
                    if(xQueueSend(s_actor.frame_q, &frame, 0) != pdTRUE) {
                        mic_frame_t stale;
                        (void)xQueueReceive(s_actor.frame_q, &stale, 0);
                        (void)xQueueSend(s_actor.frame_q, &frame, 0);
                    }
                    frame_fill = 0;
                }
            } else {
                LOG("mic capture err: %s", esp_err_to_name(err));
                frame_fill = 0;
                vTaskDelay(MIC_ACTOR_ERROR_BACKOFF_TICKS);
            }
        } else {
            if(xQueueReceive(s_actor.cmd_q, &cmd, portMAX_DELAY) != pdTRUE) {
                continue;
            }

            if(cmd.type == MIC_ACTOR_CMD_START) {
                s_actor.capturing = true;
                frame_fill = 0;
            } else if(cmd.type == MIC_ACTOR_CMD_SET_SR && cmd.sample_rate > 0) {
                (void)mic_set_sample_rate(cmd.sample_rate);
            }
        }
    }
}

esp_err_t mic_actor_init(void)
{
    if(s_actor.task != NULL) {
        return ESP_OK;
    }

    mic_config_t cfg = mic_default_config();
    esp_err_t err = mic_init(&cfg);
    if(err != ESP_OK) {
        LOG("mic_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_actor.cmd_q = xQueueCreate(MIC_ACTOR_CMD_QUEUE_LEN, sizeof(mic_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_actor.frame_q = xQueueCreate(MIC_ACTOR_FRAME_QUEUE_LEN, sizeof(mic_frame_t));
    if(s_actor.frame_q == NULL) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        mic_actor_task,
        "mic_actor",
        MIC_ACTOR_TASK_STACK,
        NULL,
        TASK_PRIO_MIC,
        &s_actor.task,
        TASK_CORE_MIC
    );
    if(ok != pdPASS) {
        vQueueDelete(s_actor.frame_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.frame_q = NULL;
        s_actor.cmd_q = NULL;
        return ESP_FAIL;
    }

    // LOG("mic actor init done");
    return ESP_OK;
}

esp_err_t mic_actor_deinit(void)
{
    if(s_actor.task) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }

    if(s_actor.frame_q) {
        vQueueDelete(s_actor.frame_q);
        s_actor.frame_q = NULL;
    }

    if(s_actor.cmd_q) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
    }

    s_actor.capturing = false;
    return mic_deinit();
}

esp_err_t mic_actor_start(TickType_t timeout_ticks)
{
    mic_actor_cmd_t cmd = {
        .type = MIC_ACTOR_CMD_START,
        .sample_rate = 0,
    };
    return mic_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t mic_actor_stop(TickType_t timeout_ticks)
{
    mic_actor_cmd_t cmd = {
        .type = MIC_ACTOR_CMD_STOP,
        .sample_rate = 0,
    };
    return mic_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t mic_actor_set_sample_rate(uint32_t sample_rate, TickType_t timeout_ticks)
{
    if(sample_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    mic_actor_cmd_t cmd = {
        .type = MIC_ACTOR_CMD_SET_SR,
        .sample_rate = sample_rate,
    };
    return mic_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t mic_actor_pop_frame(mic_frame_t *out_frame, TickType_t timeout_ticks)
{
    if(s_actor.frame_q == NULL || out_frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return (xQueueReceive(s_actor.frame_q, out_frame, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}
