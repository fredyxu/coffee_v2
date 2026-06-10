#pragma once

#include <stddef.h>

#include "modules/ui/page/page_settings/page_settings_data.h"

#ifdef __cplusplus
extern "C" {
#endif

const settings_sub_item_t *page_settings_section_ws_get_sub_items(size_t *count);

#ifdef __cplusplus
}
#endif
