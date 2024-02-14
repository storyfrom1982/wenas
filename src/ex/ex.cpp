#include <ex/ex.h>

#include <ctime>
#include <chrono>

#include <thread>
#include <threads.h>
#include <assert.h>


#ifdef __cplusplus
extern "C" {
#endif

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static uint64_t __cxx_time(void)
{
    return std::chrono::system_clock::now().time_since_epoch().count() * 1000;
}

///@return nanoseconds(relative time)
static uint64_t __cxx_clock(void)
{
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

static uint64_t __cxx_strftime(char *buf, uint64_t size, uint64_t seconds)
{
    return std::strftime(buf, size, "%Y/%m/%d-%H:%M:%S", std::localtime((const time_t*)&seconds));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////

static __xprocess_ptr __cxx_thread_create(void*(*task_enter)(void*), void *ctx)
{
    std::thread *thread_ptr = new std::thread(task_enter, ctx);
    if (thread_ptr != nullptr){
        return (__xprocess_ptr)thread_ptr;
    }
    return NULL;
}

static void __cxx_thread_free(__xprocess_ptr pid){
    std::thread *thread_ptr = (std::thread *)pid;
    thread_ptr->join();
    delete thread_ptr;    
}

static __xprocess_ptr __cxx_thread_self()
{
    return (__xprocess_ptr)(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////

typedef struct cxx_mutex {
    cnd_t cond[1];
    mtx_t mutex[1];
}*__cxx_mutex_ptr;

static __xmutex_ptr __cxx_mutex_create()
{
    __cxx_mutex_ptr ptr = new struct cxx_mutex;
    assert(ptr);
    int ret = cnd_init(ptr->cond);
    assert(ret == 0);
    ret = mtx_init(ptr->mutex, 0);
    assert(ret == 0);
    return (__xmutex_ptr)ptr;
}

static void __cxx_mutex_free(__xmutex_ptr ptr)
{
    if (ptr){
        __cxx_mutex_ptr mutex = (__cxx_mutex_ptr)ptr;
        delete mutex;
    }
}

static void __cxx_mutex_lock(__xmutex_ptr ptr)
{
    assert(ptr);
    mtx_lock(((__cxx_mutex_ptr)ptr)->mutex);
}

static void __cxx_mutex_notify(__xmutex_ptr ptr)
{
    assert(ptr);
    cnd_signal(((__cxx_mutex_ptr)ptr)->cond);
}

static void __cxx_mutex_broadcast(__xmutex_ptr ptr)
{
    assert(ptr);
    cnd_broadcast(((__cxx_mutex_ptr)ptr)->cond);
}

static void __cxx_mutex_wait(__xmutex_ptr ptr)
{
    assert(ptr);
    cnd_wait(((__cxx_mutex_ptr)ptr)->cond, ((__cxx_mutex_ptr)ptr)->mutex);
}

static int __cxx_mutex_timedwait(__xmutex_ptr ptr, uint64_t delay)
{
    assert(ptr);
    struct timespec ts;
    delay += __cxx_time();
    ts.tv_sec = delay / NANO_SECONDS;
    ts.tv_nsec = delay % NANO_SECONDS;
    if (cnd_timedwait(((__cxx_mutex_ptr)ptr)->cond, ((__cxx_mutex_ptr)ptr)->mutex, &ts) == ETIMEDOUT){
        return __XAPI_TIMEDOUT;
    }
    return 0;
}

static void __cxx_mutex_unlock(__xmutex_ptr ptr)
{
    assert(ptr);
    mtx_lock(((__cxx_mutex_ptr)ptr)->mutex);
}


#ifdef __cplusplus
}
#endif


struct __xapi_enter cxx_api_enter = {

    .time = __cxx_time,
    .clock = __cxx_clock,

    .process_create = __cxx_thread_create,
    .process_free = __cxx_thread_free,
    .process_self = __cxx_thread_self,

    .mutex_create = __cxx_mutex_create,
    .mutex_free = __cxx_mutex_free,
    .mutex_lock = __cxx_mutex_lock,
    .mutex_unlock = __cxx_mutex_unlock,
    .mutex_wait = __cxx_mutex_wait,
    .mutex_timedwait = __cxx_mutex_timedwait,
    .mutex_notify = __cxx_mutex_notify,
    .mutex_broadcast = __cxx_mutex_broadcast
    
};

__xapi_enter_ptr __xapi = &cxx_api_enter;