#ifndef __ENV_LOGGER_H__
#define __ENV_LOGGER_H__


#include <stdio.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

enum env_log_level {
    ENV_LOG_LEVEL_NONE = 0,
    ENV_LOG_LEVEL_FATAL,
    ENV_LOG_LEVEL_ERROR,
    ENV_LOG_LEVEL_WARN,
    ENV_LOG_LEVEL_INFO,
    ENV_LOG_LEVEL_DEBUG,
    ENV_LOG_LEVEL_TRACE,
    ENV_LOG_LEVEL_COUNT
};

typedef void (*logger_cb_t) (int level, const char *tag, const char *debug, const char *log);

int env_logger_start(const char *path, logger_cb_t cb);

void env_logger_stop();

void env_logger_printf(enum env_log_level level, const char *tag, const char *file, int line, const char *func, const char *fmt, ...);

#define LOGD(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_DEBUG, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)

#define LOGI(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_INFO, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)

#define LOGW(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_WARN, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)

#define LOGE(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_ERROR, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)

#define LOGF(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_FATAL, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)

#define LOGT(__TAG, __FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_TRACE, __TAG, __FILE__, __LINE__, __FUNCTION__, __FORMAT__, ##__VA_ARGS__)


#define __env_check(condition, func) \
        do { \
            if ((condition)) { \
                LOGE("CHECK STATUS", "%s Unusual, %s", func, strerror(errno)); \
                goto Reset; \
            } \
        } while (0)

#ifdef __cplusplus
}
#endif

#endif
