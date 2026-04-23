#include "tests/mic/test_mic.h"

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "core/utils/log.h"
#include "modules/mic/mic_actor.h"

void test_mic_run(void)
{
    LOG("[TEST][MIC] start");

    esp_err_t err = mic_actor_start(pdMS_TO_TICKS(100));
    if(err != ESP_OK) {
        LOG("[TEST][MIC] failed: mic_actor_start err=%s", esp_err_to_name(err));
        return;
    }

    mic_frame_t frame = {0};
    err = mic_actor_pop_frame(&frame, pdMS_TO_TICKS(600));
    if(err == ESP_OK && frame.sample_count > 0 && frame.sample_rate > 0) {
        LOG("[TEST][MIC] pass: frame sr=%u samples=%u ts=%u sample0=%d",
            (unsigned)frame.sample_rate,
            (unsigned)frame.sample_count,
            (unsigned)frame.timestamp,
            (int)frame.samples[0]);
    } else {
        LOG("[TEST][MIC] warn: no valid frame err=%s sr=%u samples=%u",
            esp_err_to_name(err),
            (unsigned)frame.sample_rate,
            (unsigned)frame.sample_count);
    }

    (void)mic_actor_stop(pdMS_TO_TICKS(100));
    LOG("[TEST][MIC] done");
}
