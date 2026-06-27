#include "mic_actor.h"

#include <limits.h>
#include <stdbool.h>
#include <string.h>

#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
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
    uint32_t capture_ok_count;
    uint32_t capture_timeout_count;
    uint32_t capture_error_count;
    uint32_t frame_drop_count;
} mic_actor_ctx_t;

static mic_actor_ctx_t s_actor = {0};
static const TickType_t MIC_ACTOR_ERROR_BACKOFF_TICKS = pdMS_TO_TICKS(10);
static const TickType_t MIC_ACTOR_FRAME_READ_TIMEOUT_TICKS = pdMS_TO_TICKS(INTERCOM_AUDIO_FRAME_MS * 2);
static const TickType_t MIC_ACTOR_TIMEOUT_BACKOFF_TICKS = pdMS_TO_TICKS(10);
static int32_t s_dc_estimate_q8;
static int32_t s_noise_gain_q15 = 32768;
static int16_t s_lowpass_prev_sample;
static int16_t s_lowpass_prev2_sample;

static void mic_actor_log_heap(const char *stage)
{
    LOG("mic actor heap: stage=%s free=%u largest=%u",
        stage != NULL ? stage : "-",
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

static esp_err_t mic_actor_post_cmd(const mic_actor_cmd_t *cmd, TickType_t timeout_ticks)
{
    if(s_actor.cmd_q == NULL || cmd == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return (xQueueSend(s_actor.cmd_q, cmd, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void mic_actor_log_frame_level(const mic_frame_t *frame)
{
    if(frame == NULL || frame->sample_count == 0) {
        return;
    }

    int32_t peak = 0;
    int64_t sum = 0;
    uint64_t abs_sum = 0;
    for(size_t i = 0; i < frame->sample_count; i++) {
        int32_t sample = frame->samples[i];
        int32_t abs_sample = sample < 0 ? -sample : sample;
        if(abs_sample > peak) {
            peak = abs_sample;
        }
        sum += sample;
        abs_sum += (uint32_t)abs_sample;
    }

    int32_t dc = (int32_t)(sum / (int64_t)frame->sample_count);
    uint32_t mean_abs = (uint32_t)(abs_sum / frame->sample_count);
    LOG("mic frame level: count=%u peak=%d mean_abs=%u dc=%d first=%d",
        (unsigned)s_actor.capture_ok_count,
        (int)peak,
        (unsigned)mean_abs,
        (int)dc,
        (int)frame->samples[0]);
}

static void mic_actor_filter_dc(mic_frame_t *frame)
{
    if(frame == NULL || frame->sample_count == 0) {
        return;
    }

    for(size_t i = 0; i < frame->sample_count; i++) {
        int32_t input_q8 = ((int32_t)frame->samples[i]) << 8;
        s_dc_estimate_q8 += (input_q8 - s_dc_estimate_q8) >> 8;
        int32_t sample = (input_q8 - s_dc_estimate_q8) >> 8;
        if(sample > INT16_MAX) {
            sample = INT16_MAX;
        } else if(sample < INT16_MIN) {
            sample = INT16_MIN;
        }
        frame->samples[i] = (int16_t)sample;
	}
}

static uint32_t mic_actor_frame_mean_abs(const mic_frame_t *frame)
{
	if(frame == NULL || frame->sample_count == 0) {
		return 0;
	}

	uint64_t abs_sum = 0;
	for(size_t i = 0; i < frame->sample_count; i++) {
		int32_t sample = frame->samples[i];
		abs_sum += (uint32_t)(sample < 0 ? -sample : sample);
	}
	return (uint32_t)(abs_sum / frame->sample_count);
}

static void mic_actor_noise_gate_frame(mic_frame_t *frame)
{
	if(frame == NULL || frame->sample_count == 0) {
		return;
	}

	uint32_t mean_abs = mic_actor_frame_mean_abs(frame);
	int32_t target_gain_q15 = MIC_NOISE_GATE_FULL_GAIN_Q15;
	if(mean_abs < MIC_NOISE_GATE_LOW_MEAN_ABS) {
		target_gain_q15 = MIC_NOISE_GATE_LOW_GAIN_Q15;
	} else if(mean_abs < MIC_NOISE_GATE_MID_MEAN_ABS) {
		target_gain_q15 = MIC_NOISE_GATE_MID_GAIN_Q15;
	} else if(mean_abs < MIC_NOISE_GATE_HIGH_MEAN_ABS) {
		target_gain_q15 = MIC_NOISE_GATE_HIGH_GAIN_Q15;
	}

	int32_t delta = target_gain_q15 - s_noise_gain_q15;
	if(delta > 0) {
		s_noise_gain_q15 += delta >> 1;
	} else {
		s_noise_gain_q15 += delta >> 4;
	}

	for(size_t i = 0; i < frame->sample_count; i++) {
		int32_t sample = ((int32_t)frame->samples[i] * s_noise_gain_q15) >> 15;
		frame->samples[i] = (int16_t)sample;
	}
}

static void mic_actor_lowpass_frame(mic_frame_t *frame)
{
	if(frame == NULL || frame->sample_count == 0) {
		return;
	}

	int16_t prev1 = s_lowpass_prev_sample;
	int16_t prev2 = s_lowpass_prev2_sample;
	for(size_t i = 0; i < frame->sample_count; i++) {
		int16_t input = frame->samples[i];
		frame->samples[i] = (int16_t)(((int32_t)prev2 + ((int32_t)prev1 * 2) + input) / 4);
		prev2 = prev1;
		prev1 = input;
	}
	s_lowpass_prev2_sample = prev2;
	s_lowpass_prev_sample = prev1;
}

static void mic_actor_limit_frame(mic_frame_t *frame)
{
    if(frame == NULL || frame->sample_count == 0) {
        return;
    }

	const int32_t limit = MIC_AUDIO_LIMIT;
    for(size_t i = 0; i < frame->sample_count; i++) {
        int32_t sample = frame->samples[i];
        if(sample > limit) {
            sample = limit;
        } else if(sample < -limit) {
            sample = -limit;
        }
        frame->samples[i] = (int16_t)sample;
    }
}

static void mic_actor_task(void *arg)
{
    (void)arg;

    mic_actor_cmd_t cmd;
    mic_frame_t frame;

    while(1) {
        if(s_actor.capturing) {
            if(xQueueReceive(s_actor.cmd_q, &cmd, 0) == pdTRUE) {
                if(cmd.type == MIC_ACTOR_CMD_STOP) {
                    s_actor.capturing = false;
                    LOG("mic capture stopped: frames=%u timeouts=%u errors=%u drops=%u",
                        (unsigned)s_actor.capture_ok_count,
                        (unsigned)s_actor.capture_timeout_count,
                        (unsigned)s_actor.capture_error_count,
                        (unsigned)s_actor.frame_drop_count);
                } else if(cmd.type == MIC_ACTOR_CMD_SET_SR && cmd.sample_rate > 0) {
                    (void)mic_set_sample_rate(cmd.sample_rate);
                } else if(cmd.type == MIC_ACTOR_CMD_START) {
                    s_actor.capturing = true;
                    s_actor.capture_ok_count = 0;
                    s_actor.capture_timeout_count = 0;
                    s_actor.capture_error_count = 0;
                    s_actor.frame_drop_count = 0;
                    s_dc_estimate_q8 = 0;
                    s_noise_gain_q15 = 32768;
                    s_lowpass_prev_sample = 0;
                    s_lowpass_prev2_sample = 0;
                    LOG("mic capture started: sample_rate=%u frame_samples=%u",
                        (unsigned)mic_get_sample_rate(),
                        (unsigned)MIC_ACTOR_FRAME_MAX_SAMPLES);
                }
                continue;
            }

            frame.sample_rate = mic_get_sample_rate();
            frame.sample_count = MIC_ACTOR_FRAME_MAX_SAMPLES;
            frame.timestamp = (uint32_t)xTaskGetTickCount();

            esp_err_t err = mic_read_pcm16(
                frame.samples,
                MIC_ACTOR_FRAME_MAX_SAMPLES,
                MIC_ACTOR_FRAME_READ_TIMEOUT_TICKS
			);
			if(err == ESP_OK) {
				mic_actor_filter_dc(&frame);
				mic_actor_lowpass_frame(&frame);
				mic_actor_noise_gate_frame(&frame);
				mic_actor_limit_frame(&frame);
				s_actor.capture_ok_count++;
                if(s_actor.capture_ok_count <= 5 || (s_actor.capture_ok_count % 50U) == 0U) {
                    LOG("mic frame captured: count=%u sample_rate=%u samples=%u",
                        (unsigned)s_actor.capture_ok_count,
                        (unsigned)frame.sample_rate,
                        (unsigned)frame.sample_count);
                    mic_actor_log_frame_level(&frame);
                }
                if(xQueueSend(s_actor.frame_q, &frame, 0) != pdTRUE) {
                    mic_frame_t stale;
                    (void)xQueueReceive(s_actor.frame_q, &stale, 0);
                    (void)xQueueSend(s_actor.frame_q, &frame, 0);
                    s_actor.frame_drop_count++;
                    if(s_actor.frame_drop_count <= 5 || (s_actor.frame_drop_count % 50U) == 0U) {
                        LOG("mic frame queue full, dropped stale frame: drops=%u captured=%u",
                            (unsigned)s_actor.frame_drop_count,
                            (unsigned)s_actor.capture_ok_count);
                    }
                }
            } else if(err == ESP_ERR_TIMEOUT) {
                s_actor.capture_timeout_count++;
                if(s_actor.capture_timeout_count <= 3 || (s_actor.capture_timeout_count % 10000U) == 0U) {
                    LOG("mic capture timeout: count=%u frames=%u",
                        (unsigned)s_actor.capture_timeout_count,
                        (unsigned)s_actor.capture_ok_count);
                }
                vTaskDelay(MIC_ACTOR_TIMEOUT_BACKOFF_TICKS);
            } else {
                s_actor.capture_error_count++;
                LOG("mic capture err: %s", esp_err_to_name(err));
                vTaskDelay(MIC_ACTOR_ERROR_BACKOFF_TICKS);
            }
        } else {
            if(xQueueReceive(s_actor.cmd_q, &cmd, portMAX_DELAY) != pdTRUE) {
                continue;
            }

            if(cmd.type == MIC_ACTOR_CMD_START) {
                s_actor.capturing = true;
                s_actor.capture_ok_count = 0;
                s_actor.capture_timeout_count = 0;
                s_actor.capture_error_count = 0;
                s_actor.frame_drop_count = 0;
                s_dc_estimate_q8 = 0;
                s_noise_gain_q15 = 32768;
                s_lowpass_prev_sample = 0;
                s_lowpass_prev2_sample = 0;
                LOG("mic capture started: sample_rate=%u frame_samples=%u",
                    (unsigned)mic_get_sample_rate(),
                    (unsigned)MIC_ACTOR_FRAME_MAX_SAMPLES);
            } else if(cmd.type == MIC_ACTOR_CMD_SET_SR && cmd.sample_rate > 0) {
                (void)mic_set_sample_rate(cmd.sample_rate);
            }
        }
    }
}

esp_err_t mic_actor_init(void)
{
    if(s_actor.task != NULL) {
        LOG("mic actor init skipped: task already running");
        return ESP_OK;
    }

    mic_actor_log_heap("before_init");
    mic_config_t cfg = mic_default_config();
    esp_err_t err = mic_init(&cfg);
    if(err != ESP_OK) {
        LOG("mic_init failed: %s", esp_err_to_name(err));
        mic_actor_log_heap("after_mic_init_fail");
        return err;
    }
    mic_actor_log_heap("after_mic_init");

    s_actor.cmd_q = xQueueCreate(MIC_ACTOR_CMD_QUEUE_LEN, sizeof(mic_actor_cmd_t));
    if(s_actor.cmd_q == NULL) {
        LOG("mic actor cmd queue create failed: len=%u", (unsigned)MIC_ACTOR_CMD_QUEUE_LEN);
        mic_actor_log_heap("cmd_queue_fail");
        (void)mic_deinit();
        return ESP_ERR_NO_MEM;
    }

    s_actor.frame_q = xQueueCreate(MIC_ACTOR_FRAME_QUEUE_LEN, sizeof(mic_frame_t));
    if(s_actor.frame_q == NULL) {
        LOG("mic actor frame queue create failed: len=%u frame_bytes=%u",
            (unsigned)MIC_ACTOR_FRAME_QUEUE_LEN,
            (unsigned)sizeof(mic_frame_t));
        mic_actor_log_heap("frame_queue_fail");
        vQueueDelete(s_actor.cmd_q);
        s_actor.cmd_q = NULL;
        (void)mic_deinit();
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
        LOG("mic actor task create failed: stack=%u prio=%u core=%d frame_bytes=%u",
            (unsigned)MIC_ACTOR_TASK_STACK,
            (unsigned)TASK_PRIO_MIC,
            (int)TASK_CORE_MIC,
            (unsigned)sizeof(mic_frame_t));
        mic_actor_log_heap("task_create_fail");
        vQueueDelete(s_actor.frame_q);
        vQueueDelete(s_actor.cmd_q);
        s_actor.frame_q = NULL;
        s_actor.cmd_q = NULL;
        (void)mic_deinit();
        return ESP_FAIL;
    }

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
