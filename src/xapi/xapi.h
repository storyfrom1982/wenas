#ifndef __XAPI_H__
#define __XAPI_H__

#if !defined(__linux__) && !defined(__APPLE__)
#error Not yet adapted to the environment
#endif


#include <stddef.h>
#include <assert.h>

#include "xatom.h"


#define MILLI_SECONDS       1000ULL
#define MICRO_SECONDS       1000000ULL
#define NANO_SECONDS        1000000000ULL

#define __XAPI_TIMEDOUT             1
#define __XAPI_MAP_FAILED           ((void *) -1)


typedef void* __xmutex_ptr;
typedef void* __xprocess_ptr;
typedef void* __xfile_ptr;


typedef struct __xipaddr {
    uint16_t family;
    uint16_t port;
    uint32_t ip;
    uint8_t zero[8];
    uint32_t keylen;
    uint32_t addrlen;
}*__xipaddr_ptr;


typedef struct __xapi_enter {

/////////////////////////////////////////////////////////
///// 时间相关
/////////////////////////////////////////////////////////    

    uint64_t (*time)(void);
    uint64_t (*clock)(void);
    uint64_t (*strftime)(char *buf, size_t size, uint64_t point);

///////////////////////////////////////////////////////
///// 并发任务
///////////////////////////////////////////////////////

    __xprocess_ptr (*process_create)(void*(*task_enter)(void*), void *ctx);
    void (*process_free)(__xprocess_ptr pid);
    __xprocess_ptr (*process_self)();

    __xmutex_ptr (*mutex_create)();
    void (*mutex_free)(__xmutex_ptr mutex);
    void (*mutex_lock)(__xmutex_ptr mutex);
    bool (*mutex_trylock)(__xmutex_ptr mutex);
    void (*mutex_unlock)(__xmutex_ptr mutex);
    void (*mutex_wait)(__xmutex_ptr mutex);
    int (*mutex_timedwait)(__xmutex_ptr mutex, uint64_t delay);
    void (*mutex_notify)(__xmutex_ptr mutex);
    void (*mutex_broadcast)(__xmutex_ptr mutex);

///////////////////////////////////////////////////////
///// 网络接口
///////////////////////////////////////////////////////

    int (*udp_open)();
    int (*udp_close)(int sock);
    int (*udp_bind)(int sock, __xipaddr_ptr ipaddr);
    int (*udp_sendto)(int sock, __xipaddr_ptr ipaddr, void *data, size_t size);
    int (*udp_recvfrom)(int sock, __xipaddr_ptr ipaddr, void *buf, size_t size);
    int (*udp_listen)(int sock);
    int (*udp_addrinfo)(char* ip_str, size_t ip_str_len, const char *name);
    int (*udp_hostbyname)(char* ip_str, size_t ip_str_len, const char *name);
    bool (*udp_host_to_ipaddr)(const char *ip, uint16_t port, __xipaddr_ptr addr);
    bool (*udp_ipaddr_to_host)(const __xipaddr_ptr addr, char* ip_str, size_t ip_str_len, uint16_t* port);

///////////////////////////////////////////////////////
///// 文件存储
///////////////////////////////////////////////////////

    bool (*make_path)(const char* path);
    bool (*check_path)(const char* path);
    bool (*check_file)(const char* path);
    bool (*delete_path)(const char* path);
    bool (*delete_file)(const char* path);
    bool (*move_path)(const char* from, const char* to);

    __xfile_ptr (*fopen)(const char* path, const char* mode);
    int (*fclose)(__xfile_ptr);
    int64_t (*ftell)(__xfile_ptr);
    int64_t (*fflush)(__xfile_ptr);
    int64_t (*fwrite)(__xfile_ptr, void* data, uint64_t size);
    int64_t (*fread)(__xfile_ptr, void* buf, uint64_t size);
    int64_t (*fseek)(__xfile_ptr, int64_t offset, int32_t whence);

///////////////////////////////////////////////////////
///// 内存管理
///////////////////////////////////////////////////////

    void* (*mmap)(void *addr, size_t len);
    int (*munmap)(void *addr, size_t len);

///////////////////////////////////////////////////////
///// 调用堆栈回溯
///////////////////////////////////////////////////////
#ifdef UNWIND_BACKTRACE
    int64_t (*backtrace)(void** stacks, int depth);
    int (*dladdr)(const void* addr, void *buf, size_t size);
#endif

}*__xapi_enter_ptr;

extern __xapi_enter_ptr __xapi;


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
extern int xlog_recorder_open(const char *path, __xlog_cb cb);
extern void xlog_recorder_close();
extern void __xlog_debug(const char *fmt, ...);
extern void __xlog_printf(enum __xlog_level level, const char *file, int line, const char *fmt, ...);

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

#define __xbreak(condition) \
    do { \
        if ((condition)) { \
            __xloge("ERROR: %s\n", #condition); \
            goto Clean; \
        } \
    } while (0)

#define __xcheck(condition) \
    do { \
        if ((condition)) { \
            __xloge("ERROR: %s\n", #condition); \
            goto XClean; \
        } \
    } while (0)



#endif //__XAPI_H__
