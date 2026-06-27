#pragma once

/**
 * @file msg.h
 * @brief 系统内部消息结构与消息发送助手接口。
 *
 * 设计目标：
 * 1. 统一消息结构（src/type/event/payload）。
 * 2. 让各模块通过小型 msg_t 交换控制消息、输入事件和状态事件。
 * 3. 大数据流不要直接放进 msg_t，例如音频流、WebSocket 大二进制数据应走
 *    ringbuffer/streambuffer，msg_t 只用于发送“数据可读/可写”等通知。
 * 4. 具体发布路由在 msg_sub 模块中完成，本文件只定义消息本身和发送助手。
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

/*
 * 消息来源。
 *
 * src 表示“谁发出的消息”，主要用于接收方判断来源、日志打印和调试。
 * 它不参与订阅路由；订阅路由由 msg_event_t 映射到 msg_topic_t 完成。
 */
typedef enum {
	MSG_SRC_ENCODER,
	MSG_SRC_KEY,
	MSG_SRC_NET,
	MSG_SRC_BT,
	MSG_SRC_LVGL,
	MSG_SRC_WIFI,
	MSG_SRC_WS,
	MSG_SRC_CW_KEYER,
	MSG_SRC_APP_INIT,
	MSG_SRC_UI,
	MSG_SRC_INTERCOM,
} msg_src_t;

/*
 * 消息类型。
 *
 * INPUT:
 *   用户输入类事件，例如编码器旋转、按键。
 * CMD:
 *   命令类事件，通常表示要求某个模块执行动作。
 * SYS:
 *   系统状态类事件，例如初始化完成、网络连接状态。
 */
typedef enum {
	MSG_TYPE_INPUT,
	MSG_TYPE_CMD,
	MSG_TYPE_SYS,
} msg_type_t;

/*
 * 具体消息事件。
 *
 * 订阅系统不会直接按 event 订阅，而是把 event 映射到 topic。
 * actor 收到 msg_t 后，再根据 event 做更细的业务分发。
 *
 * 新增 event 时要注意：
 * - 如果希望 msg_publish_auto() 能自动发布它，需要在 msg_sub.c 中补充映射。
 * - 如果 event 需要携带数据，要明确使用 msg_t.data 中哪个字段。
 */
