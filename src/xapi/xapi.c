#include "xapi.h"

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static uint64_t __unix_time(void)
{
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

///@return nanoseconds(relative time)
static uint64_t __unix_clock(void)
{
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
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
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);    
    int ret = pthread_create(&tid, &attr, task_enter, ctx);
    if (ret == 0){
        return (__xprocess_ptr)tid;
    }
    return NULL;
}

static void __posix_thread_free(__xprocess_ptr pid)
{
    pthread_join((pthread_t)pid, NULL);
}

static __xprocess_ptr __posix_thread_self()
{
    return (__xprocess_ptr)pthread_self();
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef struct posix_mutex {
    pthread_cond_t cond[1];
    pthread_mutex_t mutex[1];
}*__posix_mutex_ptr;


static __xmutex_ptr __posix_mutex_create()
{
    __posix_mutex_ptr ptr = (__posix_mutex_ptr)malloc(sizeof(struct posix_mutex));
    assert(ptr);
    __xlogd("__posix_mutex_create ptr == 0x%X\n", ((__posix_mutex_ptr)ptr));
    int ret = pthread_mutex_init(ptr->mutex, NULL);
    __xlogd("create mutex == 0x%X\n", ptr->mutex);
    assert(ret == 0);
    ret = pthread_cond_init(ptr->cond, NULL);
    assert(ret == 0);
    return (__xmutex_ptr)ptr;
}

static void __posix_mutex_free(__xmutex_ptr ptr)
{
    int ret;
    assert(ptr);
    __xlogd("free mutex == 0x%X\n", ((__posix_mutex_ptr)ptr)->mutex);
    ret = pthread_mutex_destroy(((__posix_mutex_ptr)ptr)->mutex);
    assert(ret == 0);
    ret = pthread_cond_destroy(((__posix_mutex_ptr)ptr)->cond);
    assert(ret == 0);
    __xlogd("__posix_mutex_free ptr == 0x%X\n", ((__posix_mutex_ptr)ptr));
    free(ptr);
}

static void __posix_mutex_lock(__xmutex_ptr ptr)
{
    assert(ptr);
    pthread_mutex_lock(((__posix_mutex_ptr)ptr)->mutex);
}

bool __posix_mutex_trylock(__xmutex_ptr ptr)
{
    assert(ptr);
    return pthread_mutex_trylock(((__posix_mutex_ptr)ptr)->mutex) == 0;
}

static void __posix_mutex_notify(__xmutex_ptr ptr)
{
    assert(ptr);
    pthread_cond_signal(((__posix_mutex_ptr)ptr)->cond);
}

static void __posix_mutex_broadcast(__xmutex_ptr ptr)
{
    assert(ptr);
    pthread_cond_broadcast(((__posix_mutex_ptr)ptr)->cond);
}

static void __posix_mutex_wait(__xmutex_ptr ptr)
{
    assert(ptr);
    pthread_cond_wait(((__posix_mutex_ptr)ptr)->cond, ((__posix_mutex_ptr)ptr)->mutex);
}

static int __posix_mutex_timedwait(__xmutex_ptr ptr, uint64_t delay)
{
    assert(ptr);
    struct timespec ts;
    delay += __unix_time();
    ts.tv_sec = delay / NANO_SECONDS;
    ts.tv_nsec = delay % NANO_SECONDS;
    if (pthread_cond_timedwait(((__posix_mutex_ptr)ptr)->cond, ((__posix_mutex_ptr)ptr)->mutex, &ts) == ETIMEDOUT){
        return __XAPI_TIMEDOUT;
    }
    return 0;
}

static void __posix_mutex_unlock(__xmutex_ptr ptr)
{
    assert(ptr);
    pthread_mutex_unlock(((__posix_mutex_ptr)ptr)->mutex);
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

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h> //struct hostent


static int udp_open(int buf_size)
{
    int sock;
    int opt;
    int flags;
    __xbreak((sock = socket(PF_INET, SOCK_DGRAM, 0)) < 0);
    // opt = 1;
    // __xbreak(setsockopt(sock, SOL_SOCKET, SO_RCVLOWAT, &opt, sizeof(opt)) != 0);
    opt = 1;
    __xbreak(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0);
    __xbreak(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) != 0);
    // if (buf_size > 0){
    //     opt = buf_size;
    //     __xbreak(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &opt, sizeof(opt)) != 0);
    //     // opt = buf_size * 5;
    //     // __xbreak(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt)) != 0);
    // }
    flags = fcntl(sock, F_GETFL, 0);
    __xbreak(fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1);
    return sock;

Clean:
    return -1;
}

static int udp_close(int sock)
{
    return close(sock);
}

static int udp_bind(int sock, __xipaddr_ptr ipaddr)
{
    // struct sockaddr_in *addr = (struct sockaddr_in *)&ipaddr;
    // ipaddr->addrlen = sizeof(struct sockaddr_in);
    return bind(sock, (const struct sockaddr *)ipaddr, (socklen_t)ipaddr->addrlen);
}

static uint64_t send_number = 0, lost_number = 0;
static int udp_sendto(int sock, __xipaddr_ptr ipaddr, void *data, size_t size)
{
    send_number++;
    uint64_t randtime = __unix_clock() / 1000000ULL;
    if ((send_number & 0x0f) == (randtime & 0x0f)){
        return size;
    }
    return sendto(sock, data, size, 0, (struct sockaddr*)ipaddr, (socklen_t)ipaddr->addrlen);
}

static int udp_recvfrom(int sock, __xipaddr_ptr ipaddr, void *buf, size_t size)
{
    // int result = recvfrom(sock, buf, size, 0, (struct sockaddr*)ipaddr, (socklen_t*)&ipaddr->addrlen);
    // if (result > 0){
    //     ipaddr->ip = ((struct sockaddr_in*)ipaddr->addr)->sin_addr.s_addr;
    //     ipaddr->port = ((struct sockaddr_in*)ipaddr->addr)->sin_port;
    //     ipaddr->keylen = 6;
    // }
    return recvfrom(sock, buf, size, 0, (struct sockaddr*)ipaddr, (socklen_t*)&ipaddr->addrlen);
}

static int udp_listen(int sock, uint64_t microseconds)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval timer;
    timer.tv_sec = microseconds / 1000000UL;
    timer.tv_usec = microseconds & 1000000UL;
    return select(sock + 1, &fds, NULL, NULL, &timer);
}

