#include <env/env.h>

#include <stdio.h>
#include <stdlib.h>

#define __pass(condition) \
    do { \
        if (!(condition)) { \
            printf("Check condition failed: %s, %s\n", #condition, env_check()); \
            goto Reset; \
        } \
    } while (__false)

static __atombool testatom = __true;

static __result thread_func(__ptr ctx)
{
    int ret;
    env_mutex_t *mutex = (env_mutex_t *)ctx;
    env_mutex_lock(mutex);
    printf("timedwait 5 second\n");
    __sym buf[1024] = {0};
    __uint64 n = env_strtime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    printf("timedwait retcode=%s %s\n", buf, env_check());
    ret = env_mutex_timedwait(mutex, 5 * NANO_SECONDS);

    n = env_strtime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    printf("timedwait retcode=%s %s\n", buf, env_check());
    
    env_mutex_signal(mutex);
    printf("timedwait 5 second\n");
    ret = env_mutex_timedwait(mutex, 5 * NANO_SECONDS);
    n = env_strtime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    printf("timedwait retcode=%s %s\n", buf, env_check());
    env_mutex_unlock(mutex);

    printf("tid %llx exit\n", env_thread_self());

    __atom_unlock(testatom);

    return 0;
}

void thread_test()
{
    __uint64 running = __false;
    if (__is_false(running) && __set_true(running)){
        printf("running is %u\n", running);
    }

    if (__is_true(running) && __set_false(running)){
        printf("running is %u\n", running);
    }

    env_mutex_t *mutex = env_mutex_create();
    __pass(mutex != NULL);

    // env_mutex_lock(mutex);

    env_thread_t tid;
    __result r = env_thread_create(&tid, thread_func, mutex);
    __pass(r == 0);

    // env_mutex_wait(mutex);
    // env_mutex_unlock(mutex);

    __atom_lock(testatom);

    printf("join tid %llx\n", env_thread_self());
    // env_thread_destroy(tid);
    // env_mutex_destroy(&mutex);

    printf("exit\n");

Reset:
    return;
}