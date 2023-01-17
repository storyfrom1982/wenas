#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__

#if defined(_WIN64)
#   define OS_WINDOWS
#elif defined(__APPLE__)
#   define OS_APPLE
#elif defined(__ANDROID__)
#   define OS_ANDROID
#elif defined(__linux__)
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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////
///// 可变类型指针
///////////////////////////////////////////////////////
typedef void*                   __ptr;

///////////////////////////////////////////////////////
///// 当前状态
///////////////////////////////////////////////////////
__env_export const char* env_check(void);

///////////////////////////////////////////////////////
///// 时间相关
///////////////////////////////////////////////////////
#define MILLI_SECONDS    1000ULL
#define MICRO_SECONDS    1000000ULL
#define NANO_SECONDS     1000000000ULL

__env_export uint64_t env_time(void);
__env_export uint64_t env_clock(void);
__env_export uint64_t env_strftime(char *buf, uint64_t size, uint64_t millisecond);

///////////////////////////////////////////////////////
///// 存储相关
///////////////////////////////////////////////////////
typedef void*   __fp;

__env_export __fp env_fopen(const char* path, const char* mode);
__env_export bool env_fclose(__fp fp);
__env_export int64_t env_ftell(__fp fp);
__env_export int64_t env_fflush(__fp fp);
__env_export int64_t env_fwrite(__fp fp, __ptr data, uint64_t size);
__env_export int64_t env_fread(__fp fp, __ptr buf, uint64_t size);
__env_export int64_t env_fseek(__fp fp, int64_t offset, int32_t whence);

__env_export bool env_make_path(const char* path);
__env_export bool env_find_path(const char* path);
__env_export bool env_find_file(const char* path);
__env_export bool env_remove_path(const char* path);
__env_export bool env_remove_file(const char* path);
__env_export bool env_move_path(const char* from, const char* to);

///////////////////////////////////////////////////////
///// 线程相关
///////////////////////////////////////////////////////
typedef void* env_thread_ptr;
typedef struct env_mutex env_mutex_t;
typedef __ptr(*env_thread_cb)(__ptr ctx);

__env_export int32_t env_thread_create(env_thread_ptr *tid, env_thread_cb cb, __ptr ctx);
__env_export void env_thread_destroy(env_thread_ptr *tid);
__env_export uint64_t env_thread_self();
__env_export uint64_t env_thread_id(env_thread_ptr thread_ptr);
__env_export void env_thread_sleep(uint64_t nano_seconds);

__env_export env_mutex_t* env_mutex_create(void);
__env_export void env_mutex_destroy(env_mutex_t **pp_mutex);
__env_export void env_mutex_lock(env_mutex_t *mutex);
__env_export void env_mutex_unlock(env_mutex_t *mutex);
__env_export void env_mutex_signal(env_mutex_t *mutex);
__env_export void env_mutex_broadcast(env_mutex_t *mutex);
__env_export void env_mutex_wait(env_mutex_t *mutex);
__env_export int32_t env_mutex_timedwait(env_mutex_t *mutex, uint64_t timeout);

typedef struct env_pipe env_pipe_t;
__env_export env_pipe_t* env_pipe_create(uint64_t len);
__env_export void env_pipe_destroy(env_pipe_t **pp_pipe);
__env_export uint64_t env_pipe_write(env_pipe_t *pipe, __ptr data, uint64_t len);
__env_export uint64_t env_pipe_read(env_pipe_t *pipe, __ptr buf, uint64_t len);
__env_export uint64_t env_pipe_readable(env_pipe_t *pipe);
__env_export uint64_t env_pipe_writable(env_pipe_t *pipe);
__env_export void env_pipe_stop(env_pipe_t *pipe);
__env_export void env_pipe_clear(env_pipe_t *pipe);

typedef struct linear_key_value_pair linekv_t;
typedef struct env_task_queue env_taskqueue_t;
typedef void (*env_task_ptr)(linekv_t* ctx);
typedef uint64_t (*env_timed_task_ptr)(linekv_t* ctx);
typedef linekv_t* (*env_sync_task_ptr)(linekv_t* ctx);

