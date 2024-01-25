#ifndef __EX_EX_H__
#define __EX_EX_H__

#if !defined(__linux__) && !defined(__APPLE__)
#error Not yet adapted to the environment
#endif

#if __GNUC__ >= 4
#define __ex_export    __attribute__((visibility("default")))
#define __ex_import    __attribute__((visibility("default")))
#else
#define __ex_export
#define __ex_import
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


/////////////////////////////////////////////////////////
/////// 可变类型指针
/////////////////////////////////////////////////////////
typedef void*                   __ptr;

/////////////////////////////////////////////////////////
/////// 时间相关
/////////////////////////////////////////////////////////

#define MILLI_SECONDS    1000ULL
#define MICRO_SECONDS    1000000ULL
#define NANO_SECONDS     1000000000ULL


#ifdef __cplusplus
extern "C" {
#endif


///////////////////////////////////////////////////////
///// 存储相关
///////////////////////////////////////////////////////

//将来不再考虑机械硬盘，树存储将以接近内存的速度进行读写

typedef void*   __ex_fp;

__ex_export __ex_fp __ex_fopen(const char* path, const char* mode);
__ex_export bool __ex_fclose(__ex_fp fp);
__ex_export int64_t __ex_ftell(__ex_fp fp);
__ex_export int64_t __ex_fflush(__ex_fp fp);
__ex_export int64_t __ex_fwrite(__ex_fp fp, __ptr data, uint64_t size);
__ex_export int64_t __ex_fread(__ex_fp fp, __ptr buf, uint64_t size);
__ex_export int64_t __ex_fseek(__ex_fp fp, int64_t offset, int32_t whence);

__ex_export bool __ex_make_path(const char* path);
__ex_export bool __ex_find_path(const char* path);
__ex_export bool __ex_find_file(const char* path);
__ex_export bool __ex_remove_path(const char* path);
__ex_export bool __ex_remove_file(const char* path);
__ex_export bool __ex_move_path(const char* from, const char* to);

///////////////////////////////////////////////////////
///// 线程相关
///////////////////////////////////////////////////////

typedef struct ex_pipe __ex_pipe;
__ex_export __ex_pipe* __ex_pipe_create(uint64_t len);
__ex_export void __ex_pipe_destroy(__ex_pipe **pp_pipe);
__ex_export uint64_t __ex_pipe_write(__ex_pipe *pipe, __ptr data, uint64_t len);
__ex_export uint64_t __ex_pipe_read(__ex_pipe *pipe, __ptr buf, uint64_t len);
__ex_export uint64_t __ex_pipe_readable(__ex_pipe *pipe);
__ex_export uint64_t __ex_pipe_writable(__ex_pipe *pipe);
__ex_export void __ex_pipe_stop(__ex_pipe *pipe);
__ex_export void __ex_pipe_clear(__ex_pipe *pipe);


///////////////////////////////////////////////////////
///// 日志存储
///////////////////////////////////////////////////////
enum env_log_level {
    EX_LOG_LEVEL_NONE = 0,
    EX_LOG_LEVEL_FATAL,
    EX_LOG_LEVEL_ERROR,
    EX_LOG_LEVEL_WARN,
    EX_LOG_LEVEL_INFO,
    EX_LOG_LEVEL_DEBUG,
    EX_LOG_LEVEL_TRACE,
    EX_LOG_LEVEL_COUNT
};

typedef void (*__ex_log_cb) (int32_t level, const char *log);
__ex_export int __ex_log_file_open(const char *path, __ex_log_cb cb);
__ex_export void __ex_log_file_close();
__ex_export void __ex_log_printf(enum env_log_level level, const char *file, int line, const char *fmt, ...);

#define __ex_logd(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __ex_logi(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_INFO, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __ex_logw(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_WARN, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __ex_loge(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_ERROR, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __ex_logf(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_FATAL, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __ex_logt(__FORMAT__, ...) \
        __ex_log_printf(EX_LOG_LEVEL_TRACE, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

// #define __pass(condition) \
//     do { \
//         if (!(condition)) { \
//             __ex_loge("Check condition failed: %s, %s\n", #condition, env_check()); \
//             goto Reset; \
//         } \
//     } while (false)


#ifdef __cplusplus
}
#endif

#endif //__EX_EX_H__
