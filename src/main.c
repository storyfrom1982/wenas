#include <stdio.h>

#include "ex/ex.h"
#include "ex/malloc.h"
#include "sys/struct/xline.h"


struct targs {
    __ex_mutex_ptr mtx;
    ___atom_bool *testTrue;
};

static void mutex_task(xmaker_ptr ctx)
{
    struct targs *targ = (struct targs *)xline_find_ptr(ctx, "ctx");
    __xlogi("lock %u\n", *targ->testTrue);

    while (1)
    {
        bool ret = ___atom_try_lock(targ->testTrue);
        __xlogi("__atom_try_lock %u\n", *targ->testTrue);
        if (ret){
            __xlogi("__atom_try_lock %u\n", *targ->testTrue);
            break;
        }
    }
    __xlogi("lock %u\n", *targ->testTrue);

    __xlogi("thread enter\n");
    ___lock lk = __ex_mutex_lock(targ->mtx);
    __xlogi("sleep_until\n");
    
    __ex_mutex_timed_wait(targ->mtx, lk, 3000000000);

    __ex_mutex_notify(targ->mtx);
    __xlogi("notify\n");
    __ex_mutex_wait(targ->mtx, lk);
    __ex_mutex_unlock(targ->mtx, lk);
    __xlogi("===============exit\n");
}

extern void test();

int main(int argc, char *argv[])
{
    env_backtrace_setup();
    __xlog_open("./tmp/log", NULL);
    
#if 1
    char text[1024] = {0};

    uint64_t millisecond = __ex_time();
    // ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    // __logi("c time %s", text);

    // millisecond = ___sys_clock();
    // ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    // __logi("c clock %s", text);

    ___atom_size size = 10;
    ___atom_sub(&size, 5);
    __xlogi("___atom_sub %lu\n", size);

    ___atom_add(&size, 15);
    __xlogi("___atom_add %lu\n", size);

    ___atom_bool testTrue = false;

    if (___set_true(&testTrue)){
        __xlogi("set false to true\n");
    }

    if (___is_true(&testTrue)){
        __xlogi("is true\n");
    }

    if (___set_false(&testTrue)){
        __xlogi("set true to false\n");
    }

    if (___is_false(&testTrue)){
        __xlogi("is false\n");
    }

    ___atom_lock(&testTrue);
    __xlogi("is lock\n");

    __ex_mutex_ptr mtx = __ex_mutex_create();

    ___lock lk = __ex_mutex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    xtask_ptr task = xtask_create();
    struct xmaker ctx;
    xline_maker_create(&ctx, NULL, 1024);
    xline_add_ptr(&ctx, "func", (void*)mutex_task);
    xline_add_ptr(&ctx, "ctx", (void*)&targ);
    __ex_task_post(task, ctx.xline);

    __ex_mutex_timed_wait(mtx, lk, 3000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    
    __xlogi("waitting\n");
    __ex_mutex_wait(mtx, lk);
    __xlogi("wake\n");
    
    __ex_mutex_broadcast(mtx);
    __ex_mutex_unlock(mtx, lk);

    __xlogi("join thread %lu\n", __ex_thread_id());
    // ___thread_join(tid);
    xtask_free(&task);

    __ex_mutex_free(mtx);

    char membuf[12343];
    mclear(membuf, 12343);
    memcpy(membuf, "123456789", strlen("123456789"));
    memcmp("123", "456", 3);
    
#endif
    __xlog_close();
    __xlogi("exit\n");

	return 0;
}
