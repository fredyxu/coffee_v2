#pragma once

#include <stddef.h>

#include "core/msg/msg.h"
#include "modules/ui/page/page_settings_item/page_settings_item_data.h"

#ifdef __cplusplus
extern "C" {
#endif

const settings_sub_item_t *page_settings_section_ws_get_sub_items(size_t *count);
bool page_settings_section_ws_handle_msg(const msg_t *msg);

#ifdef __cplusplus
}
#endif
