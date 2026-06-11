#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cw_keyer_actor_init(void);
esp_err_t cw_keyer_actor_deinit(void);

#ifdef __cplusplus
}
#endif
