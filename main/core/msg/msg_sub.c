#include "core/msg/msg_sub.h"

#include <stdbool.h>
#include <stddef.h>

#include "freertos/semphr.h"

/* 当前订阅表最大支持 16 个 actor queue。一个 queue 可订阅多个 topic。 */
#define MSG_SUB_MAX 16

/*
 * 当前 topic_mask 使用 uint32_t 保存，因此最多支持 32 个 topic。
 * 如果未来 MSG_TOPIC_COUNT 超过 32，应把 topic_mask 扩展为 uint32_t 数组。
 */
#define MSG_SUB_TOPIC_BITS 32u

/*
 * 一个订阅槽表示“一个 actor inbox queue 订阅了一组 topic”。
 *
 * used:
 *   该槽是否正在使用。
 * generation:
 *   槽复用计数。handle 中也保存 generation，用于避免旧 handle 误删新订阅。
 * queue:
 *   actor 自己创建的 inbox queue。消息发布时会把 msg_t 投递到这里。
 * topic_mask:
 *   topic 位图。第 N 位为 1 表示订阅了 msg_topic_t 值为 N 的 topic。
 */
typedef struct {
    bool used;
    uint8_t generation;
    QueueHandle_t queue;
    uint32_t topic_mask;
} msg_sub_slot_t;

static msg_sub_slot_t s_subs[MSG_SUB_MAX] = {0};
static SemaphoreHandle_t s_sub_lock;
static StaticSemaphore_t s_sub_lock_buf;
static portMUX_TYPE s_sub_lock_init_mux = portMUX_INITIALIZER_UNLOCKED;

/* topic 必须落在枚举范围内，也必须能被 uint32_t topic_mask 表示。 */
static bool msg_topic_valid(msg_topic_t topic)
{
    return topic >= 0 && topic < MSG_TOPIC_COUNT && topic < (msg_topic_t)MSG_SUB_TOPIC_BITS;
}

/*
 * 初始化订阅表锁。
 *
 * 使用静态 mutex，避免运行期 malloc。外层先快速判断，真正创建时进入临界区，
 * 防止两个任务同时第一次进入消息系统时重复初始化锁。
 */
