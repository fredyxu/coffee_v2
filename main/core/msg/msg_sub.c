#include "core/msg/msg_sub.h"

#include <stdbool.h>
#include <stddef.h>

#include "freertos/semphr.h"

#define MSG_SUB_MAX 16

typedef struct {
    bool used;
    uint8_t generation;
    msg_type_t type;
    msg_event_t event;
    QueueHandle_t queue;
} msg_sub_slot_t;

static msg_sub_slot_t s_subs[MSG_SUB_MAX] = {0};
static SemaphoreHandle_t s_sub_lock;

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

esp_err_t msg_sub_queue(msg_type_t type,
                        msg_event_t event,
                        QueueHandle_t queue,
                        msg_sub_handle_t *out_handle)
{
    if(queue == NULL || out_handle == NULL) {
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
        slot->type = type;
        slot->event = event;
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

esp_err_t msg_pub_subs(const msg_t *msg, TickType_t timeout_ticks)
{
    if(msg == NULL) {
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
        if(!slot->used || slot->queue == NULL) {
            continue;
        }
        if(slot->type != msg->type || slot->event != msg->event) {
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
