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

/* DMA 缓冲配置：保持对讲页可收听，同时控制播放链路 RAM 占用。 */
#define I2S_DMA_BUFFER_COUNT 4
#define I2S_DMA_BUFFER_LEN 192

/* 音频 actor 队列 / 栈配置 */
#define AUDIO_ACTOR_CMD_QUEUE_LEN 8
#define AUDIO_ACTOR_MSG_QUEUE_LEN 8
#define AUDIO_ACTOR_STREAM_QUEUE_LEN 16
#define AUDIO_ACTOR_TASK_STACK 5120

/* 麦克风默认不随开机初始化；需要录音功能时再启动，避免挤占 TLS 握手内存。 */
#define MIC_INIT_ON_STARTUP 0
#define MIC_ACTOR_CMD_QUEUE_LEN 6
#define MIC_ACTOR_FRAME_QUEUE_LEN 8
#define MIC_ACTOR_TASK_STACK 4096

/* 对讲 actor 任务配置。 */
#define INTERCOM_ACTOR_QUEUE_LEN 8
#define INTERCOM_ACTOR_TASK_STACK 5120

/* 网络对讲接收播放：按 20ms 帧预缓存，降低 WebSocket 抖动导致的断续。 */
#define INTERCOM_RX_PLAYBACK_PREBUFFER_FRAMES 5
#define INTERCOM_RX_PLAYBACK_PUSH_TIMEOUT_MS 5
