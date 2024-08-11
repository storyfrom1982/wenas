#include <env/env.h>

static __atombool testatom = true;

static __ptr thread_func(__ptr ctx)
{
    int ret;
    env_mutex_t *mutex = (env_mutex_t *)ctx;
    env_mutex_lock(mutex);
    __logi("timedwait 1 second\n");
    char buf[1024] = {0};
    uint64_t n = env_strftime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    __logi("timedwait retcode=%s\n", buf);
    ret = env_mutex_timedwait(mutex, 1 * NANO_SECONDS);
    __pass(ret == 0 || ret == ENV_TIMEDOUT);

    n = env_strftime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    
    env_mutex_signal(mutex);
    __logi("timedwait 1 second\n");
    ret = env_mutex_timedwait(mutex, 1 * NANO_SECONDS);
    __pass(ret == 0 || ret == ENV_TIMEDOUT);
    n = env_strftime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    __logi("timedwait retcode=%s\n", buf);
    env_mutex_unlock(mutex);

    __logi("tid %llx exit\n", env_thread_self());

    __atom_unlock(testatom);

Reset:

    return NULL;
}

void thread_test()
{
    uint64_t running = false;
    if (__is_false(running) && __set_true(running)){
        __logd("running is %u\n", running);
    }

    if (__is_true(running) && __set_false(running)){
        __logd("running is %u\n", running);
    }

    env_mutex_t *mutex = env_mutex_create();
    __pass(mutex != NULL);

    // env_mutex_lock(mutex);

    env_thread_ptr thread;
    int32_t r = env_thread_create(&thread, thread_func, mutex);
    __pass(r == 0);

    // env_mutex_wait(mutex);
    // env_mutex_unlock(mutex);

    __atom_lock(testatom);

    __logd("join tid %llx\n", env_thread_id(thread));
    env_thread_destroy(&thread);
    // env_mutex_destroy(&mutex);

    env_mutex_destroy(&mutex);
    __logd("exit\n");

Reset:
    return;
}