__env_export env_taskqueue_t* env_taskqueue_create();
__env_export void env_taskqueue_exit(env_taskqueue_t *tq);
__env_export void env_taskqueue_destroy(env_taskqueue_t **pp_tq);
__env_export void env_taskqueue_post_task(env_taskqueue_t *tq, linekv_t *lkv);
__env_export void env_taskqueue_insert_timed_task(env_taskqueue_t *tq, linekv_t *lkv);
__env_export linekv_t* env_taskqueue_run_sync_task(env_taskqueue_t *tq, linekv_t *lkv);

///////////////////////////////////////////////////////
///// 原子操作
///////////////////////////////////////////////////////
typedef uint64_t    __atombool;
__env_export uint64_t env_atomic_load(volatile uint64_t*);
__env_export void env_atomic_store(volatile uint64_t*, uint64_t);
__env_export uint64_t env_atomic_exchange(volatile uint64_t*, uint64_t);
__env_export bool env_atomic_compare_exchange(volatile uint64_t*, uint64_t*, uint64_t);
__env_export uint64_t env_atomic_increment(volatile uint64_t*);
__env_export uint64_t env_atomic_decrement(volatile uint64_t*);
__env_export uint64_t env_atomic_add(volatile uint64_t*, uint64_t);
__env_export uint64_t env_atomic_subtract(volatile uint64_t*, uint64_t);
__env_export uint64_t env_atomic_and(volatile uint64_t*, uint64_t);
__env_export uint64_t env_atomic_or(volatile uint64_t*, uint64_t);
__env_export uint64_t env_atomic_xor(volatile uint64_t*, uint64_t);
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

typedef void (*env_logger_cb) (int32_t level, const char *log);
__env_export int env_logger_start(const char *path, env_logger_cb cb);
__env_export void env_logger_stop();
__env_export void env_logger_printf(enum env_log_level level, const char *file, int line, const char *fmt, ...);

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
    } while (false)

///////////////////////////////////////////////////////
///// 堆栈回溯
///////////////////////////////////////////////////////
#ifdef ENV_HAVA_BACKTRACE
__env_export void env_backtrace_setup();
__env_export uint64_t env_backtrace(void** array, int32_t depth);
#endif

///////////////////////////////////////////////////////
///// 网络插口
///////////////////////////////////////////////////////
typedef int32_t __socket;
typedef struct sockaddr* __sockaddr_ptr;

typedef struct {
    union{
        struct{
            uint32_t ip;
            uint16_t port;
        };
        uint8_t addr[6];
    };
	// __sockaddr_ptr sa;
} __ipaddr;

__env_export __socket env_socket_open();
__env_export void env_socket_close(__socket sock);
__env_export int32_t env_socket_connect(__socket sock, __sockaddr_ptr addr);
__env_export int32_t env_socket_bind(__socket sock, __sockaddr_ptr addr);
__env_export int32_t env_socket_set_nonblock(__socket sock, int noblock);

__env_export __sockaddr_ptr env_socket_addr_create(char* ip, uint16_t port);
__env_export void env_socket_addr_copy(__sockaddr_ptr src, __sockaddr_ptr dst);
__env_export bool env_socket_addr_compare(__sockaddr_ptr a, __sockaddr_ptr b);
__env_export void env_socket_addr_get_ip(__sockaddr_ptr addr, __ipaddr *ip);

__env_export int32_t env_socket_send(__socket sock, const void* buf, uint64_t size);
__env_export int32_t env_socket_recv(__socket sock, void* buf, uint64_t size);
__env_export int32_t env_socket_sendto(__socket sock, const void* buf, uint64_t size, __sockaddr_ptr addr);
__env_export int32_t env_socket_recvfrom(__socket sock, void* buf, uint64_t size, __sockaddr_ptr addr);


#endif //__ENV_ENV_H__
