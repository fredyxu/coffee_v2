#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "core/msg/msg.h"

typedef struct {
    QueueHandle_t input_q;
    QueueHandle_t sys_q;
    QueueSetHandle_t ingress_set;
    QueueHandle_t ui_cmd_q;
    QueueHandle_t audio_cmd_q;
    uint32_t input_drop_cnt;
    uint32_t sys_drop_cnt;
    uint32_t ui_cmd_drop_cnt;
    uint32_t audio_cmd_drop_cnt;
} con_actor_t;

typedef struct {
    uint32_t input_drop_cnt;
    uint32_t sys_drop_cnt;
    uint32_t ui_cmd_drop_cnt;
    uint32_t audio_cmd_drop_cnt;
} con_stats_t;

esp_err_t con_init(void);

esp_err_t con_post_input(const msg_t *msg, TickType_t timeout_ticks);
esp_err_t con_post_sys(const msg_t *msg, TickType_t timeout_ticks);
esp_err_t con_recv_ui_cmd(msg_t *out_msg, TickType_t timeout_ticks);
esp_err_t con_recv_audio_cmd(msg_t *out_msg, TickType_t timeout_ticks);
esp_err_t con_recv_sys(msg_t *out_msg, TickType_t timeout_ticks);

void con_get_stats(con_stats_t *out_stats);
