#pragma once

/**
 * @file msg.h
 * @brief 消息结构与消息发送助手接口。
 *
 * 设计目标：
 * 1. 统一消息结构（src/type/event/payload）。
 * 2. 提供常用消息构造与发送封装，减少重复样板代码。
 * 3. 通过 type 区分 INPUT/CMD/SYS 三条语义通道。
 */

#include <string.h>
#include <stdint.h>
#include "esp_err.h"

#if defined(__has_include)
#if __has_include("freertos/FreeRTOS.h")
#include "freertos/FreeRTOS.h"
#else
typedef uint32_t TickType_t;
#endif
#else
#include "freertos/FreeRTOS.h"
#endif

typedef enum {
	MSG_SRC_ENCODER,
	MSG_SRC_KEY,
	MSG_SRC_NET,
	MSG_SRC_BT,
	MSG_SRC_LVGL,
	MSG_SRC_WIFI,
	MSG_SRC_APP_INIT,
} msg_src_t;

typedef enum {
	MSG_TYPE_INPUT,
	MSG_TYPE_CMD,
	MSG_TYPE_SYS,
} msg_type_t;

typedef enum {
	MSG_EVT_INPUT_KEY_DI,
	MSG_EVT_INPUT_KEY_DA,

	MSG_EVT_INPUT_ENCODER_CW,
	MSG_EVT_INPUT_ENCODER_CCW,
	MSG_EVT_INPUT_ENCODER_PRESS,
	MSG_EVT_INPUT_SCENE_CHANGE,

	MSG_EVT_CMD_UI_UPDATE_TEXT,
	MSG_EVT_CMD_UI_SCROLL,
	MSG_EVT_CMD_UI_NAV_STEP,
	
	MSG_EVT_CMD_AUDIO_TONE,
	MSG_EVT_CMD_AUDIO_STOP,
	MSG_EVT_CMD_AUDIO_VOLUME_STEP,

	MSG_EVT_SYS_INIT_DONE_LCD,
	MSG_EVT_SYS_INIT_DONE_LVGL,
	MSG_EVT_SYS_APP_INIT_INFO,
	MSG_EVT_SYS_INIT_DONE_ENCODER,
	MSG_EVT_SYS_INIT_DONE_RADIO,
	MSG_EVT_SYS_INIT_DONE_MIC,
	MSG_EVT_SYS_INIT_DONE_KEY,
	
	MSG_EVT_SYS_WIFI_CONNECTED,
	MSG_EVT_SYS_WIFI_DISCONNECTED,
	MSG_EVT_SYS_WIFI_SIGNAL_WEAK,
	MSG_EVT_SYS_WIFI_SIGNAL_LEVEL,
	MSG_EVT_SYS_WS_CONNECTED,
	MSG_EVT_SYS_WS_DISCONNECTED,
	MSG_EVT_SYS_WS_HEARTBEAT_LOST,

} msg_event_t;

/* Compatibility aliases for existing code paths. */
#define EVENT_KEY_DI MSG_EVT_INPUT_KEY_DI
#define EVENT_KEY_DA MSG_EVT_INPUT_KEY_DA
#define EVENT_ENCODER_CW MSG_EVT_INPUT_ENCODER_CW
#define EVENT_ENCODER_CCW MSG_EVT_INPUT_ENCODER_CCW
#define EVENT_ENCODER_PRESS MSG_EVT_INPUT_ENCODER_PRESS
#define EVENT_STATUS_CHANGE MSG_EVT_INPUT_SCENE_CHANGE

#define CMD_UI_UPDATE_TEXT MSG_EVT_CMD_UI_UPDATE_TEXT
#define CMD_UI_SCROLL MSG_EVT_CMD_UI_SCROLL
#define CMD_UI_NAV_STEP MSG_EVT_CMD_UI_NAV_STEP

#define CMD_AUDIO_TONE MSG_EVT_CMD_AUDIO_TONE
#define CMD_AUDIO_STOP MSG_EVT_CMD_AUDIO_STOP
#define CMD_AUDIO_VOLUME_STEP MSG_EVT_CMD_AUDIO_VOLUME_STEP

#define EVENT_INIT_DONE_LVGL MSG_EVT_SYS_INIT_DONE_LVGL
#define EVENT_INIT_DONE_ENCODER MSG_EVT_SYS_INIT_DONE_ENCODER
#define EVENT_INIT_DONE_RADIO MSG_EVT_SYS_INIT_DONE_RADIO
#define EVENT_INIT_DONE_MIC MSG_EVT_SYS_INIT_DONE_MIC
#define EVENT_INIT_DONE_KEY MSG_EVT_SYS_INIT_DONE_KEY


typedef struct {
	msg_src_t src;
	msg_type_t type;
	msg_event_t event;

	uint32_t timestamp;

	union {
		int value;

		struct {
			int freq;
			int duration;
		} tone;

		struct {
			char text[64];
		} text;
	} data;

} msg_t;

static inline msg_t msg_make(msg_src_t src, msg_type_t type, msg_event_t event, uint32_t timestamp)
{
	msg_t msg = {0};
	msg.src = src;
	msg.type = type;
	msg.event = event;
	msg.timestamp = timestamp;
	return msg;
}

static inline msg_t msg_make_cmd_value(msg_src_t src, msg_event_t event, int value, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, event, timestamp);
	msg.data.value = value;
	return msg;
}

static inline msg_t msg_make_cmd_tone(msg_src_t src, int freq, int duration, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, CMD_AUDIO_TONE, timestamp);
	msg.data.tone.freq = freq;
	msg.data.tone.duration = duration;
	return msg;
}

