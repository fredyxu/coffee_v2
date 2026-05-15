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

/**
 * @brief Subscribe a queue to exact messages matching (type, event).
 *
 * Matching msg_t values are copied into the subscriber queue. The dispatcher
 * does not call callbacks, so subscribers process messages in their own task
 * context.
 */
esp_err_t msg_sub_queue(msg_type_t type,
                        msg_event_t event,
                        QueueHandle_t queue,
                        msg_sub_handle_t *out_handle);

/**
 * @brief Cancel a previous queue subscription.
 */
esp_err_t msg_unsub(msg_sub_handle_t handle);

/**
 * @brief Publish one message to all matching queue subscribers.
 *
 * Intended for core/con dispatch code. Normal modules should keep using
 * msg_send_input/msg_send_sys helpers so messages still pass through con.
 */
esp_err_t msg_pub_subs(const msg_t *msg, TickType_t timeout_ticks);

#ifdef __cplusplus
}
#endif
