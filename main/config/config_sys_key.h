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

/* 自动键 DI 时长。 */
#define KEY_DEFAULT_AUTO_DI_MS 80
#define KEY_DEFAULT_AUTO_DA_RATIO "2.2"
#define KEY_DEFAULT_AUTO_GAP_RATIO "0.5"

/* 侧音。 */
#define KEY_DEFAULT_TONE_HZ 700

/* 电码原始文本最大长度，按用户看到的电码字符计数：·、-、空格各算 1 个。 */
#define KEY_CODE_MAX_LEN 256

#define KEY_GROUP_GAP_RATIO 3.0f
#define KEY_AUTO_DI_FALLBACK_RATIO 0.7f

#define KEY_ACTOR_QUEUE_LEN 8
#define KEY_ACTOR_TASK_STACK 3072
#define KEY_ACTOR_POLL_MS 10

#define CW_KEYER_QUEUE_LEN 8
#define CW_KEYER_TASK_STACK 3072
