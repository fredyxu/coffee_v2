#include "audio.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "freertos/semphr.h"
#include "config/config_pin.h"
#include "config/config_sys.h"
#include "core/utils/log.h"

#define AUDIO_TONE_BUF_SAMPLES 256
#define AUDIO_PI 3.14159265358979323846f

typedef struct {
    bool inited;
    uint32_t sample_rate;
    uint8_t volume_percent;
    i2s_chan_handle_t tx_chan;
    SemaphoreHandle_t lock;
} audio_ctx_t;

static audio_ctx_t s_audio = {0};

static esp_err_t audio_reconfig_sample_rate_locked(uint32_t sample_rate)
{
    esp_err_t err = i2s_channel_disable(s_audio.tx_chan);
    if(err == ESP_OK) {
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        err = i2s_channel_reconfig_std_clock(s_audio.tx_chan, &clk_cfg);
    }
    if(err == ESP_OK) {
        err = i2s_channel_enable(s_audio.tx_chan);
    }
    if(err == ESP_OK) {
        s_audio.sample_rate = sample_rate;
    }
    return err;
}

static esp_err_t audio_lock_take(void)
{
    if(s_audio.lock == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return (xSemaphoreTake(s_audio.lock, portMAX_DELAY) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void audio_lock_give(void)
{
    if(s_audio.lock) {
        (void)xSemaphoreGive(s_audio.lock);
    }
}

audio_config_t audio_default_config(void)
{
    audio_config_t cfg = {
        .bclk_io = PIN_SOUND_I2S_BCLK,
        .ws_io = PIN_SOUND_I2S_WS,
        .dout_io = PIN_SOUND_I2S_DOUT,
        .sample_rate = SOUND_SAMPLE_RATE,
        .volume_percent = 85,
    };
    return cfg;
}

esp_err_t audio_init(const audio_config_t *cfg)
{
    if(s_audio.inited) {
        return ESP_OK;
    }

    if(cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    s_audio.lock = xSemaphoreCreateMutex();
    if(s_audio.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(SOUND_I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t err = i2s_new_channel(&chan_cfg, &s_audio.tx_chan, NULL);
    if(err != ESP_OK) {
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(cfg->sample_rate),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = cfg->bclk_io,
            .ws = cfg->ws_io,
            .dout = cfg->dout_io,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    err = i2s_channel_init_std_mode(s_audio.tx_chan, &std_cfg);
    if(err != ESP_OK) {
        return err;
    }

    err = i2s_channel_enable(s_audio.tx_chan);
    if(err != ESP_OK) {
        return err;
    }

    s_audio.sample_rate = cfg->sample_rate;
    s_audio.volume_percent = (cfg->volume_percent > 100) ? 100 : cfg->volume_percent;
    s_audio.inited = true;

    LOG("audio init done (bclk=%d ws=%d dout=%d sr=%u)", cfg->bclk_io, cfg->ws_io, cfg->dout_io, (unsigned)cfg->sample_rate);
    return ESP_OK;
}

esp_err_t audio_deinit(void)
{
    if(!s_audio.inited) {
        return ESP_OK;
    }

    if(s_audio.tx_chan) {
        (void)i2s_channel_disable(s_audio.tx_chan);
        (void)i2s_del_channel(s_audio.tx_chan);
        s_audio.tx_chan = NULL;
    }

    if(s_audio.lock) {
        vSemaphoreDelete(s_audio.lock);
        s_audio.lock = NULL;
    }

    s_audio.inited = false;
    return ESP_OK;
}

bool audio_is_ready(void)
{
    return s_audio.inited;
}

uint32_t audio_get_sample_rate(void)
{
    return s_audio.sample_rate;
}

uint8_t audio_get_volume(void)
{
    return s_audio.volume_percent;
}

esp_err_t audio_set_volume(uint8_t volume_percent)
{
    if(volume_percent > 100) {
        volume_percent = 100;
    }

    if(!s_audio.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if(audio_lock_take() != ESP_OK) {
        return ESP_ERR_TIMEOUT;
    }

    s_audio.volume_percent = volume_percent;
    audio_lock_give();
    return ESP_OK;
}

esp_err_t audio_set_sample_rate(uint32_t sample_rate)
{
    if(!s_audio.inited || sample_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if(sample_rate == s_audio.sample_rate) {
        return ESP_OK;
    }

    esp_err_t err = audio_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    err = audio_reconfig_sample_rate_locked(sample_rate);

    audio_lock_give();
    return err;
}

static esp_err_t audio_write_samples(const int16_t *samples, size_t sample_count, TickType_t timeout_ticks)
{
    if(!s_audio.inited || samples == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t written = 0;
    size_t bytes = sample_count * sizeof(int16_t);
    esp_err_t err = i2s_channel_write(s_audio.tx_chan, samples, bytes, &written, timeout_ticks);
    if(err != ESP_OK) {
        return err;
    }

    return (written == bytes) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t audio_play_pcm16(const int16_t *samples, size_t sample_count, TickType_t timeout_ticks)
{
    esp_err_t err = audio_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    err = audio_write_samples(samples, sample_count, timeout_ticks);
    audio_lock_give();
    return err;
}

esp_err_t audio_play_stream_chunk(const int16_t *samples, size_t sample_count, TickType_t timeout_ticks)
{
    return audio_play_pcm16(samples, sample_count, timeout_ticks);
}

esp_err_t audio_play_tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if(!s_audio.inited || freq_hz == 0 || duration_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = audio_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    const uint32_t total_samples = (s_audio.sample_rate * duration_ms) / 1000U;
    const float step = 2.0f * AUDIO_PI * (float)freq_hz / (float)s_audio.sample_rate;
    float phase = 0.0f;

    const float volume = ((float)s_audio.volume_percent / 100.0f) * 0.35f;
    int16_t buf[AUDIO_TONE_BUF_SAMPLES];

    uint32_t generated = 0;
    while(generated < total_samples) {
        size_t n = total_samples - generated;
        if(n > AUDIO_TONE_BUF_SAMPLES) {
            n = AUDIO_TONE_BUF_SAMPLES;
        }

        for(size_t i = 0; i < n; i++) {
            float s = sinf(phase) * 32767.0f * volume;
            if(s > 32767.0f) s = 32767.0f;
            if(s < -32768.0f) s = -32768.0f;
            buf[i] = (int16_t)s;
            phase += step;
            if(phase >= 2.0f * AUDIO_PI) {
                phase -= 2.0f * AUDIO_PI;
            }
        }

        err = audio_write_samples(buf, n, pdMS_TO_TICKS(100));
        if(err != ESP_OK) {
            audio_lock_give();
            return err;
        }

        generated += (uint32_t)n;
    }

    audio_lock_give();
    return ESP_OK;
}

typedef struct {
    bool valid;
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;
    uint32_t data_size;
} wav_info_t;

static uint32_t read_le32(const uint8_t *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static wav_info_t parse_wav_header(FILE *fp)
{
    wav_info_t info = {0};
    uint8_t head[12];
    if(fread(head, 1, sizeof(head), fp) != sizeof(head)) {
        return info;
    }

    if(memcmp(head, "RIFF", 4) != 0 || memcmp(&head[8], "WAVE", 4) != 0) {
        return info;
    }

    bool fmt_ok = false;
    bool data_ok = false;

    while(true) {
        uint8_t ch[8];
        if(fread(ch, 1, sizeof(ch), fp) != sizeof(ch)) {
            break;
        }

        uint32_t chunk_size = read_le32(&ch[4]);
        if(memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[32] = {0};
            size_t read_n = (chunk_size > sizeof(fmt)) ? sizeof(fmt) : chunk_size;
            if(fread(fmt, 1, read_n, fp) != read_n) {
                break;
            }
            if(chunk_size > read_n) {
                (void)fseek(fp, (long)(chunk_size - read_n), SEEK_CUR);
            }

            uint16_t audio_fmt = read_le16(&fmt[0]);
            info.channels = read_le16(&fmt[2]);
            info.sample_rate = read_le32(&fmt[4]);
            info.bits_per_sample = read_le16(&fmt[14]);
            fmt_ok = (audio_fmt == 1);
        } else if(memcmp(ch, "data", 4) == 0) {
            info.data_size = chunk_size;
            data_ok = true;
            break;
        } else {
            (void)fseek(fp, (long)chunk_size, SEEK_CUR);
        }
    }

    info.valid = fmt_ok && data_ok;
    return info;
}

esp_err_t audio_play_file(const char *path)
{
    if(path == NULL || !s_audio.inited) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *fp = fopen(path, "rb");
    if(fp == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = audio_lock_take();
    if(err != ESP_OK) {
        fclose(fp);
        return err;
    }

    wav_info_t wav = parse_wav_header(fp);
    bool is_wav = wav.valid;

    if(is_wav) {
        if(wav.bits_per_sample != 16 || (wav.channels != 1 && wav.channels != 2)) {
            audio_lock_give();
            fclose(fp);
            return ESP_ERR_NOT_SUPPORTED;
        }

        if(wav.sample_rate != s_audio.sample_rate) {
            err = audio_reconfig_sample_rate_locked(wav.sample_rate);
            if(err != ESP_OK) {
                audio_lock_give();
                fclose(fp);
                return err;
            }
        }
    } else {
        (void)fseek(fp, 0, SEEK_SET);
    }

    uint8_t raw[1024];
    int16_t mono[512];

    while(true) {
        size_t n = fread(raw, 1, sizeof(raw), fp);
        if(n == 0) {
            break;
        }

        if(is_wav && wav.channels == 2) {
            size_t frames = n / 4;
            for(size_t i = 0; i < frames; i++) {
                mono[i] = (int16_t)(raw[i * 4] | (raw[i * 4 + 1] << 8));
            }
            err = audio_write_samples(mono, frames, pdMS_TO_TICKS(100));
        } else {
            err = audio_write_samples((const int16_t *)raw, n / 2, pdMS_TO_TICKS(100));
        }

        if(err != ESP_OK) {
            break;
        }
    }

    audio_lock_give();
    fclose(fp);
    return err;
}

esp_err_t audio_stop(void)
{
    if(!s_audio.inited) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = audio_lock_take();
    if(err != ESP_OK) {
        return err;
    }

    (void)i2s_channel_disable(s_audio.tx_chan);
    err = i2s_channel_enable(s_audio.tx_chan);

    audio_lock_give();
    return err;
}
