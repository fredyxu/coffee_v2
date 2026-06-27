#include "log.h"

#if (defined(CONFIG_APP_MODE) && CONFIG_APP_MODE == 1)

#include <stdarg.h>
#include <string.h>
#include <time.h>

void log_output(const char* file, int line, const char* format, ...) {
    // 1. 提取简短文件名
    const char* file_name = strrchr(file, '/');
    file_name = (file_name) ? (file_name + 1) : file;

    // 2. 打印醒目的位置前缀。SNTP 同步后附带墙钟时间，便于和服务端日志对齐。
    time_t now = 0;
    time(&now);
    if(now > 1700000000) {
        struct tm tm_now = {0};
        localtime_r(&now, &tm_now);
        char time_text[24] = {0};
        strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S", &tm_now);
        printf(LOG_CLR_CYAN ">>> [%s] [%s:%d] " LOG_CLR_RESET, time_text, file_name, line);
    } else {
        printf(LOG_CLR_CYAN ">>> [%s:%d] " LOG_CLR_RESET, file_name, line);
    }

    // 3. 核心：处理不定长度参数
    va_list args;
    va_start(args, format);   // 从 format 参数之后开始寻找变参
    vprintf(format, args);    // 将所有参数交给 vprintf 进行格式化解析
    va_end(args);

    // 4. 换行并强制刷新
    printf("\r\n");
    fflush(stdout);
}

#endif
