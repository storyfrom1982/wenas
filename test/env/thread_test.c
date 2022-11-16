#include <env.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

static void* thread_func(void *ctx)
{
    EnvMutex *mutex = (EnvMutex *)ctx;
    env_mutex_lock(mutex);
    fprintf(stdout, "timedwait 5 second\n");
    int ret = env_mutex_timedwait(mutex, (uint64_t)(5 * NANOSEC));
    fprintf(stdout, "timedwait retcode=%d %s\n", ret, strerror(ret));
    env_mutex_signal(mutex);
    env_mutex_unlock(mutex);
    return NULL;
}

void thread_test()
{
    EnvThread tid;
    EnvMutex mutex;
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

    fprintf(stdout, "exit\n");
}