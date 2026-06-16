#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cw_keyer_actor_init(void);
esp_err_t cw_keyer_actor_deinit(void);
const char *cw_keyer_actor_get_raw_text(void);
const char *cw_keyer_actor_get_display_text(void);

#ifdef __cplusplus
}
#endif
