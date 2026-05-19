#include "core/msg/msg.h"

#include <stdio.h>
#include "freertos/task.h"
#include "core/msg/msg_sub.h"

/*
 * 发布 INPUT 消息。
 *
 * 本函数不直接选择 topic，而是交给 msg_publish_auto() 根据 msg->event
 * 自动映射。这样输入模块只需要关心具体事件，不需要知道订阅路由细节。
 */
esp_err_t msg_send_input(const msg_t *msg, TickType_t timeout_ticks)
{
    if(msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

	return msg_publish_auto(msg, timeout_ticks);
}

/*
 * 构造并发送带整数 payload 的 INPUT 消息。
 *
 * 常用于 key、encoder 等输入模块发送“某个输入事件 + 一个整数值”。
 */
esp_err_t msg_send_input_value(msg_src_t src,
							   msg_event_t event,
							   int value,
							   TickType_t timeout_ticks)
{
	msg_t msg = msg_make(src, MSG_TYPE_INPUT, event, (uint32_t)xTaskGetTickCount());
	msg.data.value = value;
	return msg_send_input(&msg, timeout_ticks);
}

/*
 * 发布 SYS 消息。
 *
 * 与 msg_send_input() 一样，topic 由 msg_publish_auto() 根据 event 推导。
 */
esp_err_t msg_send_sys(const msg_t *msg, TickType_t timeout_ticks)
{
    if(msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return msg_publish_auto(msg, timeout_ticks);
}

/*
 * 构造并发送带整数 payload 的 SYS 消息。
 *
 * 常用于 WiFi 信号等级、错误码、阶段值等小型状态数据。
 */
esp_err_t msg_send_sys_value(msg_src_t src,
                             msg_event_t event,
                             int value,
                             TickType_t timeout_ticks)
{
    msg_t msg = msg_make(src, MSG_TYPE_SYS, event, (uint32_t)xTaskGetTickCount());
    msg.data.value = value;
    return msg_send_sys(&msg, timeout_ticks);
}

/*
 * 构造并发送带文本 payload 的 SYS 消息。
 *
 * text 会被拷贝进 msg.data.text，超出 63 字节的部分会被 snprintf 截断。
 */
esp_err_t msg_send_sys_text(msg_src_t src,
                            msg_event_t event,
                            const char *text,
                            TickType_t timeout_ticks)
{
    msg_t msg = msg_make(src, MSG_TYPE_SYS, event, (uint32_t)xTaskGetTickCount());
    (void)snprintf(msg.data.text, sizeof(msg.data.text), "%s", text ? text : "");
    return msg_send_sys(&msg, timeout_ticks);
}
