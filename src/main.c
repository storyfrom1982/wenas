#include "env/env.h"
#include "env/task.h"
#include "env/malloc.h"


struct targs {
    ___mutex_ptr mtx;
    ___atom_bool *testTrue;
};

static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}

static void mutex_task(linekv_ptr kv)
{
    struct targs *targ = (struct targs *)linekv_find_ptr(kv, "ctx");
    __logi("lock %u", *targ->testTrue);

    while (1)
    {
        bool ret = ___atom_try_lock(targ->testTrue);
        // __logi("__atom_try_lock %u", *targ->testTrue);
        if (ret){
            __logi("__atom_try_lock %u", *targ->testTrue);
            break;
        }
    }
    __logi("lock %u", *targ->testTrue);

    __logi("thread enter");
    ___lock lk = ___mutex_lock(targ->mtx);
    __logi("sleep_until");
    
    ___mutex_timer(targ->mtx, lk, 3000000000);

    ___mutex_notify(targ->mtx);
    __logi("notify");
    ___mutex_wait(targ->mtx, lk);
    ___mutex_unlock(targ->mtx, lk);
    __logi("exit");
}

int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/log", NULL);
	
    char text[1024] = {0};

    uint64_t millisecond = env_time();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    __logi("c time %s", text);

    millisecond = env_clock();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    __logi("c clock %s", text);

    ___atom_size size = 10;
    ___atom_sub(&size, 5);
    __logi("___atom_sub %lu", size);

    ___atom_add(&size, 15);
    __logi("___atom_add %lu", size);

    ___atom_bool testTrue = false;

    if (___set_true(&testTrue)){
        __logi("set false to true");
    }

    if (___is_true(&testTrue)){
        __logi("is true");
    }

    if (___set_false(&testTrue)){
        __logi("set true to false");
    }

    if (___is_false(&testTrue)){
        __logi("is false");
    }

    ___atom_lock(&testTrue);
    __logi("is lock");

    ___mutex_ptr mtx = ___mutex_create();

    ___lock lk = ___mutex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    task_ptr task = task_create();
    linekv_ptr kv = linekv_create(1024);
    linekv_add_ptr(kv, "func", (void*)mutex_task);
    linekv_add_ptr(kv, "ctx", (void*)&targ);
    task_post(task, kv);

    ___mutex_timer(mtx, lk, 3000000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    
    __logi("waitting");
    ___mutex_wait(mtx, lk);
    __logi("wake");
    
    ___mutex_broadcast(mtx);
    ___mutex_unlock(mtx, lk);

    __logi("join thread %lu", ___thread_id());
    // ___thread_join(tid);
    task_release(&task);

    ___mutex_release(mtx);

    linekv_release(&kv);

    env_logger_stop();

    __logi("exit");

#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

	return 0;
}
