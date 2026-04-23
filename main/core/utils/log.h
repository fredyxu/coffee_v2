#pragma once

#include <stdio.h>
#include "config/config_sys.h" 

// --- 颜色定义 ---
#define LOG_CLR_RESET  "\033[0m"
#define LOG_CLR_CYAN   "\033[0;36m"
#define LOG_CLR_GREEN  "\033[0;32m"

#if (defined(CONFIG_APP_MODE) && CONFIG_APP_MODE == 1)

    // 底层输出函数：接受文件名、行号以及标准 printf 风格的可变参数
    void log_output(const char* file, int line, const char* format, ...);

    /**
     * 核心改进：
     * 直接使用 __VA_ARGS__ 包含 format 在内的所有内容，
     * 避免因 format 参数单独隔离导致的参数截断。
     */
    #define LOG(...) log_output(__FILE__, __LINE__, __VA_ARGS__)

#else
    // 当 CONFIG_APP_MODE = 0 时，整个 LOG 语句会被编译器物理移除
    #define LOG(...) ((void)0)
#endif