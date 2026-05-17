#pragma once

/**
 * @file state.h
 * @brief 交互状态机接口（场景管理 + 输入决策）。
 *
 * 模块职责：
 * 1. 保存当前 scene（HOME/SETTINGS/EDIT...）
 * 2. 处理输入事件并产生命令列表
 * 3. 对外提供切换 scene 的统一入口
 *
 * 非职责：
 * - 不直接操作 UI / Audio / Network
 * - 不执行命令，只负责决策与输出命令消息
 */

#include <stddef.h>
#include "esp_err.h"
#include "core/msg/msg.h"
#include "core/state/state_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 state 模块。
 * @return ESP_OK 成功；ESP_ERR_NO_MEM 资源不足。
 */
esp_err_t state_init(void);

/**
 * @brief 获取当前 scene。
 * @return 当前场景；若未初始化则返回默认 HOME。
 */
state_scene_t state_get_scene(void);

/**
 * @brief 设置当前 scene。
 * @param scene 目标场景。
 * @return ESP_OK 成功；ESP_ERR_INVALID_ARG 参数非法或模块未初始化。
 */
esp_err_t state_set_scene(state_scene_t scene);

/**
 * @brief 处理单条输入消息，生成 0~N 条命令消息。
 *
 * 典型用法：由订阅对应输入主题的 actor 调用。
 * - input: 原始输入消息（例如编码器、按键）
 * - out_msgs: 输出命令数组
 * - out_cap: out_msgs 的容量（msg_t 个数）
 * - out_count: 实际输出条数
 *
 * 特殊规则：
 * - input->event == MSG_EVT_INPUT_SCENE_CHANGE 时，不生成命令，仅切换 scene。
 *
 * 返回值：
 * - ESP_OK：处理成功（可能 out_count=0）
 * - ESP_ERR_NOT_FOUND：没有匹配路由
 * - ESP_ERR_NO_MEM：输出数组容量不足
 * - ESP_ERR_INVALID_ARG：参数或命令模板非法
 */
esp_err_t state_handle_input(const msg_t *input,
                             msg_t *out_msgs,
                             size_t out_cap,
                             size_t *out_count);

/**
 * @brief 处理单条系统消息，生成 0~N 条命令消息。
 *
 * 典型用法：由订阅对应系统事件主题的 actor 调用。
 * - sys_msg: 系统消息（如 INIT_DONE/WIFI 状态）
 * - out_msgs: 输出命令数组
 * - out_cap: out_msgs 的容量（msg_t 个数）
 * - out_count: 实际输出条数
 *
 * 返回值：
 * - ESP_OK：处理成功（可能 out_count=0）
 * - ESP_ERR_NOT_FOUND：没有匹配处理策略
 * - ESP_ERR_NO_MEM：输出数组容量不足
 * - ESP_ERR_INVALID_ARG：参数非法
 */
esp_err_t state_handle_sys(const msg_t *sys_msg,
                           msg_t *out_msgs,
                           size_t out_cap,
                           size_t *out_count);

#ifdef __cplusplus
}
#endif
