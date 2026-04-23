#pragma once

/**
 * @file state_map.h
 * @brief state 路由表导出接口。
 *
 * 路由表本体在 state_map.c，state.c 通过该接口读取常量表。
 */

#include <stddef.h>
#include "core/state/state_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取只读路由表。
 * @param out_count 输出路由条数，可为 NULL。
 * @return 路由数组首地址（静态常量，不可修改）。
 */
const state_route_t *state_map_routes(size_t *out_count);

#ifdef __cplusplus
}
#endif
