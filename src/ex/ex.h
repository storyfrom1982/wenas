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

#include <assert.h>
#include <errno.h>

#include "xatom.h"


///////////////////////////////////////////////////////
///// 线程间通信
///////////////////////////////////////////////////////


#define __XAPI_TIMEDOUT           1


typedef void* __xmutex_ptr;
typedef void* __xprocess_ptr;

typedef struct __xapi_enter {

    uint64_t (*time)(void);
    uint64_t (*clock)(void);

    __xprocess_ptr (*process_create)(void*(*task_enter)(void*), void *ctx);
    void (*process_free)(__xprocess_ptr pid);
    __xprocess_ptr (*process_self)();

    __xmutex_ptr (*mutex_create)();
    void (*mutex_free)(__xmutex_ptr mutex);
    void (*mutex_lock)(__xmutex_ptr mutex);
    void (*mutex_unlock)(__xmutex_ptr mutex);
    void (*mutex_wait)(__xmutex_ptr mutex);
    int (*mutex_timedwait)(__xmutex_ptr mutex, uint64_t delay);
    void (*mutex_notify)(__xmutex_ptr mutex);
    void (*mutex_broadcast)(__xmutex_ptr mutex);

}*__xapi_enter_ptr;

extern __xapi_enter_ptr __xapi;


/////////////////////////////////////////////////////////
/////// 可变类型指针
/////////////////////////////////////////////////////////
// typedef void*                   __ptr;

/////////////////////////////////////////////////////////
/////// 时间相关
/////////////////////////////////////////////////////////

#define MILLI_SECONDS       1000ULL
#define MICRO_SECONDS       1000000ULL
#define NANO_SECONDS        1000000000ULL


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
__ex_export int64_t __ex_fwrite(__ex_fp fp, void* data, uint64_t size);
__ex_export int64_t __ex_fread(__ex_fp fp, void* buf, uint64_t size);
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
typedef struct xmaker* xmaker_ptr;

// // 这个缓冲区支持线程间通信，一对一，一对多，多对一，多对多
// typedef struct xpipe* xpipe_ptr;
// __ex_export xpipe_ptr xpipe_create(uint64_t len);
// __ex_export void xpipe_free(xpipe_ptr *pptr);
// __ex_export uint64_t xpipe_write(xpipe_ptr ptr, void* data, uint64_t len);
// __ex_export uint64_t xpipe_read(xpipe_ptr ptr, void* buf, uint64_t len);
// __ex_export uint64_t xpipe_readable(xpipe_ptr ptr);
// __ex_export uint64_t xpipe_writable(xpipe_ptr ptr);
// __ex_export void xpipe_clear(xpipe_ptr ptr);
// __ex_export void xpipe_break(xpipe_ptr ptr);

// // 这个缓冲区只能支持一对一线程间通信，一进一出
// typedef struct xbuf* xbuf_ptr;
// __ex_export xbuf_ptr xbuf_create(uint64_t len);
// __ex_export void xbuf_free(xbuf_ptr *pptr);
// __ex_export uint64_t xbuf_readable(xbuf_ptr ptr);
// __ex_export uint64_t xbuf_writable(xbuf_ptr ptr);
// __ex_export void xbuf_clear(xbuf_ptr ptr);
// __ex_export void xbuf_break(xbuf_ptr ptr);
// __ex_export xmaker_ptr xbuf_hold_writer(xbuf_ptr ptr);
// __ex_export void xbuf_update_writer(xbuf_ptr ptr);
// __ex_export xmaker_ptr xbuf_hold_reader(xbuf_ptr ptr);
// __ex_export void xbuf_update_reader(xbuf_ptr ptr);

__ex_export void __ex_backtrace_setup();

///////////////////////////////////////////////////////
///// 日志存储
///////////////////////////////////////////////////////
enum __xlog_level 
{
        __XLOG_LEVEL_DEBUG,
        __XLOG_LEVEL_INFO,
        __XLOG_LEVEL_ERROR
};

typedef void (*__xlog_cb) (int32_t level, const char *log);
__ex_export int __xlog_open(const char *path, __xlog_cb cb);
__ex_export void __xlog_close();
__ex_export void __xlog_printf(enum __xlog_level level, const char *file, int line, const char *fmt, ...);

#define __xlogi(__FORMAT__, ...) \
    __xlog_printf(__XLOG_LEVEL_INFO, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#define __xloge(__FORMAT__, ...) \
    __xlog_printf(__XLOG_LEVEL_ERROR, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#ifdef __XDEBUG__
    #define __xlogd(__FORMAT__, ...) \
        __xlog_printf(__XLOG_LEVEL_DEBUG, __FILE__, __LINE__, __FORMAT__, ##__VA_ARGS__)
#else
    #define __xlogd(__FORMAT__, ...) \
        do {} while(0)
#endif

#define __xcheck(condition) \
    do { \
        if (!(condition)) { \
            __xloge("CHECK FAILED: %s, %s\n", #condition, strerror(errno)); \
            goto Clean; \
        } \
    } while (0)

#define __xbreak(condition)   assert((condition))

///////////////////////////////////////////////////////
///// 线程间通信
///////////////////////////////////////////////////////

typedef struct ex_task* __ex_task_ptr;
typedef void (*__ex_task_func)(xmaker_ptr ctx);
__ex_export __ex_task_ptr __ex_task_create();
__ex_export void __ex_task_free(__ex_task_ptr *pptr);
__ex_export xmaker_ptr __ex_task_hold_pusher(__ex_task_ptr task);
__ex_export void __ex_task_update_pusher(__ex_task_ptr task);
__ex_export __ex_task_ptr __ex_task_run(__ex_task_func func, void *ctx);


#ifdef __cplusplus
}
#endif

#endif //__EX_EX_H__
