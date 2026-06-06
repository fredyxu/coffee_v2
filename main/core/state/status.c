#include "status.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    bool inited;
    SemaphoreHandle_t lock;
    status_current_t current;
} status_ctx_t;

static status_ctx_t s_status = {0};

esp_err_t status_init(void)
{
    if(s_status.inited) {
        return ESP_OK;
    }

    s_status.lock = xSemaphoreCreateMutex();
    if(s_status.lock == NULL) {
        return ESP_ERR_NO_MEM;
    }

    memset(&s_status.current, 0, sizeof(s_status.current));
    s_status.inited = true;
    return ESP_OK;
}

esp_err_t status_get_current(status_current_t *out_current)
{
    if(!s_status.inited || out_current == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)xSemaphoreTake(s_status.lock, portMAX_DELAY);
    *out_current = s_status.current;
    (void)xSemaphoreGive(s_status.lock);
    return ESP_OK;
}

esp_err_t status_apply_sys_msg(const msg_t *sys_msg)
{
    if(!s_status.inited || sys_msg == NULL || sys_msg->type != MSG_TYPE_SYS) {
        return ESP_ERR_INVALID_ARG;
    }

    (void)xSemaphoreTake(s_status.lock, portMAX_DELAY);

    switch(sys_msg->event) {
        case MSG_EVT_SYS_WIFI_CONNECTED:
            s_status.current.wifi_connected = true;
            if(s_status.current.wifi_level == 0) {
                s_status.current.wifi_level = 4;
            }
            break;
        case MSG_EVT_SYS_WIFI_DISCONNECTED:
            s_status.current.wifi_connected = false;
            s_status.current.wifi_level = 0;
            break;
        case MSG_EVT_SYS_WIFI_SIGNAL_LEVEL: {
            int lv = sys_msg->data.value;
            if(lv < 1) lv = 1;
            if(lv > 4) lv = 4;
            s_status.current.wifi_connected = true;
            s_status.current.wifi_level = (uint8_t)lv;
            break;
        }
        case MSG_EVT_SYS_WS_CONNECTED:
            s_status.current.ws_connected = true;
            break;
        case MSG_EVT_SYS_WS_DISCONNECTED:
            s_status.current.ws_connected = false;
            break;
        case MSG_EVT_SYS_WS_HEARTBEAT_LOST:
            s_status.current.ws_connected = false;
            break;
        default:
            break;
    }
    (void)xSemaphoreGive(s_status.lock);
    return ESP_OK;
}
