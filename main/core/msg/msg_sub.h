#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSG_SUB_HANDLE_INVALID 0u

typedef uint32_t msg_sub_handle_t;

typedef enum {
    MSG_TOPIC_ENCODER_INPUT = 0,
    MSG_TOPIC_KEY_INPUT,
    MSG_TOPIC_SETTINGS_STORE,
    MSG_TOPIC_WIFI_EVENT,
    MSG_TOPIC_WEBSOCKET_EVENT,
    MSG_TOPIC_MIC_INPUT,
    MSG_TOPIC_COUNT,
} msg_topic_t;

esp_err_t msg_sub_queue(msg_topic_t topic, QueueHandle_t queue, msg_sub_handle_t *out_handle);

esp_err_t msg_unsub(msg_sub_handle_t handle);

esp_err_t msg_publish(msg_topic_t topic, const msg_t *msg, TickType_t timeout_ticks);

esp_err_t msg_publish_auto(const msg_t *msg, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
