#pragma once

#include <stddef.h>

#include "modules/ui/page/page_settings_item/page_settings_item_data.h"

#ifdef __cplusplus
extern "C" {
#endif

const settings_sub_item_t *page_settings_section_morse_get_sub_items(size_t *count);

#ifdef __cplusplus
}
#endif
