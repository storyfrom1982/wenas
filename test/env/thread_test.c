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

    printf("tid %llx exit\n", env_thread_self());

    return 0;
}

void thread_test()
{
    env_mutex_t *mutex = env_mutex_create();
    __pass(mutex != NULL);

    env_mutex_lock(mutex);

    env_thread_t tid;
    __result r = env_thread_create(&tid, thread_func, mutex);
    __pass(r == 0);

    env_mutex_wait(mutex);
    env_mutex_unlock(mutex);

    printf("join tid %llx\n", env_thread_self());
    env_thread_destroy(tid);
    env_mutex_destroy(&mutex);

    printf("exit\n");

Reset:
    return;
}