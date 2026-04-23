#include "con.h"

#include <stdbool.h>
#include "freertos/task.h"
#include "config/config_sys.h"
#include "core/state/state.h"
#include "core/state/state_types.h"
#include "core/utils/log.h"

#define CON_INPUT_Q_LEN 32
#define CON_SYS_Q_LEN 32
#define CON_UI_CMD_Q_LEN 32
#define CON_AUDIO_CMD_Q_LEN 32

static con_actor_t g_con = {0};

static esp_err_t con_send_cmd(QueueHandle_t queue, msg_t *cmd_msg, uint32_t *drop_cnt)
{
    if(queue == NULL || cmd_msg == NULL || drop_cnt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(xQueueSend(queue, cmd_msg, 0) == pdTRUE) {
        return ESP_OK;
    }

    (*drop_cnt)++;
    if(((*drop_cnt) & 0x3F) == 1) {
        LOG("con queue full, drop=%u event=%d", (unsigned)(*drop_cnt), (int)cmd_msg->event);
    }
    return ESP_ERR_TIMEOUT;
}

static bool con_is_ui_cmd(msg_event_t evt)
{
    return evt == CMD_UI_UPDATE_TEXT
        || evt == CMD_UI_SCROLL
        || evt == CMD_UI_NAV_STEP;
}

static bool con_is_audio_cmd(msg_event_t evt)
{
    return evt == CMD_AUDIO_TONE
        || evt == CMD_AUDIO_STOP
        || evt == CMD_AUDIO_VOLUME_STEP;
}

static void con_dispatch_state_outputs(msg_t *out_cmds, size_t out_count)
{
    for(size_t i = 0; i < out_count; i++) {
        msg_t *cmd = &out_cmds[i];
        if(con_is_ui_cmd(cmd->event)) {
            (void)con_send_cmd(g_con.ui_cmd_q, cmd, &g_con.ui_cmd_drop_cnt);
        } else if(con_is_audio_cmd(cmd->event)) {
            (void)con_send_cmd(g_con.audio_cmd_q, cmd, &g_con.audio_cmd_drop_cnt);
        } else {
            LOG("unknown cmd event from state: %d", (int)cmd->event);
        }
    }
}

static void con_task(void *arg)
{
    (void)arg;

    msg_t input_msg;
    msg_t sys_msg;
    msg_t out_cmds[STATE_MAX_OUTPUT_CMDS];

    while(1) {
        QueueSetMemberHandle_t activated = xQueueSelectFromSet(g_con.ingress_set, portMAX_DELAY);
        if(activated == NULL) {
            continue;
        }

        if(activated == g_con.input_q) {
            if(xQueueReceive(g_con.input_q, &input_msg, 0) != pdTRUE) {
                continue;
            }

            size_t out_count = 0;
            esp_err_t err = state_handle_input(&input_msg, out_cmds, STATE_MAX_OUTPUT_CMDS, &out_count);
            if(err == ESP_ERR_NOT_FOUND) {
                continue;
            }
            if(err != ESP_OK) {
                LOG("state_handle_input failed: %s", esp_err_to_name(err));
                continue;
            }
            con_dispatch_state_outputs(out_cmds, out_count);
            continue;
        }

        if(activated == g_con.sys_q) {
            if(xQueueReceive(g_con.sys_q, &sys_msg, 0) != pdTRUE) {
                continue;
            }

            size_t out_count = 0;
            esp_err_t err = state_handle_sys(&sys_msg, out_cmds, STATE_MAX_OUTPUT_CMDS, &out_count);
            if(err == ESP_ERR_NOT_FOUND) {
                continue;
            }
            if(err != ESP_OK) {
                LOG("state_handle_sys failed: %s", esp_err_to_name(err));
                continue;
            }
            con_dispatch_state_outputs(out_cmds, out_count);
            continue;
        }
    }
}

esp_err_t con_init(void)
{
    if(g_con.input_q != NULL) {
        return ESP_OK;
    }

    esp_err_t err = state_init();
    if(err != ESP_OK) {
        return err;
    }

    g_con.input_q = xQueueCreate(CON_INPUT_Q_LEN, sizeof(msg_t));
    if(g_con.input_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    g_con.sys_q = xQueueCreate(CON_SYS_Q_LEN, sizeof(msg_t));
    if(g_con.sys_q == NULL) {
        vQueueDelete(g_con.input_q);
        g_con.input_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    g_con.ingress_set = xQueueCreateSet(CON_INPUT_Q_LEN + CON_SYS_Q_LEN);
    if(g_con.ingress_set == NULL) {
        vQueueDelete(g_con.sys_q);
        vQueueDelete(g_con.input_q);
        g_con.sys_q = NULL;
        g_con.input_q = NULL;
        return ESP_ERR_NO_MEM;
    }
    if(xQueueAddToSet(g_con.input_q, g_con.ingress_set) != pdPASS
        || xQueueAddToSet(g_con.sys_q, g_con.ingress_set) != pdPASS) {
        vQueueDelete(g_con.sys_q);
        vQueueDelete(g_con.input_q);
        vQueueDelete(g_con.ingress_set);
        g_con.sys_q = NULL;
        g_con.input_q = NULL;
        g_con.ingress_set = NULL;
        return ESP_FAIL;
    }

    g_con.ui_cmd_q = xQueueCreate(CON_UI_CMD_Q_LEN, sizeof(msg_t));
    if(g_con.ui_cmd_q == NULL) {
        vQueueDelete(g_con.ingress_set);
        vQueueDelete(g_con.sys_q);
        vQueueDelete(g_con.input_q);
        g_con.ingress_set = NULL;
        g_con.sys_q = NULL;
        g_con.input_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    g_con.audio_cmd_q = xQueueCreate(CON_AUDIO_CMD_Q_LEN, sizeof(msg_t));
    if(g_con.audio_cmd_q == NULL) {
        vQueueDelete(g_con.ui_cmd_q);
        vQueueDelete(g_con.ingress_set);
        vQueueDelete(g_con.sys_q);
        vQueueDelete(g_con.input_q);
        g_con.ui_cmd_q = NULL;
        g_con.ingress_set = NULL;
        g_con.sys_q = NULL;
        g_con.input_q = NULL;
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ok = xTaskCreatePinnedToCore(
        con_task,
        "con_task",
        4096,
        NULL,
        TASK_PRIO_CON,
        NULL,
        TASK_CORE_CON
    );
    if(task_ok != pdPASS) {
        vQueueDelete(g_con.audio_cmd_q);
        vQueueDelete(g_con.ui_cmd_q);
        vQueueDelete(g_con.ingress_set);
        vQueueDelete(g_con.sys_q);
        vQueueDelete(g_con.input_q);
        g_con.audio_cmd_q = NULL;
        g_con.ui_cmd_q = NULL;
        g_con.ingress_set = NULL;
        g_con.sys_q = NULL;
        g_con.input_q = NULL;
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t con_post_input(const msg_t *msg, TickType_t timeout_ticks)
{
    if(g_con.input_q == NULL || msg == NULL || msg->type != MSG_TYPE_INPUT) {
        return ESP_ERR_INVALID_ARG;
    }

    if(xQueueSend(g_con.input_q, msg, timeout_ticks) == pdTRUE) {
        return ESP_OK;
    }

    g_con.input_drop_cnt++;
    if((g_con.input_drop_cnt & 0x3F) == 1) {
        LOG("con input queue full, drop=%u", (unsigned)g_con.input_drop_cnt);
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t con_post_sys(const msg_t *msg, TickType_t timeout_ticks)
{
    if(g_con.sys_q == NULL || msg == NULL || msg->type != MSG_TYPE_SYS) {
        return ESP_ERR_INVALID_ARG;
    }

    if(xQueueSend(g_con.sys_q, msg, timeout_ticks) == pdTRUE) {
        return ESP_OK;
    }

    g_con.sys_drop_cnt++;
    if((g_con.sys_drop_cnt & 0x3F) == 1) {
        LOG("con sys queue full, drop=%u", (unsigned)g_con.sys_drop_cnt);
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t con_recv_ui_cmd(msg_t *out_msg, TickType_t timeout_ticks)
{
    if(g_con.ui_cmd_q == NULL || out_msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return (xQueueReceive(g_con.ui_cmd_q, out_msg, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t con_recv_audio_cmd(msg_t *out_msg, TickType_t timeout_ticks)
{
    if(g_con.audio_cmd_q == NULL || out_msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return (xQueueReceive(g_con.audio_cmd_q, out_msg, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t con_recv_sys(msg_t *out_msg, TickType_t timeout_ticks)
{
    if(g_con.sys_q == NULL || out_msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return (xQueueReceive(g_con.sys_q, out_msg, timeout_ticks) == pdTRUE) ? ESP_OK : ESP_ERR_TIMEOUT;
}

void con_get_stats(con_stats_t *out_stats)
{
    if(out_stats == NULL) {
        return;
    }

    out_stats->input_drop_cnt = g_con.input_drop_cnt;
    out_stats->sys_drop_cnt = g_con.sys_drop_cnt;
    out_stats->ui_cmd_drop_cnt = g_con.ui_cmd_drop_cnt;
    out_stats->audio_cmd_drop_cnt = g_con.audio_cmd_drop_cnt;
}
