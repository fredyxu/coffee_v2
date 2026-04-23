#include "tests/encoder/test_encoder.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_random.h"
#include "config/config_sys.h"
#include "core/utils/log.h"
#include "modules/audio/audio.h"
#include "modules/audio/audio_actor.h"
#include "modules/input/encoder/encoder.h"

static TaskHandle_t s_test_task = NULL;

static int clamp_volume(int vol)
{
    if(vol < 0) {
        return 0;
    }
    if(vol > 100) {
        return 100;
    }
    return vol;
}

static void test_encoder_audio_task(void *arg)
{
    (void)arg;

    uint8_t prev_state = (uint8_t)(((encoder_read_a() & 0x1) << 1) | (encoder_read_b() & 0x1));
    int8_t acc = 0;
    TickType_t last_sw_tick = 0;
    TickType_t last_rot_tick = 0;
    int prev_sw = encoder_read_sw();

    static const int8_t trans_lut[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0,
    };

    LOG("[TEST][ENCODER+AUDIO] interactive test started");
    LOG("[TEST][ENCODER+AUDIO] rotate: adjust volume, press: random tone");

    while(1) {
        uint8_t a = (uint8_t)(encoder_read_a() & 0x1);
        uint8_t b = (uint8_t)(encoder_read_b() & 0x1);
        uint8_t cur = (uint8_t)((a << 1) | b);
        uint8_t idx = (uint8_t)((prev_state << 2) | cur);
        prev_state = cur;
        acc += trans_lut[idx];

        if(acc >= 4) {
            TickType_t now = xTaskGetTickCount();
            if((now - last_rot_tick) >= pdMS_TO_TICKS(60)) {
                last_rot_tick = now;
                acc = 0;
                int next_vol = clamp_volume((int)audio_get_volume() + 5);
                esp_err_t err = audio_set_volume((uint8_t)next_vol);
                LOG("[TEST][ENCODER+AUDIO] CW -> volume=%d err=%s", next_vol, esp_err_to_name(err));
            }
        } else if(acc <= -4) {
            TickType_t now = xTaskGetTickCount();
            if((now - last_rot_tick) >= pdMS_TO_TICKS(60)) {
                last_rot_tick = now;
                acc = 0;
                int next_vol = clamp_volume((int)audio_get_volume() - 5);
                esp_err_t err = audio_set_volume((uint8_t)next_vol);
                LOG("[TEST][ENCODER+AUDIO] CCW -> volume=%d err=%s", next_vol, esp_err_to_name(err));
            }
        }

        TickType_t now = xTaskGetTickCount();
        int sw = encoder_read_sw();
        if(prev_sw != 0 && sw == 0 && (now - last_sw_tick) >= pdMS_TO_TICKS(220)) {
            last_sw_tick = now;

            uint32_t rnd = esp_random();
            uint32_t freq = 300 + (rnd % 1701);           // 300~2000 Hz
            uint32_t duration = 80 + ((rnd >> 11) % 161); // 80~240 ms
            esp_err_t err = audio_actor_play_tone(freq, duration, pdMS_TO_TICKS(20));
            LOG("[TEST][ENCODER+AUDIO] PRESS -> tone=%luHz dur=%lums err=%s",
                (unsigned long)freq,
                (unsigned long)duration,
                esp_err_to_name(err));
        }
        prev_sw = sw;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void test_encoder_run(void)
{
    if(s_test_task != NULL) {
        LOG("[TEST][ENCODER+AUDIO] already running");
        return;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        test_encoder_audio_task,
        "test_enc_audio",
        4096,
        NULL,
        1,
        &s_test_task,
        TASK_CORE_LVGL
    );
    if(ok != pdPASS) {
        s_test_task = NULL;
        LOG("[TEST][ENCODER+AUDIO] failed: cannot create test task");
        return;
    }

    LOG("[TEST][ENCODER+AUDIO] started");
}
