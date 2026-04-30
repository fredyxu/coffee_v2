#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool wifi_connected;
    uint8_t wifi_level; /* 0=未连接, 1~4=信号等级 */
    bool ws_connected;
} status_current_t;

esp_err_t status_init(void);
esp_err_t status_get_current(status_current_t *out_current);
esp_err_t status_apply_sys_msg(const msg_t *sys_msg);

#ifdef __cplusplus
}
#endif
