#include "core/msg/msg_sub.h"

#include <stdbool.h>
#include <stddef.h>

#include "freertos/semphr.h"

#define MSG_SUB_MAX 16

typedef struct {
    bool used;
    uint8_t generation;
    msg_topic_t topic;
    QueueHandle_t queue;
} msg_sub_slot_t;

static msg_sub_slot_t s_subs[MSG_SUB_MAX] = {0};
static SemaphoreHandle_t s_sub_lock;

static bool msg_topic_valid(msg_topic_t topic)
{
    return topic >= 0 && topic < MSG_TOPIC_COUNT;
}

static esp_err_t msg_sub_lock_init(void)
{
    if(s_sub_lock != NULL) {
        return ESP_OK;
    }

    s_sub_lock = xSemaphoreCreateMutex();
    return (s_sub_lock != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

static msg_sub_handle_t msg_sub_make_handle(size_t index, uint8_t generation)
{
    return (((msg_sub_handle_t)generation) << 8) | ((msg_sub_handle_t)index + 1u);
}

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

static esp_err_t msg_topic_from_msg(const msg_t *msg, msg_topic_t *out_topic)
{
    if(msg == NULL || out_topic == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    switch(msg->event) {
        case MSG_EVT_INPUT_ENCODER_CW:
        case MSG_EVT_INPUT_ENCODER_CCW:
        case MSG_EVT_INPUT_ENCODER_PRESS:
            *out_topic = MSG_TOPIC_ENCODER_INPUT;
            return ESP_OK;

        case MSG_EVT_INPUT_KEY_DI:
        case MSG_EVT_INPUT_KEY_DA:
            *out_topic = MSG_TOPIC_KEY_INPUT;
            return ESP_OK;

        case MSG_EVT_SYS_WIFI_CONNECTED:
        case MSG_EVT_SYS_WIFI_DISCONNECTED:
        case MSG_EVT_SYS_WIFI_SIGNAL_WEAK:
        case MSG_EVT_SYS_WIFI_SIGNAL_LEVEL:
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

esp_err_t msg_sub_queue(msg_topic_t topic, QueueHandle_t queue, msg_sub_handle_t *out_handle)
{
    if(!msg_topic_valid(topic) || queue == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = msg_sub_lock_init();
    if(err != ESP_OK) {
        return err;
    }

    (void)xSemaphoreTake(s_sub_lock, portMAX_DELAY);

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
        slot->topic = topic;
        slot->queue = queue;
        *out_handle = msg_sub_make_handle(i, slot->generation);

        (void)xSemaphoreGive(s_sub_lock);
        return ESP_OK;
    }

    (void)xSemaphoreGive(s_sub_lock);
    return ESP_ERR_NO_MEM;
}

esp_err_t msg_unsub(msg_sub_handle_t handle)
{
    size_t index = 0;
    uint8_t generation = 0;
    if(!msg_sub_decode_handle(handle, &index, &generation)) {
        return ESP_ERR_INVALID_ARG;
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

    slot->used = false;
    slot->queue = NULL;

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

    (void)xSemaphoreTake(s_sub_lock, portMAX_DELAY);
    for(size_t i = 0; i < MSG_SUB_MAX; i++) {
        const msg_sub_slot_t *slot = &s_subs[i];
        if(!slot->used || slot->queue == NULL || slot->topic != topic) {
            continue;
        }

        targets[target_count++] = slot->queue;
    }
    (void)xSemaphoreGive(s_sub_lock);

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
