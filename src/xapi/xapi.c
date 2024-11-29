#include "xapi.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "uv.h"

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static uint64_t __unix_time(void)
{
	uv_timespec64_t tp;
	uv_clock_gettime(UV_CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
}

///@return nanoseconds(relative time)
static uint64_t __unix_clock(void)
{
	uv_timespec64_t tp;
	uv_clock_gettime(UV_CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
}


static uint64_t __unix_strftime(char *buf, size_t size, uint64_t timepoint)
{
	time_t sec = (time_t)timepoint;
    struct tm t;
    localtime_r(&sec, &t);
    return strftime(buf, size, "%Y/%m/%d-%H:%M:%S", &t);
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////

static __xprocess_ptr __posix_thread_create(void*(*task_enter)(void*), void *ctx)
{
    // pthread_t tid;
    // pthread_attr_t attr;
    // pthread_attr_init(&attr);
    // pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);    
    // int ret = pthread_create(&tid, &attr, task_enter, ctx);

    uv_thread_t tid;
    int ret = uv_thread_create(&tid, task_enter, ctx);
    if (ret == 0){
        return (__xprocess_ptr)tid;
    }
    return NULL;
}

static void __posix_thread_free(__xprocess_ptr pid)
{
    // pthread_join((pthread_t)pid, NULL);
    uv_thread_join((uv_thread_t)pid);
}

static __xprocess_ptr __posix_thread_self()
{
    // return (__xprocess_ptr)pthread_self();
    return (__xprocess_ptr)uv_thread_self();
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef struct xmutex {
    // pthread_cond_t cond[1];
    // pthread_mutex_t mutex[1];
    uv_cond_t cond[1];
    uv_mutex_t mutex[1];
}*__xmutex_ptr;


static __xmutex_ptr __posix_mutex_create()
{
    __xmutex_ptr ptr = (__xmutex_ptr)malloc(sizeof(struct xmutex));
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

static void __posix_mutex_free(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_mutex_destroy(ptr->mutex);
    uv_cond_destroy(ptr->cond);
    free(ptr);
XClean:
    return;
}

static void __posix_mutex_lock(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_mutex_lock(ptr->mutex);
XClean:
    return;
}

bool __posix_mutex_trylock(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    return uv_mutex_trylock(ptr->mutex) == 0;
XClean:
    return false;
}

static void __posix_mutex_notify(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_signal(ptr->cond);
XClean:
    return;
}

static void __posix_mutex_broadcast(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_broadcast(ptr->cond);
XClean:
    return;
}

static void __posix_mutex_wait(__xmutex_ptr ptr)
{
    __xcheck(ptr == NULL);
    uv_cond_wait(ptr->cond, ptr->mutex);
XClean:
    return;
}

static int __posix_mutex_timedwait(__xmutex_ptr ptr, uint64_t delay)
{
    __xcheck(ptr == NULL);
    if (uv_cond_timedwait(ptr->cond, ptr->mutex, delay) == UV_ETIMEDOUT){
        return __XAPI_TIMEDOUT;
    }
    return 0;
XClean:
    return -1;
}

static void __posix_mutex_unlock(__xmutex_ptr ptr)
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


static __xfile_ptr __ex_fopen(const char* path, const char* mode)
{
	return fopen(path, mode);
}

static int __ex_fclose(__xfile_ptr fp)
{
	return fclose((FILE*)fp) == 0 ? true : false;
}

static int64_t __ex_ftell(__xfile_ptr fp)
{
	return ftello((FILE*)fp);
}

static int64_t __ex_fflush(__xfile_ptr fp)
{
	return fflush((FILE*)fp);
}

static int64_t __ex_fwrite(__xfile_ptr fp, void *data, uint64_t size)
{
	return fwrite(data, 1, size, (FILE*)fp);
}

static int64_t __ex_fread(__xfile_ptr fp, void *buf, uint64_t size)
{
	return fread(buf, 1, size, (FILE*)fp);
}

static int64_t __ex_fseek(__xfile_ptr fp, int64_t offset, int32_t whence)
{
	return fseeko((FILE*)fp, offset, whence);
}


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

static bool __ex_check_file(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFREG)) ? true : false;
}

/// get file size in bytes
/// return file size
static uint64_t __ex_file_size(const char* filename)
{
	struct stat st;
	if (0 == stat(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
}

static bool __ex_check_path(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? true : false;
}

static bool __ex_mkdir(const char* path)
{
	int r = mkdir(path, 0777);
	return 0 == r ? true : false;
}

static bool __ex_delete_path(const char* path)
{
	int r = rmdir(path);
	return 0 == r ? true : false;
}

static bool __ex_realpath(const char* path, char resolved_path[PATH_MAX])
{
	char* p = realpath(path, resolved_path);
	return p ? true : false;
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
static bool __ex_delete_file(const char* path)
{
	int r = remove(path);
	return 0 == r ? true : false;
}

/// change the name or location of a file
/// 0-ok, other-error
static bool __ex_move_path(const char* from, const char* to)
{
	int r = rename(from, to);
	return 0 == r ? true : false;
}

static bool __ex_make_path(const char* path)
{
    if (path == NULL || path[0] == '\0'){
        return false;
    }

    bool ret = true;
    uint64_t len = strlen(path);

    if (!__ex_check_path(path)){
        char buf[PATH_MAX] = {0};
        snprintf(buf, PATH_MAX, "%s", path);
        if(buf[len - 1] == '/'){
            buf[len - 1] = '\0';
        }
        for(char *p = buf + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                if (!__ex_check_path(buf)){
                    ret = __ex_mkdir(buf);
                    if (!ret){
                        break;
                    }
                }
                *p = '/';
            }
        }
        if (ret){
            ret = __ex_mkdir(buf);
        }
    }

    return ret;
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
};

static int udp_open(int ipv6)
{
    int sock;
    int opt = 1;
    if (ipv6){
        __xcheck((sock = socket_udp_ipv6()) < 0);
    }else {
        __xcheck((sock = socket_udp()) < 0);
    }
    __xcheck(socket_setreuseport(sock, opt) != 0);
    __xcheck(socket_setreuseaddr(sock, opt) != 0);
    __xcheck(socket_setnonblock(sock, opt) != 0);

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
#ifdef __XDEBUG__
    static uint64_t send_number = 0, lost_number = 0;
    send_number++;
    uint64_t randtime = __unix_clock() / 1000000ULL;
    if ((send_number & 0x03) == (randtime & 0x03)){
        __xlogd("lost pack ...........\n");
        return size;
    }
#endif
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

bool udp_addr_is_ipv6(__xipaddr_ptr addr)
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

    .time = __unix_time,
    .clock = __unix_clock,
    .strftime = __unix_strftime,
    .snprintf = snprintf,

    .process_create = __posix_thread_create,
    .process_free = __posix_thread_free,
    .process_self = __posix_thread_self,

    .mutex_create = __posix_mutex_create,
    .mutex_free = __posix_mutex_free,
    .mutex_lock = __posix_mutex_lock,
    .mutex_trylock = __posix_mutex_trylock,
    .mutex_unlock = __posix_mutex_unlock,
    .mutex_wait = __posix_mutex_wait,
    .mutex_timedwait = __posix_mutex_timedwait,
    .mutex_notify = __posix_mutex_notify,
    .mutex_broadcast = __posix_mutex_broadcast,

    .udp_open = udp_open,
    .udp_close = udp_close,
    .udp_bind = udp_bind_any,
    .udp_bind_addr = udp_bind_addr,
    .udp_sendto = udp_sendto,
    .udp_recvfrom = udp_recvfrom,
    .udp_listen = udp_listen,
    .udp_any_to_addr = udp_any_to_addr,
    .udp_host_to_addr = udp_host_to_addr,
    .udp_addr_to_host = udp_addr_to_host,
    .udp_addr_is_ipv6 = udp_addr_is_ipv6,
    .udp_addrinfo = udp_addrinfo,

    .make_path = __ex_make_path,
    .check_path = __ex_check_path,
    .delete_path = __ex_delete_path,
    .move_path = __ex_move_path,
    .check_file = __ex_check_file,
    .delete_file = __ex_delete_file,

    .fopen = __ex_fopen,
    .fclose = __ex_fclose,
    .ftell = __ex_ftell,
    .fflush = __ex_fflush,
    .fwrite = __ex_fwrite,
    .fread = __ex_fread,
    .fseek = __ex_fseek,
    
    .mmap = __ex_mmap,
    .munmap = __ex_munmap,

#ifdef UNWIND_BACKTRACE
    .backtrace = __ex_unwind_backtrace,
    .dladdr = __ex_dladdr,
#endif    
};

__xapi_enter_ptr __xapi = &posix_api_enter;
