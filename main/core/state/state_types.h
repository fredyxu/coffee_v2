#pragma once

/**
 * @file state_types.h
 * @brief state 模块的公共类型定义（场景、路由项、命令模板）。
 *
 * 设计目标：
 * 1. 把“输入事件 -> 命令输出”的映射描述为数据结构，而不是硬编码 if/switch。
 * 2. 让 state.c 只负责执行映射，不关心具体业务细节。
 * 3. 让新增场景/新增输入行为时，优先只改 state_map.c。
 */

#include <stddef.h>
#include <stdint.h>
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 单个输入事件最多映射出的命令数量。 */
#define STATE_MAX_OUTPUT_CMDS 3

/**
 * @brief UI/业务场景枚举。
 *
 * 注意：scene 是“交互语义层”概念，不是页面 id 的硬绑定。
 * 如果后续页面增多，建议由 UI 层把 page_id 映射成 scene，再发给 state。
 */
typedef enum {
	STATE_SCENE_NONE,
	STATE_SCENE_INIT,
    STATE_SCENE_HOME,
    STATE_SCENE_SETTINGS,
} state_scene_t;

/**
 * @brief 命令载荷类型。
 *
 * state_map 中每条命令模板会声明自己的 payload_type，
 * state.c 根据该类型把模板数据拷贝到 msg_t 的 union 中。
 */
typedef enum {
    STATE_PAYLOAD_NONE = 0,
    STATE_PAYLOAD_VALUE,
    STATE_PAYLOAD_TONE,
    STATE_PAYLOAD_TEXT,
} state_payload_type_t;

/**
 * @brief 一条“输出命令模板”。
 *
 * 含义：匹配到某条路由后，生成一条 msg_t 命令时所需的静态模板。
 * - cmd_event：输出命令类型（例如 MSG_EVT_CMD_AUDIO_TONE）
 * - payload_type + data：命令参数（例如频率/时长、整数步进、文本）
 */
typedef struct {
    msg_event_t cmd_event;
    state_payload_type_t payload_type;
    union {
        int value;
        struct {
            int freq;
            int duration;
        } tone;
        struct {
            const char *text;
        } text;
    } data;
} state_cmd_template_t;

/**
 * @brief 一条完整路由定义。
 *
 * 当且仅当：
 * - 当前 scene == route.scene
 * - 输入事件 input_event == route.input_event
 * 才会命中此路由，并输出 cmds[0..cmd_count-1]。
 */
typedef struct {
    state_scene_t scene;
    msg_event_t input_event;
    uint8_t cmd_count;
    state_cmd_template_t cmds[STATE_MAX_OUTPUT_CMDS];
} state_route_t;


#ifdef __cplusplus
}
#endif
