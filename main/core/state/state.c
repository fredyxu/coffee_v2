#include "state.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "core/state/state_map.h"
#include "core/state/status.h"
#include "core/utils/log.h"

/**
 * @file state.c
 * @brief state 决策引擎实现。
 *
 * 处理流程：
 * 1. 收到输入消息 input
 * 2. 若是 MSG_EVT_INPUT_SCENE_CHANGE，则更新 scene
 * 3. 否则用 (scene + input_event) 查路由表
 * 4. 把命中的命令模板展开为标准 msg_t 命令数组，交给上层分发
 */

/** state 模块运行时上下文。 */
typedef struct {
    /** 模块是否已初始化。 */
    bool inited;
    /** 当前交互场景。 */
    state_scene_t scene;
    /** 保护 scene 的互斥锁。 */
    SemaphoreHandle_t lock;
} state_ctx_t;

static state_ctx_t s_state = {0};

/** 校验场景枚举是否合法。 */
static bool state_scene_valid(state_scene_t scene)
{
    return scene == STATE_SCENE_NONE
		|| scene == STATE_SCENE_INIT
		|| scene == STATE_SCENE_HOME
        || scene == STATE_SCENE_SETTINGS;

}

/** 限定 state 允许下发的命令集合，避免错误模板污染总线。 */
static bool state_command_valid(msg_event_t evt)
{
    return evt == MSG_EVT_CMD_UI_UPDATE_TEXT
        || evt == MSG_EVT_CMD_UI_SCROLL
        || evt == MSG_EVT_CMD_UI_NAV_STEP
        || evt == MSG_EVT_CMD_AUDIO_TONE
        || evt == MSG_EVT_CMD_AUDIO_STOP
        || evt == MSG_EVT_CMD_AUDIO_VOLUME_STEP;
}

