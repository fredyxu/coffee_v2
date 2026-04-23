#pragma once

/* ============================================================
 * 任务调度配置（优先级 / 绑核）
 * ============================================================ */

/* 优先级：值越大优先级越高 */
#define TASK_PRIO_AUDIO 6
#define TASK_PRIO_MIC 3
#define TASK_PRIO_LVGL 5
#define TASK_PRIO_CON 5
#define TASK_PRIO_ENCODER 4

/* 核绑定：0=CPU0, 1=CPU1 */
/* 推荐：UI 单独在 Core1，音频/路由/输入在 Core0 */
#define TASK_CORE_AUDIO 0
#define TASK_CORE_MIC 0
#define TASK_CORE_LVGL 1
#define TASK_CORE_CON 0
#define TASK_CORE_ENCODER 0
