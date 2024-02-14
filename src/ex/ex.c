#include <ex/ex.h>

#include <assert.h>
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


static uint64_t __unix_strftime(char *buf, uint64_t size, uint64_t seconds)
{
	time_t sec = (time_t)seconds;
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


struct __xapi_enter posix_api_enter = {

    .time = __unix_time,
    .clock = __unix_clock,

    .process_create = __posix_thread_create,
    .process_free = __posix_thread_free,
    .process_self = __posix_thread_self,

    .mutex_create = __posix_mutex_create,
    .mutex_free = __posix_mutex_free,
    .mutex_lock = __posix_mutex_lock,
    .mutex_unlock = __posix_mutex_unlock,
    .mutex_wait = __posix_mutex_wait,
    .mutex_timedwait = __posix_mutex_timedwait,
    .mutex_notify = __posix_mutex_notify,
    .mutex_broadcast = __posix_mutex_broadcast
    
};

__xapi_enter_ptr __xapi = &posix_api_enter;