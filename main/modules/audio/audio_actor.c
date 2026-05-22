#include "audio_actor.h"

#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/utils/log.h"
#include "modules/audio/audio.h"

typedef enum {
    AUDIO_ACTOR_CMD_TONE,
    AUDIO_ACTOR_CMD_FILE,
    AUDIO_ACTOR_CMD_STOP,
    AUDIO_ACTOR_CMD_STREAM_START,
    AUDIO_ACTOR_CMD_STREAM_STOP,
} audio_actor_cmd_type_t;

typedef struct {
    audio_actor_cmd_type_t type;
    union {
        struct {
            uint32_t freq_hz;
            uint32_t duration_ms;
        } tone;
        struct {
            char path[192];
        } file;
        struct {
            uint32_t sample_rate;
        } stream;
    } data;
} audio_actor_cmd_t;

typedef struct {
    QueueHandle_t cmd_q;
    QueueHandle_t stream_q;
    TaskHandle_t task;
    bool stream_mode;
    uint32_t stream_sample_rate;
} audio_actor_ctx_t;

static audio_actor_ctx_t s_actor = {0};

static esp_err_t audio_actor_post_cmd(const audio_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void audio_actor_task(void *arg)
{
    (void)arg;

    audio_actor_cmd_t cmd;
    audio_stream_chunk_t chunk;

    while(1) {
        if(s_actor.stream_mode) {
            if(xQueueReceive(s_actor.stream_q, &chunk, pdMS_TO_TICKS(20)) == pdTRUE) {
                if(chunk.sample_rate != 0 && chunk.sample_rate != audio_get_sample_rate()) {
                    (void)audio_set_sample_rate(chunk.sample_rate);
                }
                if(chunk.sample_count > 0) {
                    (void)audio_play_stream_chunk(chunk.samples, chunk.sample_count, pdMS_TO_TICKS(40));
                }
            }

            if(xQueueReceive(s_actor.cmd_q, &cmd, 0) != pdTRUE) {
                continue;
            }
        } else {
            if(xQueueReceive(s_actor.cmd_q, &cmd, pdMS_TO_TICKS(20)) != pdTRUE) {
                continue;
            }
        }

        switch(cmd.type) {
            case AUDIO_ACTOR_CMD_TONE:
                (void)audio_play_tone(cmd.data.tone.freq_hz, cmd.data.tone.duration_ms);
                break;
            case AUDIO_ACTOR_CMD_FILE:
                (void)audio_play_file(cmd.data.file.path);
                break;
            case AUDIO_ACTOR_CMD_STOP:
                (void)audio_stop();
                if(s_actor.stream_q) {
                    (void)xQueueReset(s_actor.stream_q);
                }
                break;
            case AUDIO_ACTOR_CMD_STREAM_START:
                s_actor.stream_mode = true;
                s_actor.stream_sample_rate = cmd.data.stream.sample_rate;
                if(s_actor.stream_sample_rate != 0) {
                    (void)audio_set_sample_rate(s_actor.stream_sample_rate);
                }
                if(s_actor.stream_q) {
                    (void)xQueueReset(s_actor.stream_q);
                }
                break;
            case AUDIO_ACTOR_CMD_STREAM_STOP:
                s_actor.stream_mode = false;
                if(s_actor.stream_q) {
                    (void)xQueueReset(s_actor.stream_q);
                }
                break;
            default:
                break;
        }
    }
}

esp_err_t audio_actor_init(void)
{
    if(s_actor.task != NULL) {
        return ESP_OK;
    }

    audio_config_t cfg = audio_default_config();
    esp_err_t err = audio_init(&cfg);
    if(err != ESP_OK) {
        LOG("audio_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_actor.cmd_q = xQueueCreate(16, sizeof(audio_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_actor.stream_q = xQueueCreate(24, sizeof(audio_stream_chunk_t));
    if(s_actor.stream_q == NULL) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        audio_actor_task,
        "audio_actor",
        6144,
        NULL,
        TASK_PRIO_AUDIO,
        &s_actor.task,
        TASK_CORE_AUDIO
    );
    if(ok != pdPASS) {
        vQueueDelete(s_actor.stream_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.stream_q = NULL;
        s_actor.cmd_q = NULL;
        return ESP_FAIL;
    }

    // LOG("audio actor init done (bclk=%d ws=%d dout=%d)", PIN_SOUND_I2S_BCLK, PIN_SOUND_I2S_WS, PIN_SOUND_I2S_DOUT);
    return ESP_OK;
}

esp_err_t audio_actor_deinit(void)
{
    if(s_actor.task) {
        vTaskDelete(s_actor.task);
        s_actor.task = NULL;
    }
    if(s_actor.stream_q) {
        vQueueDelete(s_actor.stream_q);
        s_actor.stream_q = NULL;
    }
    if(s_actor.cmd_q) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
    }

    s_actor.stream_mode = false;
    s_actor.stream_sample_rate = 0;
    return audio_deinit();
}

esp_err_t audio_actor_play_tone(uint32_t freq_hz, uint32_t duration_ms, TickType_t timeout_ticks)
{
    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_TONE,
        .data.tone = {
            .freq_hz = freq_hz,
            .duration_ms = duration_ms,
        },
    };
    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_play_file(const char *path, TickType_t timeout_ticks)
{
    if(path == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_FILE,
    };
    strncpy(cmd.data.file.path, path, sizeof(cmd.data.file.path) - 1);
    cmd.data.file.path[sizeof(cmd.data.file.path) - 1] = '\0';

    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_stop(TickType_t timeout_ticks)
{
    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_STOP,
    };
    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_stream_start(uint32_t sample_rate, TickType_t timeout_ticks)
{
    if(sample_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_STREAM_START,
        .data.stream = {
            .sample_rate = sample_rate,
        },
    };
    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_stream_stop(TickType_t timeout_ticks)
{
    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_STREAM_STOP,
    };
    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_stream_push(const int16_t *samples, size_t sample_count, uint32_t sample_rate, TickType_t timeout_ticks)
{
    if(s_actor.stream_q == NULL || samples == NULL || sample_count == 0 || sample_count > AUDIO_STREAM_CHUNK_MAX_SAMPLES) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_stream_chunk_t chunk = {
        .sample_rate = sample_rate,
        .sample_count = sample_count,
    };
    memcpy(chunk.samples, samples, sample_count * sizeof(int16_t));

    return (xQueueSend(s_actor.stream_q, &chunk, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}