static inline msg_t msg_make_cmd_text(msg_src_t src, const char *text, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, CMD_UI_UPDATE_TEXT, timestamp);
	if(text == NULL) {
		msg.data.text.text[0] = '\0';
		return msg;
	}

	strncpy(msg.data.text.text, text, sizeof(msg.data.text.text) - 1);
	msg.data.text.text[sizeof(msg.data.text.text) - 1] = '\0';
	return msg;
}

/**
 * @brief 发送一条输入消息到 con 的 input 队列。
 *
 * 用法：
 * @code
 * msg_t m = msg_make(MSG_SRC_ENCODER, MSG_TYPE_INPUT, MSG_EVT_INPUT_ENCODER_CW, now_tick);
 * esp_err_t err = msg_send_input(&m, 0);
 * @endcode
 *
 * @param msg 待发送消息，且 msg->type 应为 MSG_TYPE_INPUT。
 * @param timeout_ticks 队列发送超时（FreeRTOS tick）。0 表示不阻塞。
 * @return ESP_OK 发送成功；
 *         ESP_ERR_INVALID_ARG 参数非法；
 *         ESP_ERR_TIMEOUT 队列满或超时。
 */
esp_err_t msg_send_input(const msg_t *msg, TickType_t timeout_ticks);

/**
 * @brief 构造并发送带 int payload 的 INPUT 消息。
 *
 * 行为：
 * 1. 自动填充 type = MSG_TYPE_INPUT；
 * 2. 自动填充 timestamp = xTaskGetTickCount()；
 * 3. data.value = value。
 *
 * 用法：
 * @code
 * (void)msg_send_input_value(MSG_SRC_KEY, MSG_EVT_INPUT_KEY_DI, 1, 0);
 * @endcode
 *
 * @param src 消息来源模块。
 * @param event 输入事件类型，建议使用 MSG_EVT_INPUT_*。
 * @param value 写入 msg.data.value 的值。
 * @param timeout_ticks 队列发送超时（tick）。
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_TIMEOUT。
 */
esp_err_t msg_send_input_value(msg_src_t src,
							   msg_event_t event,
							   int value,
							   TickType_t timeout_ticks);

/**
 * @brief 发送“场景切换”输入消息（EVENT_STATUS_CHANGE 的便捷封装）。
 *
 * 说明：
 * - 本函数保持向后兼容，内部等价于
 *   msg_send_input_value(src, EVENT_STATUS_CHANGE, scene_value, timeout_ticks)。
 * - 推荐用于需要触发 state scene 变更的场景。
 *
 * 用法：
 * @code
 * (void)msg_send_status_change(MSG_SRC_LVGL, STATE_SCENE_HOME, 0);
 * @endcode
 *
 * @param src 消息来源模块。
 * @param scene_value 目标场景值（通常是 state_scene_t 枚举值）。
 * @param timeout_ticks 队列发送超时（tick）。
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_TIMEOUT。
 */
esp_err_t msg_send_status_change(msg_src_t src,
								 int scene_value,
								 TickType_t timeout_ticks);

/**
 * @brief 发送一条系统消息到 con 的 sys 队列。
 *
 * 用法：
 * @code
 * msg_t m = msg_make(MSG_SRC_WIFI, MSG_TYPE_SYS, MSG_EVT_SYS_WIFI_CONNECTED, now_tick);
 * esp_err_t err = msg_send_sys(&m, 0);
 * @endcode
 *
 * @param msg 待发送消息，且 msg->type 应为 MSG_TYPE_SYS。
 * @param timeout_ticks 队列发送超时（tick）。
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_TIMEOUT。
 */
esp_err_t msg_send_sys(const msg_t *msg, TickType_t timeout_ticks);

/**
 * @brief 构造并发送带 int payload 的 SYS 消息。
 *
 * 行为：
 * 1. 自动填充 type = MSG_TYPE_SYS；
 * 2. 自动填充 timestamp = xTaskGetTickCount()；
 * 3. data.value = value。
 *
 * 用法：
 * @code
 * (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SIGNAL_WEAK, rssi, 0);
 * @endcode
 *
 * @param src 消息来源模块。
 * @param event 系统事件类型，建议使用 MSG_EVT_SYS_*。
 * @param value 写入 msg.data.value 的值（如强度、错误码、阶段值）。
 * @param timeout_ticks 队列发送超时（tick）。
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_TIMEOUT。
 */
esp_err_t msg_send_sys_value(msg_src_t src,
							 msg_event_t event,
							 int value,
							 TickType_t timeout_ticks);

/**
 * @brief 构造并发送带 text payload 的 SYS 消息。
 *
 * 行为：
 * 1. 自动填充 type = MSG_TYPE_SYS；
 * 2. 自动填充 timestamp = xTaskGetTickCount()；
 * 3. 将 text 拷贝到 msg.data.text.text（自动截断并补 '\0'）。
 *
 * 用法：
 * @code
 * (void)msg_send_sys_text(MSG_SRC_APP_INIT, MSG_EVT_SYS_APP_INIT_INFO, "屏幕初始化完成", 0);
 * @endcode
 *
 * @param src 消息来源模块。
 * @param event 系统事件类型。
 * @param text 文本内容（可为 NULL，等价为空字符串）。
 * @param timeout_ticks 队列发送超时（tick）。
 * @return ESP_OK / ESP_ERR_INVALID_ARG / ESP_ERR_TIMEOUT。
 */
esp_err_t msg_send_sys_text(msg_src_t src,
							msg_event_t event,
							const char *text,
							TickType_t timeout_ticks);
