#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cw_keyer_actor_init(void);
esp_err_t cw_keyer_actor_deinit(void);
const char *cw_keyer_actor_get_raw_text(void);
const char *cw_keyer_actor_get_display_text(void);
size_t cw_keyer_actor_render_display_text(const char *raw_text, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
