#include "audio_actor.h"

#include <math.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "app/app_settings.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/msg/msg.h"
#include "core/msg/msg_sub.h"
#include "core/utils/log.h"
#include "modules/audio/audio.h"

#define AUDIO_ACTOR_TONE_CHUNK_MS 10
#define AUDIO_ACTOR_TONE_MAX_SAMPLES 512
#define AUDIO_ACTOR_TONE_TABLE_SIZE 256
#define AUDIO_ACTOR_TONE_PHASE_BITS 32
#define AUDIO_ACTOR_TONE_ATTACK_MS 8
#define AUDIO_ACTOR_TONE_RELEASE_MS 8
#define AUDIO_ACTOR_PI 3.14159265358979323846f

typedef enum {
    AUDIO_ACTOR_CMD_TONE,
    AUDIO_ACTOR_CMD_TONE_ON,
    AUDIO_ACTOR_CMD_TONE_OFF,
    AUDIO_ACTOR_CMD_FILE,
    AUDIO_ACTOR_CMD_STOP,
    AUDIO_ACTOR_CMD_STREAM_START,
    AUDIO_ACTOR_CMD_STREAM_STOP,
} audio_actor_cmd_type_t;

typedef enum {
    AUDIO_ACTOR_TONE_ENV_OFF = 0,
    AUDIO_ACTOR_TONE_ENV_ATTACK,
    AUDIO_ACTOR_TONE_ENV_SUSTAIN,
    AUDIO_ACTOR_TONE_ENV_RELEASE,
} audio_actor_tone_env_t;

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
    QueueHandle_t msg_q;
    QueueHandle_t stream_q;
    msg_sub_handle_t sub_handle;
    TaskHandle_t task;
    bool stream_mode;
    bool tone_mode;
    audio_actor_tone_env_t tone_env;
    uint32_t tone_freq_hz;
    uint32_t tone_phase;
    uint32_t tone_env_sample;
    uint32_t stream_sample_rate;
} audio_actor_ctx_t;

static audio_actor_ctx_t s_actor = {0};
static int16_t s_tone_table[AUDIO_ACTOR_TONE_TABLE_SIZE];
static bool s_tone_table_ready;

static void audio_actor_init_tone_table(void)
{
    if(s_tone_table_ready) {
        return;
    }

    for(size_t i = 0; i < AUDIO_ACTOR_TONE_TABLE_SIZE; i++) {
        float phase = (2.0f * AUDIO_ACTOR_PI * (float)i) / (float)AUDIO_ACTOR_TONE_TABLE_SIZE;
        s_tone_table[i] = (int16_t)(sinf(phase) * 32767.0f);
    }
    s_tone_table_ready = true;
}

static void audio_actor_tone_start(uint32_t freq_hz)
{
    s_actor.tone_freq_hz = freq_hz;
    s_actor.tone_phase = 0;
    s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_ATTACK;
    s_actor.tone_env_sample = 0;
    s_actor.tone_mode = true;
}

static void audio_actor_tone_release(void)
{
    if(s_actor.tone_mode && s_actor.tone_env != AUDIO_ACTOR_TONE_ENV_OFF) {
        s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_RELEASE;
        s_actor.tone_env_sample = 0;
        return;
    }

    s_actor.tone_mode = false;
    s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_OFF;
    s_actor.tone_env_sample = 0;
    (void)audio_stop();
}

