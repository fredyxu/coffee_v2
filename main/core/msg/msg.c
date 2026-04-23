#include "core/msg/msg.h"

#include <stdio.h>
#include "freertos/task.h"
#include "core/con/con.h"

/* 直接转发 INPUT 消息到 con 的 input 队列。 */
esp_err_t msg_send_input(const msg_t *msg, TickType_t timeout_ticks)
{
    if(msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

	return con_post_input(msg, timeout_ticks);
}

/* 构造 INPUT+value 的常用消息，自动填充当前 tick。 */
esp_err_t msg_send_input_value(msg_src_t src,
							   msg_event_t event,
							   int value,
							   TickType_t timeout_ticks)
{
	msg_t msg = msg_make(src, MSG_TYPE_INPUT, event, (uint32_t)xTaskGetTickCount());
	msg.data.value = value;
	return msg_send_input(&msg, timeout_ticks);
}

/* 历史兼容封装：用于 scene 切换通知。 */
esp_err_t msg_send_status_change(msg_src_t src,
                                 int scene_value,
                                 TickType_t timeout_ticks)
{
    return msg_send_input_value(src, EVENT_STATUS_CHANGE, scene_value, timeout_ticks);
}

/* 直接转发 SYS 消息到 con 的 sys 队列。 */
esp_err_t msg_send_sys(const msg_t *msg, TickType_t timeout_ticks)
{
    if(msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return con_post_sys(msg, timeout_ticks);
}

/* 构造 SYS+value 的常用消息，自动填充当前 tick。 */
esp_err_t msg_send_sys_value(msg_src_t src,
                             msg_event_t event,
                             int value,
                             TickType_t timeout_ticks)
{
    msg_t msg = msg_make(src, MSG_TYPE_SYS, event, (uint32_t)xTaskGetTickCount());
    msg.data.value = value;
    return msg_send_sys(&msg, timeout_ticks);
}

/* 构造 SYS+text 的常用消息，自动填充当前 tick。 */
esp_err_t msg_send_sys_text(msg_src_t src,
                            msg_event_t event,
                            const char *text,
                            TickType_t timeout_ticks)
{
    msg_t msg = msg_make(src, MSG_TYPE_SYS, event, (uint32_t)xTaskGetTickCount());
    (void)snprintf(msg.data.text.text, sizeof(msg.data.text.text), "%s", text ? text : "");
    return msg_send_sys(&msg, timeout_ticks);
}
