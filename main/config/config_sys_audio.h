#pragma once

/* ============================================================
 * 音频系统配置（I2S / DMA）
 * ============================================================ */

/* I2S 端口：0=I2S0, 1=I2S1 */
#define SOUND_I2S_PORT 0
#define MIC_I2S_PORT 1

/* 扬声器输出参数 */
#define SOUND_SAMPLE_RATE 44100
#define SOUND_BITS_PER_SAMPLE 16
/* 约定值：1=Mono, 2=Stereo */
#define SOUND_CHANNEL_FORMAT 1

/* 麦克风采集参数 */
#define MIC_SAMPLE_RATE 16000
#define MIC_BITS_PER_SAMPLE 32
/* 约定值：1=Mono, 2=Stereo */
#define MIC_CHANNEL_FORMAT 1

/* DMA 缓冲配置：提高可降低欠载概率，但增加 RAM 占用 */
#define I2S_DMA_BUFFER_COUNT 6
#define I2S_DMA_BUFFER_LEN 256