static esp_err_t audio_actor_post_cmd(const audio_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void audio_actor_handle_msg(const msg_t *msg)
{
    if(msg == NULL || msg->type != MSG_TYPE_CMD) {
        return;
    }

    switch(msg->event) {
        case MSG_EVT_CMD_AUDIO_TONE:
            (void)audio_actor_play_tone((uint32_t)msg->data.tone.freq, (uint32_t)msg->data.tone.duration, 0);
            break;
        case MSG_EVT_CMD_AUDIO_TONE_ON:
            if(msg->data.tone.freq > 0) {
                // LOG("audio tone on: freq=%d src=%d", msg->data.tone.freq, msg->src);
                audio_actor_tone_start((uint32_t)msg->data.tone.freq);
            }
            break;
        case MSG_EVT_CMD_AUDIO_TONE_OFF:
        case MSG_EVT_CMD_AUDIO_STOP:
            // LOG("audio tone off: event=%d src=%d", msg->event, msg->src);
            s_actor.stream_mode = false;
            if(s_actor.stream_q) {
                (void)xQueueReset(s_actor.stream_q);
            }
            audio_actor_tone_release();
            break;
        default:
            break;
    }
}

static void audio_actor_drain_msg_queue(void)
{
    if(s_actor.msg_q == NULL) {
        return;
    }

    msg_t msg;
    while(xQueueReceive(s_actor.msg_q, &msg, 0) == pdTRUE) {
        audio_actor_handle_msg(&msg);
    }
}

static void audio_actor_write_tone_chunk(void)
{
    if(!s_actor.tone_mode || s_actor.tone_env == AUDIO_ACTOR_TONE_ENV_OFF ||
       s_actor.tone_freq_hz == 0 || !audio_is_ready()) {
        return;
    }
    audio_actor_init_tone_table();

    uint32_t sample_rate = audio_get_sample_rate();
    if(sample_rate == 0) {
        return;
    }

    size_t sample_count = (sample_rate * AUDIO_ACTOR_TONE_CHUNK_MS) / 1000U;
    if(sample_count == 0) {
        sample_count = 1;
    }
    if(sample_count > AUDIO_ACTOR_TONE_MAX_SAMPLES) {
        sample_count = AUDIO_ACTOR_TONE_MAX_SAMPLES;
    }

    int16_t samples[AUDIO_ACTOR_TONE_MAX_SAMPLES];
    float volume = ((float)audio_get_volume() / 100.0f) * 0.35f;
    uint32_t phase_step = (uint32_t)(((uint64_t)s_actor.tone_freq_hz << AUDIO_ACTOR_TONE_PHASE_BITS) / sample_rate);
    uint32_t attack_samples = (sample_rate * AUDIO_ACTOR_TONE_ATTACK_MS) / 1000U;
    uint32_t release_samples = (sample_rate * AUDIO_ACTOR_TONE_RELEASE_MS) / 1000U;
    if(attack_samples == 0) {
        attack_samples = 1;
    }
    if(release_samples == 0) {
        release_samples = 1;
    }

    for(size_t i = 0; i < sample_count; i++) {
        float env = 1.0f;
        if(s_actor.tone_env == AUDIO_ACTOR_TONE_ENV_ATTACK) {
            env = (float)s_actor.tone_env_sample / (float)attack_samples;
            s_actor.tone_env_sample++;
            if(s_actor.tone_env_sample >= attack_samples) {
                s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_SUSTAIN;
                s_actor.tone_env_sample = 0;
                env = 1.0f;
            }
        } else if(s_actor.tone_env == AUDIO_ACTOR_TONE_ENV_RELEASE) {
            if(s_actor.tone_env_sample >= release_samples) {
                env = 0.0f;
                s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_OFF;
                s_actor.tone_mode = false;
            } else {
                env = (float)(release_samples - s_actor.tone_env_sample) / (float)release_samples;
                s_actor.tone_env_sample++;
            }
        }

        uint8_t idx = (uint8_t)(s_actor.tone_phase >> 24);
        samples[i] = (int16_t)((float)s_tone_table[idx] * volume * env);
        s_actor.tone_phase += phase_step;
    }

    esp_err_t err = audio_play_stream_chunk(samples, sample_count, portMAX_DELAY);
    if(err != ESP_OK) {
        static uint32_t s_drop_count;
        s_drop_count++;
        if((s_drop_count % 100U) == 1U) {
            LOG("audio tone chunk drop: err=%s count=%u", esp_err_to_name(err), (unsigned)s_drop_count);
        }
    }
    if(!s_actor.tone_mode || s_actor.tone_env == AUDIO_ACTOR_TONE_ENV_OFF) {
        s_actor.tone_env = AUDIO_ACTOR_TONE_ENV_OFF;
        s_actor.tone_env_sample = 0;
        (void)audio_stop();
    }
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void audio_actor_task(void *arg)
{
    (void)arg;

    audio_actor_cmd_t cmd;
    audio_stream_chunk_t chunk;

    while(1) {
        audio_actor_drain_msg_queue();

        if(s_actor.tone_mode && !s_actor.stream_mode) {
            audio_actor_drain_msg_queue();
            if(!s_actor.tone_mode || s_actor.stream_mode) {
                continue;
            }
            if(xQueueReceive(s_actor.cmd_q, &cmd, 0) == pdTRUE) {
                switch(cmd.type) {
                    case AUDIO_ACTOR_CMD_TONE_OFF:
                    case AUDIO_ACTOR_CMD_STOP:
                        audio_actor_tone_release();
                        break;
                    case AUDIO_ACTOR_CMD_TONE_ON:
                        audio_actor_tone_start(cmd.data.tone.freq_hz);
                        break;
                    default:
                        break;
                }
            }
            if(s_actor.tone_mode && !s_actor.stream_mode) {
                audio_actor_write_tone_chunk();
            }
            continue;
        }

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
            case AUDIO_ACTOR_CMD_TONE_ON:
                audio_actor_tone_start(cmd.data.tone.freq_hz);
                break;
            case AUDIO_ACTOR_CMD_TONE_OFF:
                audio_actor_tone_release();
                break;
            case AUDIO_ACTOR_CMD_FILE:
                (void)audio_play_file(cmd.data.file.path);
                break;
            case AUDIO_ACTOR_CMD_STOP:
                audio_actor_tone_release();
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
    cfg.volume_percent = (uint8_t)app_settings.audio_volume;
    esp_err_t err = audio_init(&cfg);
    if(err != ESP_OK) {
        LOG("audio_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_actor.cmd_q = xQueueCreate(AUDIO_ACTOR_CMD_QUEUE_LEN, sizeof(audio_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    err = msg_actor_queue_create_with_len(AUDIO_ACTOR_MSG_QUEUE_LEN, &s_actor.msg_q);
    if(err != ESP_OK) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        return err;
    }

    const msg_topic_t topics[] = {
        MSG_TOPIC_AUDIO_CMD,
    };
    err = msg_sub(s_actor.msg_q, topics, sizeof(topics) / sizeof(topics[0]), &s_actor.sub_handle);
    if(err != ESP_OK) {
        vQueueDelete(s_actor.msg_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.msg_q = NULL;
        s_actor.cmd_q = NULL;
        return err;
    }

    s_actor.stream_q = xQueueCreate(AUDIO_ACTOR_STREAM_QUEUE_LEN, sizeof(audio_stream_chunk_t));
    if(s_actor.stream_q == NULL) {
        (void)msg_unsub(s_actor.sub_handle, NULL, 0);
        vQueueDelete(s_actor.msg_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
        s_actor.msg_q = NULL;
        s_actor.cmd_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        audio_actor_task,
        "audio_actor",
        AUDIO_ACTOR_TASK_STACK,
        NULL,
        TASK_PRIO_AUDIO,
        &s_actor.task,
        TASK_CORE_AUDIO
    );
    if(ok != pdPASS) {
        (void)msg_unsub(s_actor.sub_handle, NULL, 0);
        vQueueDelete(s_actor.stream_q);
        vQueueDelete(s_actor.msg_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
        s_actor.stream_q = NULL;
        s_actor.msg_q = NULL;
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
    if(s_actor.sub_handle != MSG_SUB_HANDLE_INVALID) {
        (void)msg_unsub(s_actor.sub_handle, NULL, 0);
        s_actor.sub_handle = MSG_SUB_HANDLE_INVALID;
    }
    if(s_actor.stream_q) {
        vQueueDelete(s_actor.stream_q);
        s_actor.stream_q = NULL;
    }
    if(s_actor.msg_q) {
        vQueueDelete(s_actor.msg_q);
        s_actor.msg_q = NULL;
    }
    if(s_actor.cmd_q) {
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
    }

    s_actor.stream_mode = false;
    s_actor.tone_mode = false;
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

esp_err_t audio_actor_tone_on(uint32_t freq_hz, TickType_t timeout_ticks)
{
    if(freq_hz == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_TONE_ON,
        .data.tone = {
            .freq_hz = freq_hz,
            .duration_ms = 0,
        },
    };
    return audio_actor_post_cmd(&cmd, timeout_ticks);
}

esp_err_t audio_actor_tone_off(TickType_t timeout_ticks)
{
    audio_actor_cmd_t cmd = {
        .type = AUDIO_ACTOR_CMD_TONE_OFF,
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
