#include <env/env.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

static void* thread_func(void *ctx)
{
    int ret;
    env_mutex_t *mutex = (env_mutex_t *)ctx;
    env_mutex_lock(mutex);
    LOGD("THREAD", "timedwait 5 second\n");
    ret = env_mutex_timedwait(mutex, (uint64_t)(5 * NANOSEC));
    LOGD("THREAD", "timedwait retcode=%d %s\n", ret, strerror(ret));
    env_mutex_signal(mutex);
    LOGD("THREAD", "timedwait 5 second\n");
    ret = env_mutex_timedwait(mutex, (uint64_t)(5 * NANOSEC));
    LOGD("THREAD", "timedwait retcode=%d %s\n", ret, strerror(ret));
    env_mutex_unlock(mutex);

    LOGD("THREAD", "thread %x exit\n", env_thread_self());

    return NULL;
}

void thread_test()
{
    env_thread_t tid;
    env_mutex_t mutex;
    int ret = env_mutex_init(&mutex);
    if (ret != 0){
        fprintf(stderr, "env_mutex_init failed: errcode=%d\n", ret);
    }
    env_mutex_lock(&mutex);

    ret = env_thread_create(&tid, thread_func, &mutex);
    if (ret != 0){
        fprintf(stderr, "env_thread_create failed: errcode=%d\n", ret);
    }

    env_mutex_wait(&mutex);
    env_mutex_unlock(&mutex);

    LOGD("THREAD", "join thread %x\n", tid);
    env_thread_join(tid);

    LOGD("THREAD", "exit\n");
}