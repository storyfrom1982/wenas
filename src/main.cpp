#include <iostream>

#include "ex/ex.h"
#include "ex/malloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/struct/xline.h"

#ifdef __cplusplus
}
#endif

struct targs {
    __ex_mutex_ptr mtx;
    ___atom_bool *testTrue;
};

static void mutex_task(xmaker_ptr kv)
{
    struct targs *targ = (struct targs *)xline_find_pointer(kv, "ctx");

    std::cout << "tt: " << *targ->testTrue << std::endl;
    while (1)
    {
        bool ret = ___atom_try_lock(targ->testTrue);
        // std::cout << "__atom_try_lock: " << *targ->testTrue << std::endl;
        if (ret){
            std::cout << "__atom_try_lock: " << *targ->testTrue << std::endl;
            break;
        }
    }
    std::cout << "tt: " << *targ->testTrue << std::endl;

    std::cout << "thread enter\n";
    auto lk = __ex_mutex_lock(targ->mtx);
    std::cout << "sleep_until\n";
    __ex_mutex_timed_wait(targ->mtx, *(___lock*)(&lk), 3000000000);
    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    __ex_mutex_notify(targ->mtx);
    std::cout << "notify\n";
    __ex_mutex_wait(targ->mtx, lk);
    __ex_mutex_unlock(targ->mtx, lk);
    std::cout << "exit\n";
    // return nullptr;
}


int main(int argc, char *argv[])
{
    env_backtrace_setup();
    xlog_recorder_open("./tmp/log", NULL);
#if 1
    
    ___atom_size size = 10;
    ___atom_set(&size, 10);

    ___atom_sub(&size, 5);
    std::cout << "___atom_sub: " << size << std::endl;

    ___atom_add(&size, 15);
    std::cout << "___atom_add: " << size << std::endl;

    ___atom_bool testTrue = false;

    if (___set_true(&testTrue)){
        std::cout << "set false to true\n";
    }

    if (___is_true(&testTrue)){
        std::cout << "is true\n";
    }

    if (___set_false(&testTrue)){
        std::cout << "set true to false\n";
    }

    if (___is_false(&testTrue)){
        std::cout << "is false\n";
    }

    ___atom_lock(&testTrue);
    std::cout << "is lock: " << testTrue.load() << std::endl;

    __ex_mutex_ptr mtx = __ex_mutex_create();

    auto lk = __ex_mutex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    xtask_ptr task = xtask_create();
    struct xmaker ctx;
    xline_make(&ctx, NULL, 1024);
    xline_add_pointer(&ctx, "func", (void*)mutex_task);
    xline_add_pointer(&ctx, "ctx", (void*)&targ);
    __ex_task_post(task, ctx.xline);

    // ___thread_ptr tid = ___thread_create(mutex_task, &targ);

    __ex_mutex_timed_wait(mtx, *(CxxMutex::CxxLock*)(&lk), 3000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    std::cout << "waitting\n";
    __ex_mutex_wait(mtx, lk);
    std::cout << "wake\n";
    __ex_mutex_broadcast(mtx);
    __ex_mutex_unlock(mtx, lk);

    std::cout << "join thread " << __ex_thread_id() << std::endl;
    // ___thread_join(tid);
    xtask_free(&task);

    __ex_mutex_free(mtx);
#endif
    xlog_recorder_close();

    std::cout << "exit\n";

    char membuf[12343];
    mclear(membuf, 12343);
    memcpy(membuf, "123456789", strlen("123456789"));
    memcmp("123", "456", 3);

	return 0;
}