bool udp_addrinfo(char* ip_str, size_t ip_str_len, const char *hostname) {

    int status;
    struct addrinfo hints = {0}, *res, *p;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    __xcheck((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0);

    if (res) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
        __xcheck(inet_ntop(res->ai_family, &(ipv4->sin_addr), ip_str, ip_str_len) != NULL);
    }

    freeaddrinfo(res);

    return true;

XClean:

    if (res){
        freeaddrinfo(res);
    }

    return false;
}

bool udp_hostbyname(char* ip_str, size_t ip_str_len, const char *name) {
    struct hostent *host_info = gethostbyname(name);
    if (host_info != NULL && host_info->h_addr_list[0] != NULL) {
        return inet_ntop(AF_INET, host_info->h_addr_list[0], ip_str, ip_str_len) != NULL;
    }
    return false;
}

// #define INET_ADDRSTRLEN 16
// #define INET6_ADDRSTRLEN 46
bool udp_addr_to_host(const __xipaddr_ptr addr, char* ip, uint16_t* port) {
    *port = ntohs(addr->port);
    return inet_ntop(AF_INET, (struct in_addr*)&(addr->ip), ip, __XAPI_IP_STR_LEN) != NULL;
}

bool udp_host_to_addr(const char *ip, uint16_t port, __xipaddr_ptr ipaddr)
{
    ipaddr->keylen = 6;
    ipaddr->addrlen = sizeof(struct sockaddr_in);
    ipaddr->family = AF_INET;
    ipaddr->port = htons(port);
    if (ip == NULL){
        ipaddr->ip = INADDR_ANY;
    }else {
        return (inet_aton(ip, (struct in_addr*)&(ipaddr->ip)) == 1);
    }
    return true;
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
    .udp_bind = udp_bind,
    .udp_sendto = udp_sendto,
    .udp_recvfrom = udp_recvfrom,
    .udp_listen = udp_listen,
    .udp_host_to_addr = udp_host_to_addr,
    .udp_addr_to_host = udp_addr_to_host,
    .udp_hostbyname = udp_hostbyname,
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
