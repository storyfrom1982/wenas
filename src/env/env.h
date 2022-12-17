#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__

#if defined(_WIN64) && defined(__PL64__)
#   define OS_WINDOWS
#elif defined(__APPLE__) && defined(__PL64__)
#   define OS_APPLE
#elif defined(__ANDROID__) && defined(__PL64__)
#   define OS_ANDROID
#elif defined(__linux__) && defined(__PL64__)
#   define OS_LINUX
#else
#error Not yet adapted to the environment
#endif

#if defined(OS_WINDOWS)
#define inline          _inline
#define __env_export    __declspec(dllexport)
#define __env_import    __declspec(dllimport)
#else
#if __GNUC__ >= 4
#define __env_export    __attribute__((visibility("default")))
#define __env_import    __attribute__((visibility("default")))
#else
#define __env_export
#define __env_import
#endif
#endif

#define NULL    ((void*)0)

///////////////////////////////////////////////////////
///// 2进制
///////////////////////////////////////////////////////
#define __false                 0
#define __true                  1
#ifndef OS_WINDOWS
typedef unsigned char           __bool;
#endif

///////////////////////////////////////////////////////
///// 256进制符号
///////////////////////////////////////////////////////
typedef char                    __sym;
typedef char*                   __symptr;

///////////////////////////////////////////////////////
///// 可变类型指针
///////////////////////////////////////////////////////
typedef void*                   __ptr;

///////////////////////////////////////////////////////
///// 自然数集
///////////////////////////////////////////////////////
typedef unsigned char           __uint8;
typedef unsigned short          __uint16;
#ifdef __ENV_LONG_64__
typedef unsigned int            __uint32;
typedef unsigned long           __uint64;
#else
typedef unsigned long           __uint32;
typedef unsigned long long      __uint64;
#endif

///////////////////////////////////////////////////////
///// 整数集
///////////////////////////////////////////////////////
typedef char                    __sint8;
typedef short                   __sint16;
#ifdef __ENV_LONG_64__
typedef int                     __sint32;
typedef long                    __sint64;
#else
typedef long                    __sint32;
typedef long long               __sint64;
#endif

///////////////////////////////////////////////////////
///// 实数集
///////////////////////////////////////////////////////
typedef float                   __real32;
typedef double                  __real64;

///////////////////////////////////////////////////////
///// 返回值类型
///////////////////////////////////////////////////////
typedef int                     __result;

///////////////////////////////////////////////////////
///// 当前状态
///////////////////////////////////////////////////////
__env_export const __sym* env_check(void);
__env_export const __sym* env_parser(__result error);

///////////////////////////////////////////////////////
///// 时间相关
///////////////////////////////////////////////////////
#define MILLI_SECONDS    1000ULL
#define MICRO_SECONDS    1000000ULL
#define NANO_SECONDS     1000000000ULL

__env_export __uint64 env_time(void);
__env_export __uint64 env_clock(void);
__env_export __uint64 env_strtime(__sym *buf, __uint64 size, __uint64 millisecond);

///////////////////////////////////////////////////////
///// 存储相关
///////////////////////////////////////////////////////
typedef void*   __fp;

__env_export __fp env_fopen(const __symptr path, const __symptr mode);
__env_export __bool env_fclose(__fp fp);
__env_export __sint64 env_ftell(__fp fp);
__env_export __sint64 env_fflush(__fp fp);
__env_export __sint64 env_fwrite(__fp fp, __ptr data, __uint64 size);
__env_export __sint64 env_fread(__fp fp, __ptr buf, __uint64 size);
__env_export __sint64 env_fseek(__fp fp, __sint64 offset, __sint32 whence);

__env_export __bool env_make_path(const __symptr path);
__env_export __bool env_find_path(const __symptr path);
__env_export __bool env_find_file(const __symptr path);
__env_export __bool env_remove_path(const __symptr path);
__env_export __bool env_remove_file(const __symptr path);
__env_export __bool env_move_path(const __symptr from, const __symptr to);

///////////////////////////////////////////////////////
///// 线程相关
///////////////////////////////////////////////////////
typedef __result (*env_thread_cb)(__ptr ctx);
typedef __uint64 env_thread_t;
typedef struct env_mutex env_mutex_t;

__env_export __result env_thread_create(env_thread_t *tid, env_thread_cb cb, __ptr ctx);
__env_export __result env_thread_destroy(env_thread_t tid);
__env_export env_thread_t env_thread_self();
__env_export void env_thread_sleep(__uint64 nano_seconds);

__env_export env_mutex_t* env_mutex_create(void);
__env_export void env_mutex_destroy(env_mutex_t **pp_mutex);
__env_export void env_mutex_lock(env_mutex_t *mutex);
__env_export void env_mutex_unlock(env_mutex_t *mutex);
__env_export void env_mutex_signal(env_mutex_t *mutex);
__env_export void env_mutex_broadcast(env_mutex_t *mutex);
__env_export void env_mutex_wait(env_mutex_t *mutex);
__env_export __result env_mutex_timedwait(env_mutex_t *mutex, __uint64 timeout);

