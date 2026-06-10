#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 无效订阅句柄。actor 初始化时可用它作为默认值，取消订阅后也建议恢复为该值。 */
#define MSG_SUB_HANDLE_INVALID 0u

/* 订阅句柄。内部编码了订阅槽下标和 generation，调用方只需要保存和传回。 */
typedef uint32_t msg_sub_handle_t;

/* actor 未指定队列长度时使用的默认 inbox 长度。队列元素类型固定为 msg_t。 */
#define MSG_ACTOR_QUEUE_DEFAULT_LEN 16u

/*
 * 消息主题。
 *
 * topic 是订阅路由的粒度，event 是具体事件。
 * 一个 topic 下可以包含多个 event，例如 MSG_TOPIC_ENCODER_INPUT 下包含
 * CW/CCW/PRESS 三类编码器事件。
 *
 * 订阅时按 topic 订阅；actor 收到 msg_t 后再按 msg.event 做细分处理。
 */
typedef enum {
    MSG_TOPIC_ENCODER_INPUT = 0,
    MSG_TOPIC_KEY_INPUT,
    MSG_TOPIC_SETTINGS_STORE,
    MSG_TOPIC_WIFI_CMD,
    MSG_TOPIC_WIFI_EVENT,
    MSG_TOPIC_WEBSOCKET_CMD,
    MSG_TOPIC_WEBSOCKET_EVENT,
    MSG_TOPIC_MIC_INPUT,
    MSG_TOPIC_COUNT,
} msg_topic_t;

/*
 * 创建默认长度的 actor inbox 队列。
 *
 * 适合对队列容量没有特殊要求的 actor。队列元素固定为 msg_t，因此 actor
 * 后续应使用 xQueueReceive(queue, &msg, ...) 读取消息。
 */
esp_err_t msg_actor_queue_create(QueueHandle_t *out_queue);

/*
 * 创建指定长度的 actor inbox 队列。
 *
 * len 表示队列最多缓存多少条 msg_t。高频输入较多的 actor 可以设置更大的值。
 */
esp_err_t msg_actor_queue_create_with_len(size_t len, QueueHandle_t *out_queue);

/*
 * 订阅一个或多个 topic。
 *
 * 设计约定：
 * - 一个 actor 通常只创建一个 inbox queue。
 * - 同一个 queue 可以订阅多个 topic。
 * - 同一个 queue 重复调用 msg_sub() 时不会新增订阅槽，而是合并 topic。
 * - out_handle 返回该 queue 对应的订阅句柄，后续 msg_unsub() 使用它取消订阅。
 *
 * topics 可以只包含一个 topic，也可以包含多个 topic。
 */
esp_err_t msg_sub(QueueHandle_t queue,
                  const msg_topic_t *topics,
                  size_t topic_count,
                  msg_sub_handle_t *out_handle);

/*
 * 取消订阅。
 *
 * 两种用法：
 * - msg_unsub(handle, topics, count)：只取消指定 topic。
 * - msg_unsub(handle, NULL, 0)：取消该 handle 上的全部 topic，并释放订阅槽。
 *
 * 如果取消部分 topic 后该订阅槽已经没有任何 topic，内部会自动释放该槽。
 */
esp_err_t msg_unsub(msg_sub_handle_t handle, const msg_topic_t *topics, size_t topic_count);

/*
 * 向指定 topic 发布消息。
 *
 * 发布会把 msg_t 拷贝到所有订阅了该 topic 的 actor inbox queue。
 * 如果某些 queue 满了，函数仍会继续尝试投递其他 queue，最后返回 ESP_ERR_TIMEOUT。
 */
esp_err_t msg_publish(msg_topic_t topic, const msg_t *msg, TickType_t timeout_ticks);

/*
 * 根据 msg->event 自动推导 topic 并发布。
 *
 * 适合发送方只知道具体 event、不想手动选择 topic 的场景。
 * 新增 event 后，如果希望自动发布，需要在 msg_sub.c 的映射表里补充对应 topic。
 */
esp_err_t msg_publish_auto(const msg_t *msg, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