esp_err_t state_init(void)
{
    /* 幂等初始化：重复调用返回成功。 */
    if(s_state.inited) {
        return ESP_OK;
    }

    /* 仅需要一个互斥锁来保护 scene 读写。 */
    s_state.lock = xSemaphoreCreateMutex();
    if(s_state.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    s_state.scene = STATE_SCENE_INIT;

    esp_err_t err = status_init();
    if(err != ESP_OK) {
        vSemaphoreDelete(s_state.lock);
        s_state.lock = NULL;
        return err;
    }

    s_state.inited = true;
    return ESP_OK;
}

state_scene_t state_get_scene(void)
{
    /* 未初始化时回落到默认场景，避免上层崩溃。 */
    if(!s_state.inited) {
        return STATE_SCENE_HOME;
    }

    (void)xSemaphoreTake(s_state.lock, portMAX_DELAY);
    state_scene_t scene = s_state.scene;
    (void)xSemaphoreGive(s_state.lock);
    return scene;
}

esp_err_t state_set_scene(state_scene_t scene)
{
    if(!s_state.inited || !state_scene_valid(scene)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* scene 写操作使用互斥保护。 */
    (void)xSemaphoreTake(s_state.lock, portMAX_DELAY);
    s_state.scene = scene;
    (void)xSemaphoreGive(s_state.lock);

    LOG("state scene changed: %d", (int)scene);
    return ESP_OK;
}

static esp_err_t state_build_cmd(const msg_t *input,
                                 const state_cmd_template_t *tpl,
                                 msg_t *out_msg)
{
    if(input == NULL || tpl == NULL || out_msg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if(!state_command_valid(tpl->cmd_event)) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 从输入消息继承 src，统一标记为命令类型，并刷新 timestamp。 */
    *out_msg = msg_make(input->src, MSG_TYPE_CMD, tpl->cmd_event, (uint32_t)xTaskGetTickCount());

    /* 根据 payload 类型把模板参数写入 msg_t 联合体。 */
    switch(tpl->payload_type) {
        case STATE_PAYLOAD_NONE:
            break;
        case STATE_PAYLOAD_VALUE:
            out_msg->data.value = tpl->data.value;
            break;
        case STATE_PAYLOAD_TONE:
            out_msg->data.tone.freq = tpl->data.tone.freq;
            out_msg->data.tone.duration = tpl->data.tone.duration;
            break;
        case STATE_PAYLOAD_TEXT:
            (void)snprintf(out_msg->data.text, sizeof(out_msg->data.text), "%s",
                           tpl->data.text.text ? tpl->data.text.text : "");
            break;
        default:
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t state_emit_ui_text_cmd(const msg_t *src_msg,
                                        const char *text,
                                        msg_t *out_msgs,
                                        size_t out_cap,
                                        size_t *out_count)
{
    if(src_msg == NULL || out_msgs == NULL || out_count == NULL || out_cap < 1) {
        return ESP_ERR_NO_MEM;
    }

    out_msgs[0] = msg_make(src_msg->src, MSG_TYPE_CMD, MSG_EVT_CMD_UI_UPDATE_TEXT, (uint32_t)xTaskGetTickCount());
    (void)snprintf(out_msgs[0].data.text, sizeof(out_msgs[0].data.text), "%s", text ? text : "");
    *out_count = 1;
    return ESP_OK;
}

esp_err_t state_handle_input(const msg_t *input,
                             msg_t *out_msgs,
                             size_t out_cap,
                             size_t *out_count)
{
    if(!s_state.inited || input == NULL || out_count == NULL || input->type != MSG_TYPE_INPUT) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 默认无输出命令。 */
    *out_count = 0;

    /* 特殊输入：状态切换事件，不做路由查找。 */
    if(input->event == MSG_EVT_INPUT_SCENE_CHANGE) {
        return state_set_scene((state_scene_t)input->data.value);
    }

    /* 读取当前 scene，然后进行静态路由匹配。 */
    const state_scene_t current_scene = state_get_scene();

    size_t route_count = 0;
    const state_route_t *routes = state_map_routes(&route_count);
    if(routes == NULL || route_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    /* 线性匹配：规模增大后可替换为哈希/二维表，但当前成本最低且可读性高。 */
    for(size_t i = 0; i < route_count; i++) {
        const state_route_t *r = &routes[i];
        if(r->scene != current_scene || r->input_event != input->event) {
            continue;
        }

        /* 调用者提供的输出缓冲区不足。 */
        if(r->cmd_count > out_cap || out_msgs == NULL) {
            return ESP_ERR_NO_MEM;
        }

        /* 命中后按模板逐条展开命令。 */
        for(size_t j = 0; j < r->cmd_count; j++) {
            esp_err_t err = state_build_cmd(input, &r->cmds[j], &out_msgs[j]);
            if(err != ESP_OK) {
                return err;
            }
        }

        *out_count = r->cmd_count;
        return ESP_OK;
    }

    /* 未命中：由上层决定是忽略还是记录日志。 */
    return ESP_ERR_NOT_FOUND;
}

esp_err_t state_handle_sys(const msg_t *sys_msg,
                           msg_t *out_msgs,
                           size_t out_cap,
                           size_t *out_count)
{
    if(!s_state.inited || sys_msg == NULL || out_count == NULL || sys_msg->type != MSG_TYPE_SYS) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_count = 0;
    const state_scene_t current_scene = state_get_scene();
    (void)status_apply_sys_msg(sys_msg);

    switch(sys_msg->event) {
        case MSG_EVT_SYS_APP_INIT_INFO:
            if(current_scene != STATE_SCENE_INIT) {
                return ESP_OK;
            }
            return state_emit_ui_text_cmd(sys_msg, sys_msg->data.text, out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_INIT_DONE_LVGL:
            if(current_scene != STATE_SCENE_INIT) {
                return ESP_OK;
            }
            return state_emit_ui_text_cmd(sys_msg, "[INIT] LVGL READY", out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_WIFI_CONNECTED:
            return state_emit_ui_text_cmd(sys_msg, "[WIFI] CONNECTED", out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_WIFI_DISCONNECTED:
            return state_emit_ui_text_cmd(sys_msg, "[WIFI] DISCONNECTED", out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_WS_CONNECTED:
            return state_emit_ui_text_cmd(sys_msg, "[WS] CONNECTED", out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_WS_DISCONNECTED:
            return state_emit_ui_text_cmd(sys_msg, "[WS] DISCONNECTED", out_msgs, out_cap, out_count);
        case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
            return state_emit_ui_text_cmd(sys_msg, "[WS] HEARTBEAT LOST", out_msgs, out_cap, out_count);
        default:
            return ESP_ERR_NOT_FOUND;
    }
}