static esp_err_t msg_sub_lock_init(void)
{
    if(s_sub_lock != NULL) {
        return ESP_OK;
    }

    portENTER_CRITICAL(&s_sub_lock_init_mux);
    if(s_sub_lock == NULL) {
        s_sub_lock = xSemaphoreCreateMutexStatic(&s_sub_lock_buf);
    }
    portEXIT_CRITICAL(&s_sub_lock_init_mux);

    return (s_sub_lock != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

/*
 * 生成订阅句柄。
 *
 * 低 8 位保存 index + 1，因此 0 可以保留为 MSG_SUB_HANDLE_INVALID。
 * 高位保存 generation。取消订阅时会同时校验 index 和 generation。
 */
static msg_sub_handle_t msg_sub_make_handle(size_t index, uint8_t generation)
{
    return (((msg_sub_handle_t)generation) << 8) | ((msg_sub_handle_t)index + 1u);
}

/* 把订阅句柄还原为订阅表下标和 generation。 */
static bool msg_sub_decode_handle(msg_sub_handle_t handle, size_t *out_index, uint8_t *out_generation)
{
    if(handle == MSG_SUB_HANDLE_INVALID || out_index == NULL || out_generation == NULL) {
        return false;
    }

    size_t index = (size_t)((handle & 0xFFu) - 1u);
    if(index >= MSG_SUB_MAX) {
        return false;
    }

    *out_index = index;
    *out_generation = (uint8_t)((handle >> 8) & 0xFFu);
    return true;
}

/* 将 topic 转为 topic_mask 中的一位。调用前必须保证 topic 合法。 */
static uint32_t msg_topic_bit(msg_topic_t topic)
{
    return 1u << (uint32_t)topic;
}

/*
 * 将 topic 数组转换为 bitmask。
 *
 * 重复 topic 会自然去重，因为同一 bit 多次置 1 结果不变。
 */
static esp_err_t msg_topics_to_mask(const msg_topic_t *topics, size_t topic_count, uint32_t *out_mask)
{
    if(topics == NULL || topic_count == 0 || out_mask == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t mask = 0;
    for(size_t i = 0; i < topic_count; i++) {
        if(!msg_topic_valid(topics[i])) {
            return ESP_ERR_INVALID_ARG;
        }
        mask |= msg_topic_bit(topics[i]);
    }

    if(mask == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_mask = mask;
    return ESP_OK;
}

/*
 * 根据具体 event 推导 topic。
 *
 * 发送方一般调用 msg_send_input()/msg_send_sys()，最终会进入 msg_publish_auto()。
 * 自动发布依赖这里的映射关系。新增 event 后，如果需要自动路由，必须在这里
 * 把 event 映射到合适的 topic。
 */
static esp_err_t msg_topic_from_msg(const msg_t *msg, msg_topic_t *out_topic)
{
    if(msg == NULL || out_topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch(msg->event) {
        case MSG_EVT_INPUT_ENCODER_CW:
        case MSG_EVT_INPUT_ENCODER_CCW:
        case MSG_EVT_INPUT_ENCODER_PRESS:
        case MSG_EVT_INPUT_ENCODER_LONG_PRESS:
            *out_topic = MSG_TOPIC_ENCODER_INPUT;
            return ESP_OK;

        case MSG_EVT_INPUT_KEY_DI:
        case MSG_EVT_INPUT_KEY_DA:
            *out_topic = MSG_TOPIC_KEY_INPUT;
            return ESP_OK;

        case MSG_EVT_CMD_WIFI_START:
        case MSG_EVT_CMD_WIFI_STOP:
        case MSG_EVT_CMD_WIFI_SCAN:
        case MSG_EVT_CMD_WIFI_CONNECT:
        case MSG_EVT_CMD_WIFI_DISCONNECT:
        case MSG_EVT_CMD_WIFI_SET_CREDENTIALS:
            *out_topic = MSG_TOPIC_WIFI_CMD;
            return ESP_OK;

        case MSG_EVT_SYS_WIFI_CONNECTED:
        case MSG_EVT_SYS_WIFI_DISCONNECTED:
        case MSG_EVT_SYS_WIFI_SIGNAL_WEAK:
        case MSG_EVT_SYS_WIFI_SIGNAL_LEVEL:
        case MSG_EVT_SYS_WIFI_SCAN_STARTED:
        case MSG_EVT_SYS_WIFI_SCAN_AP_FOUND:
        case MSG_EVT_SYS_WIFI_SCAN_DONE:
        case MSG_EVT_SYS_WIFI_SCAN_FAILED:
            *out_topic = MSG_TOPIC_WIFI_EVENT;
            return ESP_OK;

        case MSG_EVT_SYS_WS_CONNECTED:
        case MSG_EVT_SYS_WS_DISCONNECTED:
        case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
            *out_topic = MSG_TOPIC_WEBSOCKET_EVENT;
            return ESP_OK;

        default:
            break;
    }

    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t msg_actor_queue_create(QueueHandle_t *out_queue)
{
    return msg_actor_queue_create_with_len(MSG_ACTOR_QUEUE_DEFAULT_LEN, out_queue);
}

/* 创建 actor inbox queue。队列元素类型固定为 msg_t，长度由 actor 自己决定。 */
esp_err_t msg_actor_queue_create_with_len(size_t len, QueueHandle_t *out_queue)
{
    if(len == 0 || out_queue == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    QueueHandle_t queue = xQueueCreate((UBaseType_t)len, sizeof(msg_t));
    if(queue == NULL) {
        return ESP_ERR_NO_MEM;
    }

    *out_queue = queue;
    return ESP_OK;
}

esp_err_t msg_sub(QueueHandle_t queue,
                  const msg_topic_t *topics,
                  size_t topic_count,
                  msg_sub_handle_t *out_handle)
{
    if(queue == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t topic_mask = 0;
    esp_err_t err = msg_topics_to_mask(topics, topic_count, &topic_mask);
    if(err != ESP_OK) {
        return err;
    }

    err = msg_sub_lock_init();
    if(err != ESP_OK) {
        return err;
    }

    (void)xSemaphoreTake(s_sub_lock, portMAX_DELAY);

    /*
     * 同一个 actor queue 重复订阅时合并 topic。
     *
     * 这样 actor 初始化流程即使被重复调用，也不会为同一个 queue 占用多个槽；
     * 后续取消订阅仍然只需要保存一个 handle。
     */
    for(size_t i = 0; i < MSG_SUB_MAX; i++) {
        msg_sub_slot_t *slot = &s_subs[i];
        if(slot->used && slot->queue == queue) {
            slot->topic_mask |= topic_mask;
            *out_handle = msg_sub_make_handle(i, slot->generation);

            (void)xSemaphoreGive(s_sub_lock);
            return ESP_OK;
        }
    }

    /* 没有已存在的 queue 时，分配一个新的订阅槽。 */
    for(size_t i = 0; i < MSG_SUB_MAX; i++) {
        msg_sub_slot_t *slot = &s_subs[i];
        if(slot->used) {
            continue;
        }

        slot->used = true;
        slot->generation++;
        if(slot->generation == 0) {
            slot->generation = 1;
        }
        slot->queue = queue;
        slot->topic_mask = topic_mask;
        *out_handle = msg_sub_make_handle(i, slot->generation);

        (void)xSemaphoreGive(s_sub_lock);
        return ESP_OK;
    }

    (void)xSemaphoreGive(s_sub_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t msg_unsub(msg_sub_handle_t handle, const msg_topic_t *topics, size_t topic_count)
{
    size_t index = 0;
    uint8_t generation = 0;
    if(!msg_sub_decode_handle(handle, &index, &generation)) {
        return ESP_ERR_INVALID_ARG;
    }

    /*
     * topics == NULL 且 topic_count == 0 表示取消全部订阅。
     * 其他组合都必须提供合法的 topic 数组。
     */
    bool unsub_all = (topics == NULL && topic_count == 0);
    uint32_t topic_mask = 0;
    if(!unsub_all) {
        esp_err_t err = msg_topics_to_mask(topics, topic_count, &topic_mask);
        if(err != ESP_OK) {
            return err;
        }
    }

    esp_err_t err = msg_sub_lock_init();
    if(err != ESP_OK) {
        return err;
    }

    (void)xSemaphoreTake(s_sub_lock, portMAX_DELAY);

    msg_sub_slot_t *slot = &s_subs[index];
    if(!slot->used || slot->generation != generation) {
        (void)xSemaphoreGive(s_sub_lock);
        return ESP_ERR_NOT_FOUND;
    }

    /*
     * 取消全部时直接释放槽。
     * 取消部分 topic 时只清除对应 bit；如果清完后没有 topic，释放整个槽。
     */
    if(unsub_all) {
        slot->used = false;
        slot->queue = NULL;
        slot->topic_mask = 0;
    } else {
        slot->topic_mask &= ~topic_mask;
        if(slot->topic_mask == 0) {
            slot->used = false;
            slot->queue = NULL;
        }
    }

    (void)xSemaphoreGive(s_sub_lock);
    return ESP_OK;
}

esp_err_t msg_publish(msg_topic_t topic, const msg_t *msg, TickType_t timeout_ticks)
{
    if(!msg_topic_valid(topic) || msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = msg_sub_lock_init();
    if(err != ESP_OK) {
        return err;
    }

    QueueHandle_t targets[MSG_SUB_MAX];
    size_t target_count = 0;

    /*
     * 只在锁内扫描订阅表并复制目标 queue。
     * 不在持锁状态下调用 xQueueSend()，避免某个满队列阻塞整个订阅系统。
     */
    (void)xSemaphoreTake(s_sub_lock, portMAX_DELAY);
    for(size_t i = 0; i < MSG_SUB_MAX; i++) {
        const msg_sub_slot_t *slot = &s_subs[i];
        if(!slot->used || slot->queue == NULL || (slot->topic_mask & msg_topic_bit(topic)) == 0) {
            continue;
        }

        targets[target_count++] = slot->queue;
    }
    (void)xSemaphoreGive(s_sub_lock);

    /*
     * msg_t 会被 FreeRTOS queue 拷贝进目标队列。
     * 如果某个订阅者队列满了，继续投递其他订阅者，最后统一返回超时错误。
     */
    esp_err_t result = ESP_OK;
    for(size_t i = 0; i < target_count; i++) {
        if(xQueueSend(targets[i], msg, timeout_ticks) != pdTRUE) {
            result = ESP_ERR_TIMEOUT;
        }
    }

    return result;
}

esp_err_t msg_publish_auto(const msg_t *msg, TickType_t timeout_ticks)
{
    msg_topic_t topic = MSG_TOPIC_COUNT;
    esp_err_t err = msg_topic_from_msg(msg, &topic);
    if(err != ESP_OK) {
        return err;
    }

    return msg_publish(topic, msg, timeout_ticks);
}