typedef enum {
	MSG_EVT_INPUT_KEY_DI,
	MSG_EVT_INPUT_KEY_DA,
	MSG_EVT_INPUT_KEY_A_DOWN,
	MSG_EVT_INPUT_KEY_A_UP,
	MSG_EVT_INPUT_KEY_B_DOWN,
	MSG_EVT_INPUT_KEY_B_UP,
	MSG_EVT_INPUT_KEY_CODE_CHANGED,
	MSG_EVT_INPUT_CW_RAW_SYMBOL,
	MSG_EVT_INPUT_CW_DISPLAY_SYMBOL,
	MSG_EVT_INPUT_CW_CLEARED,
	MSG_EVT_INPUT_CW_CONTENT_STATE,
	MSG_EVT_INPUT_CW_TEXT_CHANGED,

	MSG_EVT_INPUT_ENCODER_CW,
	MSG_EVT_INPUT_ENCODER_CCW,
	MSG_EVT_INPUT_ENCODER_PRESS,
	MSG_EVT_INPUT_ENCODER_LONG_PRESS,
	MSG_EVT_INPUT_ENCODER_RELEASE,
	MSG_EVT_INPUT_SCENE_CHANGE,

	MSG_EVT_CMD_UI_UPDATE_TEXT,
	MSG_EVT_CMD_UI_SCROLL,
	MSG_EVT_CMD_UI_NAV_STEP,
	MSG_EVT_CMD_UI_PAGE_GO_BACK,
	
	MSG_EVT_CMD_AUDIO_TONE,
	MSG_EVT_CMD_AUDIO_TONE_ON,
	MSG_EVT_CMD_AUDIO_TONE_OFF,
	MSG_EVT_CMD_AUDIO_STOP,
	MSG_EVT_CMD_AUDIO_VOLUME_STEP,

	MSG_EVT_CMD_CW_KEYER_DI_START,
	MSG_EVT_CMD_CW_KEYER_DI_STOP,
	MSG_EVT_CMD_CW_KEYER_DA_START,
	MSG_EVT_CMD_CW_KEYER_DA_STOP,
	MSG_EVT_CMD_CW_SEND,
	MSG_EVT_CMD_CW_DELETE_LAST_GROUP,
	MSG_EVT_CMD_CW_RESTORE_LAST_GROUP,
	MSG_EVT_CMD_CW_CLEAR_DELETE_HISTORY,
	MSG_EVT_CMD_CW_CLEAR,
	MSG_EVT_CMD_KEY_REFRESH_MODE,

	MSG_EVT_CMD_WIFI_START,
	MSG_EVT_CMD_WIFI_STOP,
	MSG_EVT_CMD_WIFI_SCAN,
	MSG_EVT_CMD_WIFI_CONNECT,
	MSG_EVT_CMD_WIFI_DISCONNECT,
	MSG_EVT_CMD_WIFI_SET_ENABLE,
	MSG_EVT_CMD_WIFI_SET_CREDENTIALS,
	MSG_EVT_CMD_WS_SET_ENABLE,
	MSG_EVT_CMD_WS_RECONNECT,
	MSG_EVT_CMD_WS_SEND_CW,
	MSG_EVT_CMD_WS_ROOM_LIST_REQ,
	MSG_EVT_CMD_WS_ROOM_USERS_REQ,
	MSG_EVT_CMD_WS_INTERCOM_ROOM_JOIN,
	MSG_EVT_CMD_WS_INTERCOM_ROOM_LEAVE,
	MSG_EVT_CMD_WS_INTERCOM_TALK_START,
	MSG_EVT_CMD_WS_INTERCOM_TALK_STOP,
	MSG_EVT_CMD_INTERCOM_TALK_START_REQ,
	MSG_EVT_CMD_INTERCOM_TALK_STOP,

	MSG_EVT_SYS_INIT_DONE_LCD,
	MSG_EVT_SYS_INIT_DONE_LVGL,
	MSG_EVT_SYS_APP_INIT_INFO,
	MSG_EVT_SYS_APP_INIT_DONE,
	MSG_EVT_SYS_INIT_DONE_ENCODER,
	MSG_EVT_SYS_INIT_DONE_RADIO,
	MSG_EVT_SYS_INIT_DONE_MIC,
	MSG_EVT_SYS_INIT_DONE_KEY,
	
	MSG_EVT_SYS_WIFI_CONNECTED,
	MSG_EVT_SYS_WIFI_STOPPING,
	MSG_EVT_SYS_WIFI_DISCONNECTED,
	MSG_EVT_SYS_WIFI_SIGNAL_LEVEL,
	MSG_EVT_SYS_WIFI_SCAN_STARTED,
	MSG_EVT_SYS_WIFI_SCAN_AP_FOUND,
	MSG_EVT_SYS_WIFI_SCAN_DONE,
	MSG_EVT_SYS_WIFI_SCAN_FAILED,
	MSG_EVT_SYS_WS_CONNECTED,
	MSG_EVT_SYS_WS_DISCONNECTED,
	MSG_EVT_SYS_WS_HEARTBEAT_LOST,
	MSG_EVT_SYS_WS_CW_RECEIVED,
	MSG_EVT_SYS_WS_ROOM_LIST_UPDATED,
	MSG_EVT_SYS_WS_ROOM_USERS_UPDATED,
	MSG_EVT_SYS_INTERCOM_TALK_START_ACK,
	MSG_EVT_SYS_INTERCOM_SPEAKER_CHANGED,

} msg_event_t;

