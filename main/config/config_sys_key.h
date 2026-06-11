#pragma once

/* ============================================================
 * 电键系统配置
 * ============================================================ */

#define KEY_DEFAULT_ENABLE 1

/* 0=手动键，1=自动键。 */
#define KEY_MODE_MANUAL 0
#define KEY_MODE_AUTO 1
#define KEY_DEFAULT_MODE KEY_MODE_MANUAL

/* 按下接地。 */
#define KEY_DEFAULT_ACTIVE_LEVEL 0
#define KEY_DEFAULT_SWAP_AB 0
#define KEY_DEFAULT_DEBOUNCE_MS 10

/* 手动键 DI 判断阈值。超过该时长认为是 DA。 */
#define KEY_DEFAULT_MANUAL_DI_MS 120
#define KEY_DEFAULT_MANUAL_ADAPTIVE_ENABLE 0

/* 自动键 DI 时长。0 表示使用 manual_di_ms * 0.7。 */
#define KEY_DEFAULT_AUTO_DI_MS 0
#define KEY_DEFAULT_AUTO_DA_RATIO "2.2"

/* 侧音。 */
#define KEY_DEFAULT_TONE_HZ 700

/* 电码文本缓冲区。msg_t.data.text 当前为 64 字节。 */
#define KEY_CODE_MAX_LEN 64

#define KEY_GROUP_GAP_RATIO 1.2f
#define KEY_AUTO_DI_FALLBACK_RATIO 0.7f

#define KEY_ACTOR_QUEUE_LEN 8
#define KEY_ACTOR_TASK_STACK 3072
#define KEY_ACTOR_POLL_MS 10

#define CW_KEYER_QUEUE_LEN 8
#define CW_KEYER_TASK_STACK 3072
