#ifndef __ENV_THREAD_H__
#define __ENV_THREAD_H__


#include "unix/thread.h"


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


static inline int env_mutex_init(env_mutex_t *mutex)
{
    int ret;

    ret = env_thread_mutex_init(mutex->mutex);
    if (ret != 0){
        return ret;
    }

    ret = env_thread_cond_init(mutex->cond);
    if (ret != 0){
        env_thread_mutex_destroy(mutex->mutex);
    }
    
    return ret;
}

static inline void env_mutex_destroy(env_mutex_t *mutex)
{
    env_thread_cond_destroy(mutex->cond);
    env_thread_mutex_destroy(mutex->mutex);
}

static inline void env_mutex_lock(env_mutex_t *mutex)
{
    env_thread_mutex_lock(mutex->mutex);
}

static inline void env_mutex_unlock(env_mutex_t *mutex)
{
    env_thread_mutex_unlock(mutex->mutex);
}

static inline void env_mutex_signal(env_mutex_t *mutex)
{
    env_thread_cond_signal(mutex->cond);
}

static inline void env_mutex_broadcast(env_mutex_t *mutex)
{
    env_thread_cond_broadcast(mutex->cond);
}

static inline void env_mutex_wait(env_mutex_t *mutex)
{
    env_thread_cond_wait(mutex->cond, mutex->mutex);
}

static inline int env_mutex_timedwait(env_mutex_t *mutex, uint64_t timeout)
{
    return env_thread_cond_timedwait(mutex->cond, mutex->mutex, timeout + env_time());
}


#endif