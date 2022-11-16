#ifndef __ENV_H__
#define __ENV_H__

#include "platforms/unix/unix.h"


typedef struct env_mutex {
    EnvThreadCond cond[1];
    EnvThreadMutex mutex[1];
}EnvMutex;

static inline int env_mutex_init(EnvMutex *emutex)
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

static inline void env_mutex_destroy(EnvMutex *emutex)
{
    env_thread_cond_destroy(emutex->cond);
    env_thread_mutex_destroy(emutex->mutex);
}

static inline void env_mutex_lock(EnvMutex *emutex)
{
    env_thread_mutex_lock(emutex->mutex);
}

static inline void env_mutex_unlock(EnvMutex *emutex)
{
    env_thread_mutex_unlock(emutex->mutex);
}

static inline void env_mutex_signal(EnvMutex *emutex)
{
    env_thread_cond_signal(emutex->cond);
}

static inline void env_mutex_broadcast(EnvMutex *emutex)
{
    env_thread_cond_broadcast(emutex->cond);
}

static inline void env_mutex_wait(EnvMutex *emutex)
{
    env_thread_cond_wait(emutex->cond, emutex->mutex);
}

static inline int env_mutex_timedwait(EnvMutex *emutex, uint64_t timeout)
{
    return env_thread_cond_timedwait(emutex->cond, emutex->mutex, timeout + env_time());
}

#endif