#ifndef __UNIX_THREAD_H__
#define __UNIX_THREAD_H__


#include "unix.h"

#include <pthread.h>


typedef pthread_t env_thread_t;
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;


static inline int env_thread_create(env_thread_t *tid, void*(*func)(void*), void *ctx)
{
    return pthread_create(tid, NULL, func, ctx);
}

static inline env_thread_t env_thread_self()
{
    return pthread_self();
}

static inline int env_thread_destroy(env_thread_t tid)
{
    return pthread_join(tid, NULL);
}

static inline int env_thread_mutex_init(env_thread_mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

static inline void env_thread_mutex_destroy(env_thread_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

static inline void env_thread_mutex_lock(env_thread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

static inline void env_thread_mutex_unlock(env_thread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

static inline int env_thread_cond_init(env_thread_cond_t *cond)
{
    return pthread_cond_init(cond, NULL);
}

static inline void env_thread_cond_destroy(env_thread_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

static inline void env_thread_cond_signal(env_thread_cond_t *cond)
{
    pthread_cond_signal(cond);
}

static inline void env_thread_cond_broadcast(env_thread_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

static inline void env_thread_cond_wait(env_thread_cond_t *cond, env_thread_mutex_t *mutex)
{
    pthread_cond_wait(cond, mutex);
}

static inline int env_thread_cond_timedwait(env_thread_cond_t *cond, env_thread_mutex_t *mutex, uint64_t timeout)
{
    struct timespec ts;
    ts.tv_sec = timeout / NANOSEC;
    ts.tv_nsec = timeout % NANOSEC;
    return pthread_cond_timedwait(cond, mutex, &ts);
}


#endif