#ifndef __ENV_UNIX_H__
#define __ENV_UNIX_H__

#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#undef MILLISEC
#define MILLISEC    1000UL

#undef MICROSEC
#define MICROSEC    1000000UL

#undef NANOSEC
#define NANOSEC     1000000000UL

typedef pthread_t EnvThread;
typedef pthread_cond_t EnvThreadCond;
typedef pthread_mutex_t EnvThreadMutex;


static inline uint64_t env_time()
{
    struct timespec t;
    //https://man7.org/linux/man-pages/man2/clock_getres.2.html
    if (clock_gettime(CLOCK_REALTIME, &t) == 0){
        return t.tv_sec * NANOSEC + t.tv_nsec;
    }else {
        struct timeval tv;
        //https://man7.org/linux/man-pages/man2/settimeofday.2.html
        if (gettimeofday(&tv, NULL) == 0){
            return tv.tv_sec * NANOSEC + tv.tv_usec * 1000UL;
        }
    }
    //TODO
    return 0;
}

static inline int env_thread_create(EnvThread *tid, void*(*func)(void*), void *ctx)
{
    return pthread_create(tid, NULL, func, ctx);
}

static inline EnvThread env_thread_self()
{
    return pthread_self();
}

static inline int env_thread_join(EnvThread tid)
{
    return pthread_join(tid, NULL);
}

static inline int env_thread_mutex_init(EnvThreadMutex *emutex)
{
    return pthread_mutex_init(emutex, NULL);
}

static inline void env_thread_mutex_destroy(EnvThreadMutex *emutex)
{
    pthread_mutex_destroy(emutex);
}

static inline void env_thread_mutex_lock(EnvThreadMutex *emutex)
{
    pthread_mutex_lock(emutex);
}

static inline void env_thread_mutex_unlock(EnvThreadMutex *emutex)
{
    pthread_mutex_unlock(emutex);
}

static inline int env_thread_cond_init(EnvThreadCond *econd)
{
    return pthread_cond_init(econd, NULL);
}

static inline void env_thread_cond_destroy(EnvThreadCond *econd)
{
    pthread_cond_destroy(econd);
}

static inline void env_thread_cond_signal(EnvThreadCond *econd)
{
    pthread_cond_signal(econd);
}

static inline void env_thread_cond_broadcast(EnvThreadCond *econd)
{
    pthread_cond_broadcast(econd);
}

static inline void env_thread_cond_wait(EnvThreadCond *econd, EnvThreadMutex *emutex)
{
    pthread_cond_wait(econd, emutex);
}

static inline int env_thread_cond_timedwait(EnvThreadCond *econd, EnvThreadMutex *emutex, uint64_t timeout)
{
    struct timespec ts;
    ts.tv_sec = timeout / NANOSEC;
    ts.tv_nsec = timeout % NANOSEC;
    return pthread_cond_timedwait(econd, emutex, &ts);
}

#endif // __ENV_UNIX_H__