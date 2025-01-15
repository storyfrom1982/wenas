#include "xapi.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "uv.h"

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static uint64_t __xtime(void)
{
	uv_timespec64_t tp;
	uv_clock_gettime(UV_CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
}

///@return nanoseconds(relative time)
static uint64_t __xclock(void)
{
	uv_timespec64_t tp;
	uv_clock_gettime(UV_CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
}


static uint64_t __ex_strftime(char *buf, size_t size, uint64_t timepoint)
{
	time_t sec = (time_t)timepoint;
    struct tm t;
    localtime_r(&sec, &t);
    return strftime(buf, size, "%Y/%m/%d-%H:%M:%S", &t);
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////

static __xthread_ptr __xthread_create(void(*task_enter)(void*), void *ctx)
{
    uv_thread_t tid;
    int ret = uv_thread_create(&tid, task_enter, ctx);
    if (ret == 0){
        return (__xthread_ptr)tid;
    }
    return NULL;
}

static void __xthread_join(__xthread_ptr tid)
{
    uv_thread_t id = (uv_thread_t)tid;
    uv_thread_join(&id);
}

static __xthread_ptr __xthread_self()
{
    return (__xthread_ptr)uv_thread_self();
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef struct __xmutex {
    uv_cond_t cond[1];
    uv_mutex_t mutex[1];
}*__xmutex_ptr;


static __xmutex_ptr __xmutex_create()
{
    __xmutex_ptr ptr = (__xmutex_ptr)malloc(sizeof(struct __xmutex));
    __xcheck(ptr == NULL);
    int ret = uv_mutex_init(ptr->mutex);
    __xcheck(ret != 0);
    ret = uv_cond_init(ptr->cond);
    __xcheck(ret != 0);
    return ptr;

XClean:
    if (ptr){
        free(ptr);
    }
    return NULL;
}

static void __xmutex_free(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_mutex_destroy(ptr->mutex);
    uv_cond_destroy(ptr->cond);
    free(ptr);
XClean:
    return;
}

static void __xmutex_lock(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_mutex_lock(ptr->mutex);
XClean:
    return;
}

static int __xmutex_trylock(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    return uv_mutex_trylock(ptr->mutex) == 0;
XClean:
    return 0;
}

static void __xmutex_notify(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_signal(ptr->cond);
XClean:
    return;
}

static void __xmutex_broadcast(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_broadcast(ptr->cond);
XClean:
    return;
}

static void __xmutex_wait(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_wait(ptr->cond, ptr->mutex);
XClean:
    return;
}

static int __xmutex_timedwait(__xmutex_ptr ptr, uint64_t delay)
{
    __xcheck(ptr == NULL);
    if (uv_cond_timedwait(ptr->cond, ptr->mutex, delay) == UV_ETIMEDOUT){
        return __XAPI_TIMEDOUT;
    }
    return 0;
XClean:
    return -1;
}

static void __xmutex_unlock(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_mutex_unlock(ptr->mutex);
XClean:
    return;
}


#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>

__xdir_ptr __fs_dir_open(const char* path)
{
    uv_dir_t *dir = NULL;
    uv_fs_t opendir_req;
    __xcheck(uv_fs_opendir(NULL, &opendir_req, path, NULL) == UV_ENOTDIR);
    __xcheck(opendir_req.ptr == NULL);
    dir = (uv_dir_t*)opendir_req.ptr;
XClean:
    uv_fs_req_cleanup(&opendir_req);
    return dir;
}

int __fs_dir_read(__xdir_ptr xdir, int(*cb)(const char *name, void *ctx), void *ctx)
{
    int ret;
    uv_fs_t readdir_req;
    uv_dirent_t dirents[1];
    uv_dir_t *dir = (uv_dir_t*)xdir;
    dir->dirents = dirents;
    dir->nentries = (sizeof(dirents) / sizeof(dirents[0]));
    ret = uv_fs_readdir(NULL, &readdir_req, dir, NULL);
    if ((ret != 0)){
        cb(dirents[0].name, ctx);
    }
    uv_fs_req_cleanup(&readdir_req);
    return ret;
}

void __fs_dir_close(__xdir_ptr dir)
{
    uv_fs_t closedir_req;
    uv_fs_closedir(NULL, &closedir_req, (uv_dir_t*)dir, NULL);
    uv_fs_req_cleanup(&closedir_req);
}

static int __fs_dir_exist(const char* path)
{
    uv_fs_t stat_req;
    int r = uv_fs_stat(NULL, &stat_req, path, NULL);
    r = (r == 0 && ((uv_stat_t*)stat_req.ptr)->st_mode & S_IFDIR);
    uv_fs_req_cleanup(&stat_req);
    return r;
}

static int __fs_dir_maker(const char* path)
{
    uv_fs_t mkdir_req;
    int r = uv_fs_mkdir(NULL, &mkdir_req, path, 0755, NULL);
    uv_fs_req_cleanup(&mkdir_req);
    return r;
}

static int __fs_dir_remove(const char* path)
{
    uv_fs_t rmdir_req;
    int r = uv_fs_rmdir(NULL, &rmdir_req, path, NULL);
    uv_fs_req_cleanup(&rmdir_req);
    return r;
}

static __xfile_t __fs_file_open(const char* path, int flags, int mode)
{
    __xfile_t fd;
    uv_fs_t open_req;
    //flags UV_FS_O_WRONLY | UV_FS_O_APPEND, UV_FS_O_RDWR | UV_FS_O_CREAT
    // mode S_IWUSR | S_IRUSR
    if (flags == XAPI_FS_FLAG_WRITE){
        fd = uv_fs_open(NULL, &open_req, path, UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_APPEND, mode, NULL);
    }else if (flags == XAPI_FS_FLAG_READ){
        fd = uv_fs_open(NULL, &open_req, path, UV_FS_O_RDONLY, mode, NULL);
    }else if (flags == XAPI_FS_FLAG_CREATE){
        fd = uv_fs_open(NULL, &open_req, path, UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC, mode, NULL);
    }
    
    uv_fs_req_cleanup(&open_req);
    return fd;
}

static int __fs_file_close(__xfile_t fd)
{
    uv_fs_t close_req;
    int r = uv_fs_close(NULL, &close_req, fd, NULL);
    uv_fs_req_cleanup(&close_req);
    return r;
}

static int __fs_file_write(__xfile_t fd, void *data, unsigned int size)
{
    uv_fs_t write_req;
    uv_buf_t iov = uv_buf_init(data, size);
    int r = uv_fs_write(NULL, &write_req, fd, &iov, 1, -1, NULL);
    uv_fs_req_cleanup(&write_req);
    return r;
}

static int __fs_file_read(__xfile_t fd, void *buf, unsigned int size)
{
    uv_fs_t read_req;
    uv_buf_t iov = uv_buf_init(buf, size);
    int r = uv_fs_read(NULL, &read_req, fd, &iov, 1, -1, NULL);
    uv_fs_req_cleanup(&read_req);
    return r;
}

static int64_t __fs_file_tell(__xfile_t fd)
{
    return lseek(fd, 0, SEEK_CUR);
}

static int64_t __fs_file_seek(__xfile_t fd, int64_t offset, int32_t whence)
{
    return lseek(fd, offset, SEEK_SET);
}

static uint64_t __fs_file_size(const char* filename)
{
    uv_fs_t stat_req;
    uint64_t size = 0;
    int r = uv_fs_stat(NULL, &stat_req, filename, NULL);
    if (r == 0 && ((uv_stat_t*)stat_req.ptr)->st_mode & S_IFREG){
        size = ((uv_stat_t*)stat_req.ptr)->st_size;
    }
    uv_fs_req_cleanup(&stat_req);
    return size;
}

static int __fs_file_exist(const char* filepath)
{
    uv_fs_t stat_req;
    int r = uv_fs_stat(NULL, &stat_req, filepath, NULL);
    r = (r == 0 && ((uv_stat_t*)stat_req.ptr)->st_mode & S_IFREG);
    uv_fs_req_cleanup(&stat_req);
    return r;
}

static int __fs_file_remove(const char* path)
{
    uv_fs_t req;
    int r = uv_fs_unlink(NULL, &req, path, NULL);
    uv_fs_req_cleanup(&req);
    return r;
}

static int __fs_path_rename(const char* path, const char* new_path)
{
    uv_fs_t rename_req;
    int r = uv_fs_rename(NULL, &rename_req, path, new_path, NULL);
    uv_fs_req_cleanup(&rename_req);
    return r;
}

static int __fs_path_maker(const char* path)
{
    if (path == NULL || path[0] == '\0'){
        return -1;
    }

    int ret = 0;
    uint64_t len = strlen(path);

    if (!__fs_dir_exist(path)){
        char buf[PATH_MAX] = {0};
        snprintf(buf, PATH_MAX, "%s", path);
        if(buf[len - 1] == '/'){
            buf[len - 1] = '\0';
        }
        for(char *p = buf + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                if (!__fs_dir_exist(buf)){
                    ret = __fs_dir_maker(buf);
                    if (ret != 0){
                        break;
                    }
                }
                *p = '/';
            }
        }
        if (ret == 0){
            ret = __fs_dir_maker(buf);
        }
    }

    return ret;
}

int __fs_path_scanner(const char *path, int name_pos, int(*cb)(const char *name, int type, uint64_t size, void **ctx), void **ctx)
{
    uv_fs_t req;
    uv_dirent_t dent;
    __xcheck(uv_fs_scandir(NULL, &req, path, 0, NULL) < 0);
    __xcheck(cb(path + name_pos, 0, 0, ctx) != 0);

    while (uv_fs_scandir_next(&req, &dent) != UV_EOF){
        if (!((dent.name[0] == '.' && dent.name[1] == '\0') || (dent.name[0] == '.' && dent.name[1] == '.' && dent.name[2] == '\0'))){
            uint64_t size = 0;
            int full_path_len = strlen(path) + strlen(dent.name) + 2;
            char full_path[full_path_len];
            // printf("parent=%s###name=%s\n", path, dent.name);
            snprintf(full_path, full_path_len, "%s/%s\0", path, dent.name);
            if (dent.type == UV_DIRENT_FILE) {
                uv_fs_t stat_req;
                if (uv_fs_stat(NULL, &stat_req, full_path, NULL) == 0){
                    size = stat_req.statbuf.st_size;
                }
                uv_fs_req_cleanup(&stat_req);
                __xcheck(cb(full_path + name_pos, 1, size, ctx) != 0);
            }else if (dent.type == UV_DIRENT_DIR){
                __xcheck(__fs_path_scanner(full_path, name_pos, cb, ctx) != 0);
            }
        }
    }

    uv_fs_req_cleanup(&req);
    return 0;
XClean:
    uv_fs_req_cleanup(&req);
    return -1;
}

int __fs_path_cwd(char* buf, size_t *size)
{
    __xcheck(uv_cwd(buf, size) != 0);
    return 0;
XClean:
    return -1;
}

#include "sockutil.h"
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h> //struct hostent


struct __xipaddr {
    union{
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    };
    socklen_t addrlen;
#ifdef __XDEBUG__
    char ip[46];
    uint16_t port;
#endif
};

static int udp_open(int ipv6, int reuse, int nonblock)
{
    int sock;
    int opt;
    if (ipv6){
        __xcheck((sock = socket_udp_ipv6()) < 0);
    }else {
        __xcheck((sock = socket_udp()) < 0);
    }
    opt = nonblock;
    __xcheck(socket_setnonblock(sock, opt) != 0);
    opt = reuse;
    __xcheck(socket_setreuseport(sock, opt) != 0);
    __xcheck(socket_setreuseaddr(sock, opt) != 0);
    return sock;
XClean:
    return -1;
}

static int udp_close(int sock)
{
    return close(sock);
}

static int udp_bind_any(int sock, uint16_t port)
{
    return socket_bind_any(sock, port);
}

static int udp_bind_addr(int sock, __xipaddr_ptr ipaddr)
{
    return socket_bind(sock, (const struct sockaddr *)ipaddr, (socklen_t)ipaddr->addrlen);
}

static int udp_sendto(int sock, __xipaddr_ptr ipaddr, void *data, size_t size)
{
// #ifdef __XDEBUG__
//     static uint64_t send_number = 0, lost_number = 0;
//     send_number++;
//     uint64_t randtime = __xclock() / 1000000ULL;
//     if ((send_number & 0x03) == (randtime & 0x03)){
//         __xlogd("lost pack ...........%lu\n", size);
//         return size;
//     }
// #endif
    return socket_sendto(sock, data, size, 0, (struct sockaddr*)ipaddr, ipaddr->addrlen);
}

static int udp_local_send(int sock, __xipaddr_ptr ipaddr, void *data, size_t size)
{
    return socket_sendto(sock, data, size, 0, (struct sockaddr*)ipaddr, ipaddr->addrlen);
}

static int udp_recvfrom(int sock, __xipaddr_ptr ipaddr, void *buf, size_t size)
{
    return socket_recvfrom(sock, buf, size, 0, (struct sockaddr*)ipaddr, &ipaddr->addrlen);
}

static int udp_listen(int sock[2], uint64_t microseconds)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock[0], &fds);
    FD_SET(sock[1], &fds);
    struct timeval timer;
    timer.tv_sec = microseconds / 1000000UL;
    timer.tv_usec = microseconds & 1000000UL;
    return socket_select(sock[1] > sock[0] ? sock[1] + 1 : sock[0] + 1, &fds, NULL, NULL, &timer);
}

int udp_addrinfo(char ip[__XAPI_IP_STR_LEN], const char *hostname) {

    int status;
    struct addrinfo *res = NULL;
    __xcheck((status = socket_getaddrinfo(hostname, NULL, NULL, &res)) != 0);
    __xcheck(socket_addr_to(res->ai_addr, res->ai_addrlen, ip, NULL) != 0);
    freeaddrinfo(res);
    return 0;

XClean:
    if (res){
        freeaddrinfo(res);
    }
    return -1;
}

int udp_addr_to_host(const __xipaddr_ptr addr, char* ip, uint16_t* port) {
    return socket_addr_to((const struct sockaddr*)addr, addr->addrlen, ip, port);
}

__xipaddr_ptr udp_addr_dump(const __xipaddr_ptr addr)
{
    __xipaddr_ptr res = (__xipaddr_ptr)malloc(sizeof(struct __xipaddr));
    __xcheck(addr == NULL);
    __xcheck(res == NULL);
    *res = *addr;
    return res;
XClean:
    return NULL;
}

int udp_addr_is_ipv6(__xipaddr_ptr addr)
{
    return ((struct sockaddr*)addr)->sa_family == AF_INET6;
}

__xipaddr_ptr udp_any_to_addr(int ipv6, uint16_t port)
{
    __xipaddr_ptr ipaddr = (__xipaddr_ptr)malloc(sizeof(struct __xipaddr));
    __xcheck(ipaddr == NULL);
    if (ipv6){
        ipaddr->v6.sin6_family = AF_INET6;
        ipaddr->v6.sin6_port = htons(port);
        ipaddr->v6.sin6_addr = in6addr_any;
        ipaddr->addrlen = sizeof(struct sockaddr_in6);
    }else {
        ipaddr->v4.sin_family = AF_INET;
        ipaddr->v4.sin_port = htons(port);
        ipaddr->v4.sin_addr.s_addr = INADDR_ANY;
        ipaddr->addrlen = sizeof(struct sockaddr_in);
    }
    return ipaddr;

XClean:
    if (ipaddr != NULL){
        free(ipaddr);
    }
    return NULL;
}

__xipaddr_ptr udp_host_to_addr(const char *ip, uint16_t port)
{
    __xipaddr_ptr ipaddr = NULL;
    __xcheck(ip == NULL);
    ipaddr = (__xipaddr_ptr)malloc(sizeof(struct __xipaddr));
    __xcheck(ipaddr == NULL);
    memset(ipaddr, 0, sizeof(struct __xipaddr));
    __xcheck(socket_addr_from((struct sockaddr_storage*)ipaddr, &ipaddr->addrlen, ip, port) != 0);
    return ipaddr;

XClean:
    if (ipaddr != NULL){
        free(ipaddr);
    }
    return NULL;
}

const char* udp_addr_ip(const __xipaddr_ptr addr)
{
    if (addr->port == 0){
        udp_addr_to_host(addr, addr->ip, &addr->port);
    }
    return addr->ip;
}

uint16_t udp_addr_port(const __xipaddr_ptr addr)
{
    if (addr->port == 0){
        udp_addr_to_host(addr, addr->ip, &addr->port);
    }
    return addr->port;
}


#include <sys/mman.h>

void* __ex_mmap(void *addr, size_t len)
{
    return mmap(addr, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

int __ex_munmap(void *addr, size_t len)
{
    return munmap(addr, len);
}


#ifdef UNWIND_BACKTRACE

#define __USE_GNU
#include <dlfcn.h>
#include <stdio.h>
#include <unwind.h>

struct backtrace_stack {
    void** head;
    void** end;
};

static inline _Unwind_Reason_Code __unwind_backtrace_callback(struct _Unwind_Context* unwind_context, void* vp)
{
    struct backtrace_stack* stack = (struct backtrace_stack*)vp;
    if (stack->head == stack->end) {
		//数组已满
        return _URC_END_OF_STACK;
    }
    *stack->head = (void*)_Unwind_GetIP(unwind_context);
    if (*stack->head == NULL) {
        return _URC_END_OF_STACK;
    }
    ++stack->head;
    return _URC_NO_REASON;
}

static int64_t __ex_unwind_backtrace(void** stacks, int depth)
{
    struct backtrace_stack stack = {0};
    stack.head = &stacks[0];
    stack.end = &stacks[0] + (depth - 2);
    _Unwind_Backtrace(__unwind_backtrace_callback, &stack);
    return stack.head - &stacks[0];
}

static int __ex_dladdr(const void* addr, void *buf, size_t size)
{
    Dl_info info= {0};
    if (dladdr(addr, &info) && info.dli_sname) {
        if (strlen(info.dli_sname) > size){
            return -1;
        }
        return snprintf(buf, size, "@%s ", info.dli_sname);
    }
    return 0;
}

#endif //#ifdef XMALLOC_BACKTRACE



struct __xapi_enter posix_api_enter = {

    .time = __xtime,
    .clock = __xclock,
    .strftime = __ex_strftime,
    .snprintf = snprintf,

    .thread_create = __xthread_create,
    .thread_join = __xthread_join,
    .thread_self = __xthread_self,

    .mutex_create = __xmutex_create,
    .mutex_free = __xmutex_free,
    .mutex_lock = __xmutex_lock,
    .mutex_trylock = __xmutex_trylock,
    .mutex_unlock = __xmutex_unlock,
    .mutex_wait = __xmutex_wait,
    .mutex_timedwait = __xmutex_timedwait,
    .mutex_notify = __xmutex_notify,
    .mutex_broadcast = __xmutex_broadcast,

    .udp_open = udp_open,
    .udp_close = udp_close,
    .udp_bind = udp_bind_any,
    .udp_bind_addr = udp_bind_addr,
    .udp_sendto = udp_sendto,
    .udp_local_send = udp_local_send,
    .udp_recvfrom = udp_recvfrom,
    .udp_listen = udp_listen,
    .udp_any_to_addr = udp_any_to_addr,
    .udp_host_to_addr = udp_host_to_addr,
    .udp_addr_to_host = udp_addr_to_host,
    .udp_addr_is_ipv6 = udp_addr_is_ipv6,
    .udp_addrinfo = udp_addrinfo,
    .udp_addr_dump = udp_addr_dump,
    .udp_addr_ip = udp_addr_ip,
    .udp_addr_port = udp_addr_port,

    .fs_dir_open = __fs_dir_open,
    .fs_dir_read = __fs_dir_read,
    .fs_dir_close = __fs_dir_close,
    .fs_dir_exist = __fs_dir_exist,
    .fs_dir_maker = __fs_dir_maker,
    .fs_dir_remove = __fs_dir_remove,
    .fs_file_open = __fs_file_open,
    .fs_file_close = __fs_file_close,
    .fs_file_tell = __fs_file_tell,
    .fs_file_write = __fs_file_write,
    .fs_file_read = __fs_file_read,
    .fs_file_seek = __fs_file_seek,
    .fs_file_size = __fs_file_size,
    .fs_file_exist = __fs_file_exist,
    .fs_file_remove = __fs_file_remove,
    
    .fs_path_maker = __fs_path_maker,
    .fs_path_scanner = __fs_path_scanner,
    .fs_path_rename = __fs_path_rename,
    .fs_path_cwd = __fs_path_cwd,
    
    .mmap = __ex_mmap,
    .munmap = __ex_munmap,

#ifdef UNWIND_BACKTRACE
    .backtrace = __ex_unwind_backtrace,
    .dladdr = __ex_dladdr,
#endif    
};

__xapi_enter_ptr __xapi = &posix_api_enter;
