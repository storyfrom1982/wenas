#include <stdio.h>

#include "ex/ex.h"
#include "ex/malloc.h"
#include "sys/struct/xline.h"


struct targs {
    __ex_lock_ptr mtx;
    ___atom_bool *testTrue;
};

static void mutex_task(__ex_task_ctx_maker_ptr ctx)
{
    struct targs *targ = (struct targs *)xline_find_ptr(ctx, "ctx");
    __ex_logi("lock %u\n", *targ->testTrue);

    while (1)
    {
        bool ret = ___atom_try_lock(targ->testTrue);
        __ex_logi("__atom_try_lock %u\n", *targ->testTrue);
        if (ret){
            __ex_logi("__atom_try_lock %u\n", *targ->testTrue);
            break;
        }
    }
    __ex_logi("lock %u\n", *targ->testTrue);

    __ex_logi("thread enter\n");
    ___lock lk = __ex_lock(targ->mtx);
    __ex_logi("sleep_until\n");
    
    __ex_timed_wait(targ->mtx, lk, 3000000000);

    __ex_notify(targ->mtx);
    __ex_logi("notify\n");
    __ex_wait(targ->mtx, lk);
    __ex_unlock(targ->mtx, lk);
    __ex_logi("===============exit\n");
}

extern void test();

int main(int argc, char *argv[])
{
    __ex_log_file_open("./tmp/log", NULL);
    __ex_backtrace_setup();

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
    __ex_logi("___atom_sub %lu\n", size);

    ___atom_add(&size, 15);
    __ex_logi("___atom_add %lu\n", size);

    ___atom_bool testTrue = false;

    if (___set_true(&testTrue)){
        __ex_logi("set false to true\n");
    }

    if (___is_true(&testTrue)){
        __ex_logi("is true\n");
    }

    if (___set_false(&testTrue)){
        __ex_logi("set true to false\n");
    }

    if (___is_false(&testTrue)){
        __ex_logi("is false\n");
    }

    ___atom_lock(&testTrue);
    __ex_logi("is lock\n");

    __ex_lock_ptr mtx = __ex_lock_create();

    ___lock lk = __ex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    __ex_task_ptr task = __ex_task_create();
    struct xline_maker ctx;
    xline_maker_setup(&ctx, NULL, 1024);
    xline_add_ptr(&ctx, "func", (void*)mutex_task);
    xline_add_ptr(&ctx, "ctx", (void*)&targ);
    __ex_task_post(task, ctx.xline);

    __ex_timed_wait(mtx, lk, 3000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    
    __ex_logi("waitting\n");
    __ex_wait(mtx, lk);
    __ex_logi("wake\n");
    
    __ex_broadcast(mtx);
    __ex_unlock(mtx, lk);

    __ex_logi("join thread %lu\n", __ex_thread_id());
    // ___thread_join(tid);
    __ex_task_destroy(&task);

    __ex_lock_destroy(mtx);

    char membuf[12343];
    mclear(membuf, 12343);
    memcpy(membuf, "123456789", strlen("123456789"));
    memcmp("123", "456", 3);
    
#endif
    __ex_log_file_close();
    __ex_logi("exit\n");

	return 0;
}
