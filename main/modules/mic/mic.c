#include "mic.h"

#include <limits.h>
#include <string.h>

#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "freertos/semphr.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/utils/log.h"

typedef struct {
    bool inited;
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    i2s_chan_handle_t rx_chan;
    SemaphoreHandle_t lock;
    StaticSemaphore_t lock_buf;
    uint32_t raw_read_timeout_count;
    uint32_t raw_read_error_count;
} mic_ctx_t;

static mic_ctx_t s_mic = {0};

static void mic_cleanup_channel_locked(void)
{
    if(s_mic.rx_chan) {
        (void)i2s_channel_disable(s_mic.rx_chan);
        (void)i2s_del_channel(s_mic.rx_chan);
        s_mic.rx_chan = NULL;
    }
}

static esp_err_t mic_reconfig_sample_rate_locked(uint32_t sample_rate)
{
    esp_err_t err = i2s_channel_disable(s_mic.rx_chan);
    if(err == ESP_OK) {
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        err = i2s_channel_reconfig_std_clock(s_mic.rx_chan, &clk_cfg);
    }
    if(err == ESP_OK) {
        err = i2s_channel_enable(s_mic.rx_chan);
    }
    if(err == ESP_OK) {
        s_mic.sample_rate = sample_rate;
    }
    return err;
}

