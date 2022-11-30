#ifndef __ENV_THREAD_H__
#define __ENV_THREAD_H__


#include "unix/thread.h"


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


static inline int env_mutex_init(env_mutex_t *emutex)
{
    int ret;

    ret = env_thread_mutex_init(emutex->mutex);
    if (ret != 0){
        return ret;
    }

    ret = env_thread_cond_init(emutex->cond);
    if (ret != 0){
        env_thread_mutex_destroy(emutex->mutex);
    }
    
    return ret;
}

static inline void env_mutex_destroy(env_mutex_t *emutex)
{
    env_thread_cond_destroy(emutex->cond);
    env_thread_mutex_destroy(emutex->mutex);
}

static inline void env_mutex_lock(env_mutex_t *emutex)
{
    env_thread_mutex_lock(emutex->mutex);
}

static inline void env_mutex_unlock(env_mutex_t *emutex)
{
    env_thread_mutex_unlock(emutex->mutex);
}

static inline void env_mutex_signal(env_mutex_t *emutex)
{
    env_thread_cond_signal(emutex->cond);
}

static inline void env_mutex_broadcast(env_mutex_t *emutex)
{
    env_thread_cond_broadcast(emutex->cond);
}

static inline void env_mutex_wait(env_mutex_t *emutex)
{
    env_thread_cond_wait(emutex->cond, emutex->mutex);
}

static inline int env_mutex_timedwait(env_mutex_t *emutex, uint64_t timeout)
{
    return env_thread_cond_timedwait(emutex->cond, emutex->mutex, timeout + env_time());
}


#endif