#pragma once

#include "esp_err.h"
#include "core/msg/msg.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void (*on_input)(const msg_t *msg);
} ui_page_ops_t;


esp_err_t ui_actor_init(void);

void ui_actor_set_ops(const ui_page_ops_t *ops);
void ui_actor_clean_ops();


#ifdef __cplusplus
}
#endif