static esp_err_t mic_lock_take(void)
{
    if(s_mic.lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return (xSemaphoreTake(s_mic.lock, portMAX_DELAY) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void mic_lock_give(void)
{
    if(s_mic.lock) {
        (void)xSemaphoreGive(s_mic.lock);
    }
}

mic_config_t mic_default_config(void)
{
    mic_config_t cfg = {
        .bclk_io = PIN_MIC_I2S_BCLK,
        .ws_io = PIN_MIC_I2S_WS,
        .din_io = PIN_MIC_I2S_DIN,
        .sample_rate = MIC_SAMPLE_RATE,
        .bits_per_sample = (uint8_t)MIC_BITS_PER_SAMPLE,
    };
    return cfg;
}

esp_err_t mic_init(const mic_config_t *cfg)
{
    if(s_mic.inited) {
        return ESP_OK;
    }

    if(cfg == NULL || cfg->sample_rate == 0 || cfg->bits_per_sample == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_mic.lock = xSemaphoreCreateMutexStatic(&s_mic.lock_buf);
    if(s_mic.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(MIC_I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = MIC_I2S_DMA_DESC_NUM;
    chan_cfg.dma_frame_num = MIC_I2S_DMA_FRAME_NUM;
    chan_cfg.auto_clear = true;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_mic.rx_chan);
    if(err != ESP_OK) {
        LOG("mic i2s new channel failed: port=%d err=%s", MIC_I2S_PORT, esp_err_to_name(err));
        s_mic.rx_chan = NULL;
        if(s_mic.lock) {
            vSemaphoreDelete(s_mic.lock);
            s_mic.lock = NULL;
        }
        return err;
    }

    i2s_data_bit_width_t bit_width = (cfg->bits_per_sample == 16) ? I2S_DATA_BIT_WIDTH_16BIT : I2S_DATA_BIT_WIDTH_32BIT;

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bit_width, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->bclk_io,
            .ws = cfg->ws_io,
            .dout = I2S_GPIO_UNUSED,
            .din = cfg->din_io,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
    LOG("mic i2s init: port=%d bclk=%d ws=%d din=%d sr=%u bits=%u mode=philips-stereo-right",
        MIC_I2S_PORT,
        cfg->bclk_io,
        cfg->ws_io,
        cfg->din_io,
        (unsigned)cfg->sample_rate,
        (unsigned)cfg->bits_per_sample);

    err = i2s_channel_init_std_mode(s_mic.rx_chan, &std_cfg);
    if(err != ESP_OK) {
        LOG("mic i2s std init failed: %s", esp_err_to_name(err));
        mic_cleanup_channel_locked();
        if(s_mic.lock) {
            vSemaphoreDelete(s_mic.lock);
            s_mic.lock = NULL;
        }
        return err;
    }

    err = i2s_channel_enable(s_mic.rx_chan);
    if(err != ESP_OK) {
        LOG("mic i2s enable failed: %s", esp_err_to_name(err));
        mic_cleanup_channel_locked();
        if(s_mic.lock) {
            vSemaphoreDelete(s_mic.lock);
            s_mic.lock = NULL;
        }
        return err;
    }

    s_mic.sample_rate = cfg->sample_rate;
    s_mic.bits_per_sample = cfg->bits_per_sample;
    s_mic.raw_read_timeout_count = 0;
    s_mic.raw_read_error_count = 0;
    s_mic.inited = true;

    return ESP_OK;
}

esp_err_t mic_deinit(void)
{
    if(!s_mic.inited) {
        return ESP_OK;
    }

    if(s_mic.rx_chan) {
        mic_cleanup_channel_locked();
    }

    if(s_mic.lock) {
        vSemaphoreDelete(s_mic.lock);
        s_mic.lock = NULL;
    }

    s_mic.inited = false;
    return ESP_OK;
}

bool mic_is_ready(void)
{
    return s_mic.inited;
}

uint32_t mic_get_sample_rate(void)
{
    return s_mic.sample_rate;
}

esp_err_t mic_set_sample_rate(uint32_t sample_rate)
{
    if(!s_mic.inited || sample_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if(sample_rate == s_mic.sample_rate) {
        return ESP_OK;
    }

    esp_err_t err = mic_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    err = mic_reconfig_sample_rate_locked(sample_rate);
    mic_lock_give();
    return err;
}

esp_err_t mic_read_raw(void *buf, size_t bytes, size_t *out_bytes, TickType_t timeout_ticks)
{
    if(!s_mic.inited || buf == NULL || bytes == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = mic_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    size_t read_bytes = 0;
    err = i2s_channel_read(s_mic.rx_chan, buf, bytes, &read_bytes, timeout_ticks);
    if(out_bytes) {
        *out_bytes = read_bytes;
    }

    if(err == ESP_ERR_TIMEOUT) {
        s_mic.raw_read_timeout_count++;
        if(s_mic.raw_read_timeout_count <= 3 || (s_mic.raw_read_timeout_count % 10000U) == 0U ||
           read_bytes > 0) {
            LOG("mic raw read timeout: requested=%u got=%u count=%u",
                (unsigned)bytes,
                (unsigned)read_bytes,
                (unsigned)s_mic.raw_read_timeout_count);
        }
    } else if(err != ESP_OK) {
        s_mic.raw_read_error_count++;
        LOG("mic raw read failed: err=%s requested=%u got=%u count=%u",
            esp_err_to_name(err),
            (unsigned)bytes,
            (unsigned)read_bytes,
            (unsigned)s_mic.raw_read_error_count);
    }

    mic_lock_give();
    return err;
}

static int16_t mic_s32_to_s16(int32_t v)
{
    v >>= 17;
    if(v > INT16_MAX) v = INT16_MAX;
    if(v < INT16_MIN) v = INT16_MIN;
    return (int16_t)v;
}

esp_err_t mic_read_pcm16(int16_t *samples, size_t sample_count, TickType_t timeout_ticks)
{
    if(!s_mic.inited || samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if(s_mic.bits_per_sample == 16) {
        size_t out_bytes = 0;
        esp_err_t err = mic_read_raw(samples, sample_count * sizeof(int16_t), &out_bytes, timeout_ticks);
        if(err != ESP_OK) {
            return err;
        }
        return (out_bytes == sample_count * sizeof(int16_t)) ? ESP_OK : ESP_ERR_TIMEOUT;
    }

    if(s_mic.bits_per_sample == 32) {
        int32_t tmp[256];
        size_t total = 0;

        while(total < sample_count) {
            size_t n = sample_count - total;
            if(n > 256) {
                n = 256;
            }

            size_t out_bytes = 0;
            esp_err_t err = mic_read_raw(tmp, n * sizeof(int32_t), &out_bytes, timeout_ticks);
            if(err != ESP_OK) {
                return err;
            }
            if(out_bytes != n * sizeof(int32_t)) {
                return ESP_ERR_TIMEOUT;
            }

            for(size_t i = 0; i < n; i++) {
                samples[total + i] = mic_s32_to_s16(tmp[i]);
            }
            total += n;
        }

        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t mic_read_pcm16_some(int16_t *samples, size_t max_samples, size_t *out_samples)
{
    if(!s_mic.inited || samples == NULL || out_samples == NULL || max_samples == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_samples = 0;

    if(s_mic.bits_per_sample == 16) {
        size_t out_bytes = 0;
        esp_err_t err = mic_read_raw(samples, max_samples * sizeof(int16_t), &out_bytes, 0);
        if(err == ESP_ERR_TIMEOUT) {
            return ESP_OK;
        }
        if(err != ESP_OK) {
            return err;
        }

        *out_samples = out_bytes / sizeof(int16_t);
        return ESP_OK;
    }

    if(s_mic.bits_per_sample == 32) {
        int32_t tmp[256];
        size_t n = max_samples;
        if(n > 256) {
            n = 256;
        }

        size_t out_bytes = 0;
        esp_err_t err = mic_read_raw(tmp, n * sizeof(int32_t), &out_bytes, 0);
        if(err == ESP_ERR_TIMEOUT) {
            return ESP_OK;
        }
        if(err != ESP_OK) {
            return err;
        }

        size_t got = out_bytes / sizeof(int32_t);
        for(size_t i = 0; i < got; i++) {
            samples[i] = mic_s32_to_s16(tmp[i]);
        }
        *out_samples = got;
        return ESP_OK;
    }

    return ESP_ERR_NOT_SUPPORTED;
}
