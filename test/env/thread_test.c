#include <env/env.h>

#include <stdio.h>
#include <stdlib.h>

#define __pass(condition) \
    do { \
        if (!(condition)) { \
            printf("Check condition failed: %s, %s\n", #condition, env_status()); \
            goto Reset; \
        } \
    } while (__false)

static __result thread_func(__ptr ctx)
{
    int ret;
    env_mutex_t *mutex = (env_mutex_t *)ctx;
    env_mutex_lock(mutex);
    printf("timedwait 5 second\n");
    ret = env_mutex_timedwait(mutex, (__uint64)(5 * NANO_SECONDS));
    printf("timedwait retcode=%llu %s\n", env_time(), env_status());
    env_mutex_signal(mutex);
    printf("timedwait 5 second\n");
    ret = env_mutex_timedwait(mutex, (__uint64)(5 * NANO_SECONDS));
    printf("timedwait retcode=%llu %s\n", env_time(), env_parser(ret));
    env_mutex_unlock(mutex);

    printf("thread %x exit\n", env_thread_self());

    return 0;
}

void thread_test()
{
    env_mutex_t *mutex = env_mutex_create();
    __pass(mutex != NULL);

    env_mutex_lock(mutex);

    env_thread_t *thread = env_thread_create(thread_func, mutex);
    __pass(thread != NULL);

    env_mutex_wait(mutex);
    env_mutex_unlock(mutex);

    printf("join thread %x\n", env_thread_self());
    env_thread_destroy(&thread);
    env_mutex_destroy(&mutex);

    printf("exit\n");

Reset:
    return;
}