typedef enum {
	MSG_INTERCOM_TALK_ACK_BUSY = 0,
	MSG_INTERCOM_TALK_ACK_OK = 1,
	MSG_INTERCOM_TALK_ACK_OFFLINE = -1,
	MSG_INTERCOM_TALK_ACK_TIMEOUT = -2,
	MSG_INTERCOM_TALK_ACK_LOCAL_ERROR = -3,
} msg_intercom_talk_ack_t;

/*
 * 系统消息。
 *
 * msg_t 是跨模块投递的固定大小消息。FreeRTOS queue 会按值拷贝它，因此这里
 * 应保持结构体较小、字段语义清晰。
 *
 * src:
 *   消息来源模块。
 * type:
 *   消息大类，方便接收方先做粗分。
 * event:
 *   具体事件，接收方最终通常按它执行处理逻辑。
 * timestamp:
 *   创建消息时的 tick，便于判断事件发生时间或调试延迟。
 * data:
 *   小型 payload 联合体。每条 event 应约定使用其中一种字段。
 */
typedef struct {
	msg_src_t src;
	msg_type_t type;
	msg_event_t event;

	uint32_t timestamp;

	union {
		int value;
		char text[64];

		struct {
			char ssid[33];
			int rssi;
			uint8_t authmode;
			uint8_t channel;
		} wifi_ap;

		struct {
			char ssid[33];
			char password[65];
		} wifi_credentials;

		struct {
			int freq;
			int duration;
		} tone;

	} data;

} msg_t;

/* 构造基础消息，只填充 src/type/event/timestamp，payload 默认为 0。 */
static inline msg_t msg_make(msg_src_t src, msg_type_t type, msg_event_t event, uint32_t timestamp)
{
	msg_t msg = {0};
	msg.src = src;
	msg.type = type;
	msg.event = event;
	msg.timestamp = timestamp;
	return msg;
}

/* 构造带整数 payload 的 CMD 消息。 */
static inline msg_t msg_make_cmd_value(msg_src_t src, msg_event_t event, int value, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, event, timestamp);
	msg.data.value = value;
	return msg;
}

/* 构造音频 tone 命令。freq 为频率，duration 为持续时间，单位由音频模块定义。 */
static inline msg_t msg_make_cmd_tone(msg_src_t src, int freq, int duration, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, MSG_EVT_CMD_AUDIO_TONE, timestamp);
	msg.data.tone.freq = freq;
	msg.data.tone.duration = duration;
	return msg;
}

/* 构造 UI 文本更新命令。文本会被拷贝并保证以 '\0' 结尾。 */
static inline msg_t msg_make_cmd_text(msg_src_t src, const char *text, uint32_t timestamp)
{
	msg_t msg = msg_make(src, MSG_TYPE_CMD, MSG_EVT_CMD_UI_UPDATE_TEXT, timestamp);
	if(text == NULL) {
		msg.data.text[0] = '\0';
		return msg;
	}

	strncpy(msg.data.text, text, sizeof(msg.data.text) - 1);
	msg.data.text[sizeof(msg.data.text) - 1] = '\0';
	return msg;
}

/**
 * @brief 发布一条 INPUT 消息。
 *
 * 函数内部会根据 msg->event 自动选择 topic，并投递给订阅了该 topic 的 actor。
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
 *         ESP_ERR_NOT_SUPPORTED event 没有配置 topic 映射；
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

esp_err_t msg_send_cmd(const msg_t *msg, TickType_t timeout_ticks);

esp_err_t msg_send_cmd_value(msg_src_t src,
							 msg_event_t event,
							 int value,
							 TickType_t timeout_ticks);

/**
 * @brief 发布一条 SYS 消息。
 *
 * 函数内部会根据 msg->event 自动选择 topic，并投递给订阅了该 topic 的 actor。
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
 * (void)msg_send_sys_value(MSG_SRC_WIFI, MSG_EVT_SYS_WIFI_SIGNAL_LEVEL, level, 0);
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
 * 3. 将 text 拷贝到 msg.data.text（自动截断并补 '\0'）。
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
