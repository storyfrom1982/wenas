#ifndef __XAPI_H__
#define __XAPI_H__

// #if !defined(__linux__) && !defined(__APPLE__)
// #error Not yet adapted to the environment
// #endif


// #include <stddef.h>
// #include <assert.h>

#include "xatom.h"


#define MILLI_SECONDS       1000ULL
#define MICRO_SECONDS       1000000ULL
#define NANO_SECONDS        1000000000ULL

#define XAPI_FS_FLAG_READ       0x01
#define XAPI_FS_FLAG_WRITE      0x02
#define XAPI_FS_FLAG_CREATE     0x04

#define __XAPI_TIMEDOUT         1
#define __XAPI_MAP_FAILED       ((void *) -1)
#define __XAPI_IP_STR_LEN       46

#define __XAPI_FS_ITEM_TYPE_DIR      0
#define __XAPI_FS_ITEM_TYPE_FILE     1

typedef struct __xfs_item {
    int type;
    int path_len;
    const char *path;
    uint64_t size;
}*__xfs_item_ptr;

typedef int __xfile_t;
typedef void* __xdir_ptr;
typedef struct __xfs_enter* __xfs_enter_ptr;
typedef struct __xfs_scanner* __xfs_scanner_ptr;
typedef void* __xthread_ptr;
typedef struct __xmutex* __xmutex_ptr;
typedef struct __xipaddr* __xipaddr_ptr;


typedef struct __xapi_enter {

/////////////////////////////////////////////////////////
///// 时间相关
/////////////////////////////////////////////////////////    

    uint64_t (*time)(void);
    uint64_t (*clock)(void);
    uint64_t (*strftime)(char *buf, size_t size, uint64_t point);
    int (*snprintf)(char *, size_t, const char *, ...);

///////////////////////////////////////////////////////
///// 并发任务
///////////////////////////////////////////////////////

    __xthread_ptr (*thread_create)(void(*task_enter)(void*), void *ctx);
    void (*thread_join)(__xthread_ptr pid);
    __xthread_ptr (*thread_self)();

    __xmutex_ptr (*mutex_create)();
    void (*mutex_free)(__xmutex_ptr mutex);
    void (*mutex_lock)(__xmutex_ptr mutex);
    int (*mutex_trylock)(__xmutex_ptr mutex);
    void (*mutex_unlock)(__xmutex_ptr mutex);
    void (*mutex_wait)(__xmutex_ptr mutex);
    int (*mutex_timedwait)(__xmutex_ptr mutex, uint64_t delay);
    void (*mutex_notify)(__xmutex_ptr mutex);
    void (*mutex_broadcast)(__xmutex_ptr mutex);

///////////////////////////////////////////////////////
///// 网络接口
///////////////////////////////////////////////////////

    int (*udp_open)(int ipv6, int reuse, int nonblock);
    int (*udp_close)(int sock);
    int (*udp_bind)(int sock, uint16_t port);
    int (*udp_bind_addr)(int sock, __xipaddr_ptr ipaddr);
    int (*udp_sendto)(int sock, __xipaddr_ptr ipaddr, void *data, size_t size);
    int (*udp_local_send)(int sock, __xipaddr_ptr ipaddr, void *data, size_t size);
    int (*udp_recvfrom)(int sock, __xipaddr_ptr ipaddr, void *buf, size_t size);
    int (*udp_listen)(int sock[2], uint64_t microseconds);
    int (*udp_addrinfo)(char ip[__XAPI_IP_STR_LEN], const char *name);
    int (*udp_addr_to_host)(const __xipaddr_ptr addr, char* ip, uint16_t* port);
    int (*udp_addr_is_ipv6)(const __xipaddr_ptr addr);
    __xipaddr_ptr (*udp_any_to_addr)(int ipv6, uint16_t port);
    __xipaddr_ptr (*udp_host_to_addr)(const char *ip, uint16_t port);
    __xipaddr_ptr (*udp_addr_dump)(const __xipaddr_ptr addr);
    const char* (*udp_addr_ip)(const __xipaddr_ptr addr);
    uint16_t (*udp_addr_port)(const __xipaddr_ptr addr);

///////////////////////////////////////////////////////
///// 文件系统
///////////////////////////////////////////////////////

    __xdir_ptr (*fs_dir_open)(const char* path);
    int (*fs_dir_read)(__xdir_ptr dir, int(*cb)(const char *name, void *ctx), void *ctx);
    void (*fs_dir_close)(__xdir_ptr dir);
    int (*fs_dir_exist)(const char* path);
    int (*fs_dir_maker)(const char* path);
    int (*fs_dir_remove)(const char* path);
    
    __xfile_t (*fs_file_open)(const char* path, int flags, int mode);
    int (*fs_file_close)(__xfile_t);
    int (*fs_file_write)(__xfile_t, void* data, unsigned int size);
    int (*fs_file_read)(__xfile_t, void* buf, unsigned int size);
    int64_t (*fs_file_tell)(__xfile_t);
    int64_t (*fs_file_seek)(__xfile_t, int64_t offset, int whence);
    uint64_t (*fs_file_size)(const char* path);
    int (*fs_file_exist)(const char* path);
    int (*fs_file_remove)(const char* path);

    int (*fs_path_maker)(const char* path);
    int (*fs_path_scanner)(const char* path, int name_pos, int(*cb)(const char *name, int type, uint64_t size, void **ctx), void **ctx);
    int (*fs_path_rename)(const char* path, const char* new_path);
    int (*fs_path_cwd)(char* buf, size_t *size);

    __xfs_scanner_ptr (*fs_scanner_open)(const char *path);
    void (*fs_scanner_close)(__xfs_scanner_ptr scanner);
    __xfs_item_ptr (*fs_scanner_read)(__xfs_scanner_ptr scanner);

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


#define __xcheck(condition) \
    do { \
        if ((condition)) { \
            __xloge("ERROR: %s\n", #condition); \
            goto XClean; \
        } \
    } while (0)



#endif //__XAPI_H__
