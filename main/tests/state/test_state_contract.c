#include "tests/state/test_state_contract.h"

#include <stdbool.h>
#include "esp_err.h"
#include "core/msg/msg.h"
#include "core/state/state.h"
#include "core/state/state_types.h"
#include "core/utils/log.h"

static bool test_expect(const char *name, bool pass)
{
    LOG("[TEST][STATE] %s: %s", name, pass ? "PASS" : "FAIL");
    return pass;
}

void test_state_contract_run(void)
{
    bool all_pass = true;
    esp_err_t err = state_init();
    all_pass &= test_expect("state_init", err == ESP_OK);

    msg_t out_cmds[STATE_MAX_OUTPUT_CMDS];
    size_t out_count = 0;

    msg_t sys_msg = msg_make(MSG_SRC_WIFI, MSG_TYPE_SYS, MSG_EVT_SYS_WIFI_CONNECTED, 0);
    err = state_handle_input(&sys_msg, out_cmds, STATE_MAX_OUTPUT_CMDS, &out_count);
    all_pass &= test_expect("reject_sys_msg", err == ESP_ERR_INVALID_ARG);

    msg_t cmd_msg = msg_make(MSG_SRC_WIFI, MSG_TYPE_CMD, MSG_EVT_CMD_UI_SCROLL, 0);
    err = state_handle_input(&cmd_msg, out_cmds, STATE_MAX_OUTPUT_CMDS, &out_count);
    all_pass &= test_expect("reject_cmd_msg", err == ESP_ERR_INVALID_ARG);

    msg_t scene_change_msg = msg_make(MSG_SRC_LVGL, MSG_TYPE_INPUT, MSG_EVT_INPUT_SCENE_CHANGE, 0);
    scene_change_msg.data.value = STATE_SCENE_SETTINGS;
    err = state_handle_input(&scene_change_msg, out_cmds, STATE_MAX_OUTPUT_CMDS, &out_count);
    all_pass &= test_expect("accept_scene_change", err == ESP_OK && out_count == 0);
    all_pass &= test_expect("scene_updated", state_get_scene() == STATE_SCENE_SETTINGS);

    LOG("[TEST][STATE] contract: %s", all_pass ? "PASS" : "FAIL");
}