typedef struct env_pipe env_pipe_t;
__env_export env_pipe_t* env_pipe_create(__uint64 len);
__env_export void env_pipe_destroy(env_pipe_t **pp_pipe);
__env_export __uint64 env_pipe_write(env_pipe_t *pipe, __ptr data, __uint64 len);
__env_export __uint64 env_pipe_read(env_pipe_t *pipe, __ptr buf, __uint64 len);
__env_export __uint64 env_pipe_readable(env_pipe_t *pipe);
__env_export __uint64 env_pipe_writable(env_pipe_t *pipe);
__env_export void env_pipe_stop(env_pipe_t *pipe);
__env_export void env_pipe_clear(env_pipe_t *pipe);

///////////////////////////////////////////////////////
///// 源自操作
///////////////////////////////////////////////////////
typedef __uint64 __atombool;

__env_export __uint64 env_atomic_load(volatile __uint64*);
__env_export void env_atomic_store(volatile __uint64*, __uint64);
__env_export __uint64 env_atomic_exchange(volatile __uint64*, __uint64);
__env_export __bool env_atomic_compare_exchange(volatile __uint64*, __uint64*, __uint64);
__env_export __uint64 env_atomic_increment(volatile __uint64*);
__env_export __uint64 env_atomic_decrement(volatile __uint64*);
__env_export __uint64 env_atomic_add(volatile __uint64*, __uint64);
__env_export __uint64 env_atomic_subtract(volatile __uint64*, __uint64);
__env_export __uint64 env_atomic_and(volatile __uint64*, __uint64);
__env_export __uint64 env_atomic_or(volatile __uint64*, __uint64);
__env_export __uint64 env_atomic_xor(volatile __uint64*, __uint64);
__env_export __atombool env_atomic_is_true(volatile __atombool*);
__env_export __atombool env_atomic_is_false(volatile __atombool*);
__env_export __atombool env_atomic_set_true(volatile __atombool*);
__env_export __atombool env_atomic_set_false(volatile __atombool*);

#define	__is_true(x) env_atomic_is_true(&(x))
#define	__is_false(x) env_atomic_is_false(&(x))
#define	__set_true(x) env_atomic_set_true(&(x))
#define	__set_false(x) env_atomic_set_false(&(x))
#define __atom_add(x, y) env_atomic_add(&(x), (y))
#define __atom_sub(x, y) env_atomic_subtract(&(x), (y))
#define __atom_lock(x) while(!__set_true(x)) env_thread_sleep(1ULL)
#define __atom_try_lock(x) __set_true(x)
#define __atom_unlock(x) __set_false(x)

///////////////////////////////////////////////////////
///// 状态码
///////////////////////////////////////////////////////
#define ENV_TIMEDOUT        1

///////////////////////////////////////////////////////
///// 日志存储
///////////////////////////////////////////////////////
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

typedef void (*env_logger_cb) (__sint32 level, const __sym *log);
__env_export __result env_logger_start(const __sym *path, env_logger_cb cb);
__env_export void env_logger_stop();
__env_export void env_logger_printf(enum env_log_level level, const __sym *file, __sint32 line, const __sym *fmt, ...);

#define __logd(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_DEBUG, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __logi(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_INFO, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __logw(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_WARN, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __loge(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_ERROR, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __logf(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_FATAL, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __logt(__FORMAT__, ...) \
        env_logger_printf(ENV_LOG_LEVEL_TRACE, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __pass(condition) \
    do { \
        if (!(condition)) { \
            __loge("Check condition failed: %s, %s\n", #condition, env_check()); \
            goto Reset; \
        } \
    } while (__false)

///////////////////////////////////////////////////////
///// 堆栈回溯
///////////////////////////////////////////////////////
#ifdef ENV_HAVA_BACKTRACE
__env_export void env_backtrace_setup();
__env_export __uint64 env_backtrace(__ptr* array, __sint32 depth);
#endif


///////////////////////////////////////////////////////
///// 内存安全
///////////////////////////////////////////////////////
__env_export __ptr malloc(__uint64 size);
__env_export __ptr calloc(__uint64 number, __uint64 size);
__env_export __ptr realloc(__ptr address, __uint64 size);
__env_export __ptr memalign(__uint64 boundary, __uint64 size);
__env_export __ptr aligned_alloc(__uint64 alignment, __uint64 size);
__env_export __ptr _aligned_alloc(__uint64 alignment, __uint64 size);
__env_export __sym* strdup(const __sym *s);
__env_export __sym* strndup(const __sym *s, __uint64 n);
__env_export __result posix_memalign(__ptr *ptr, __uint64 align, __uint64 size);
__env_export void free(__ptr address);
__env_export void env_malloc_debug(void (*cb)(const __sym *debug));

#endif //__ENV_ENV_H__