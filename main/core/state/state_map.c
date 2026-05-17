#include "state_map.h"

/**
 * @file state_map.c
 * @brief 场景路由表定义：把“场景 + 输入事件”映射到“输出命令列表”。
 *
 * 扩展建议：
 * 1. 新增场景行为时，优先只在本文件添加/修改路由项。
 * 2. 尽量保持一条路由只描述一个输入场景组合，避免隐式副作用。
 * 3. cmds 数量不要超过 STATE_MAX_OUTPUT_CMDS。
 */

/* 便捷宏：减少静态路由表样板代码。 */
#define ROUTE_CMD_NONE(_evt) { .cmd_event = (_evt), .payload_type = STATE_PAYLOAD_NONE }
#define ROUTE_CMD_VALUE(_evt, _v) { .cmd_event = (_evt), .payload_type = STATE_PAYLOAD_VALUE, .data.value = (_v) }
#define ROUTE_CMD_TONE(_freq, _dur) { .cmd_event = MSG_EVT_CMD_AUDIO_TONE, .payload_type = STATE_PAYLOAD_TONE, .data.tone = { .freq = (_freq), .duration = (_dur) } }
#define ROUTE_CMD_TEXT(_txt) { .cmd_event = MSG_EVT_CMD_UI_UPDATE_TEXT, .payload_type = STATE_PAYLOAD_TEXT, .data.text = { .text = (_txt) } }

/* 路由静态常量表。state_handle_input 会线性匹配本表。 */
static const state_route_t s_routes[] = {
    // HOME: 编码器用于音量调节
    {
        .scene = STATE_SCENE_HOME,
        .input_event = MSG_EVT_INPUT_ENCODER_CW,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_VALUE(MSG_EVT_CMD_AUDIO_VOLUME_STEP, +1) },
    },
    {
        .scene = STATE_SCENE_HOME,
        .input_event = MSG_EVT_INPUT_ENCODER_CCW,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_VALUE(MSG_EVT_CMD_AUDIO_VOLUME_STEP, -1) },
    },
    {
        .scene = STATE_SCENE_HOME,
        .input_event = MSG_EVT_INPUT_ENCODER_PRESS,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_NONE(MSG_EVT_CMD_AUDIO_STOP) },
    },

    // SETTINGS: 编码器用于菜单移动
    {
        .scene = STATE_SCENE_SETTINGS,
        .input_event = MSG_EVT_INPUT_ENCODER_CW,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_VALUE(MSG_EVT_CMD_UI_NAV_STEP, +1) },
    },
    {
        .scene = STATE_SCENE_SETTINGS,
        .input_event = MSG_EVT_INPUT_ENCODER_CCW,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_VALUE(MSG_EVT_CMD_UI_NAV_STEP, -1) },
    },
    {
        .scene = STATE_SCENE_SETTINGS,
        .input_event = MSG_EVT_INPUT_ENCODER_PRESS,
        .cmd_count = 1,
        .cmds = { ROUTE_CMD_TEXT("[SET] ENTER") },
    },

    // 电键：全场景共用
    {
        .scene = STATE_SCENE_HOME,
        .input_event = MSG_EVT_INPUT_KEY_DI,
        .cmd_count = 2,
        .cmds = { ROUTE_CMD_TONE(800, 100), ROUTE_CMD_TEXT(".") },
    },
    {
        .scene = STATE_SCENE_HOME,
        .input_event = MSG_EVT_INPUT_KEY_DA,
        .cmd_count = 2,
        .cmds = { ROUTE_CMD_TONE(800, 300), ROUTE_CMD_TEXT("-") },
    },
    {
        .scene = STATE_SCENE_SETTINGS,
        .input_event = MSG_EVT_INPUT_KEY_DI,
        .cmd_count = 2,
        .cmds = { ROUTE_CMD_TONE(800, 100), ROUTE_CMD_TEXT(".") },
    },
    {
        .scene = STATE_SCENE_SETTINGS,
        .input_event = MSG_EVT_INPUT_KEY_DA,
        .cmd_count = 2,
        .cmds = { ROUTE_CMD_TONE(800, 300), ROUTE_CMD_TEXT("-") },
    },
    // {
    //     .scene = STATE_SCENE_EDIT,
    //     .input_event = MSG_EVT_INPUT_KEY_DI,
    //     .cmd_count = 2,
    //     .cmds = { ROUTE_CMD_TONE(800, 100), ROUTE_CMD_TEXT(".") },
    // },
    // {
    //     .scene = STATE_SCENE_EDIT,
    //     .input_event = MSG_EVT_INPUT_KEY_DA,
    //     .cmd_count = 2,
    //     .cmds = { ROUTE_CMD_TONE(800, 300), ROUTE_CMD_TEXT("-") },
    // },
};

/**
 * @brief 导出路由表给 state.c 使用。
 */
const state_route_t *state_map_routes(size_t *out_count)
{
    if(out_count) {
        *out_count = sizeof(s_routes) / sizeof(s_routes[0]);
    }
    return s_routes;
}
