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
    __ex_lock_ptr mtx;
    ___atom_bool *testTrue;
};

static void mutex_task(xline_object_ptr kv)
{
    struct targs *targ = (struct targs *)xline_object_find_ptr(kv, "ctx");

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
    auto lk = __ex_lock(targ->mtx);
    std::cout << "sleep_until\n";
    __ex_timed_wait(targ->mtx, *(___lock*)(&lk), 3000000000);
    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    __ex_notify(targ->mtx);
    std::cout << "notify\n";
    __ex_wait(targ->mtx, lk);
    __ex_unlock(targ->mtx, lk);
    std::cout << "exit\n";
    // return nullptr;
}


int main(int argc, char *argv[])
{
    __ex_log_file_open("./tmp/log", NULL);
#if 1
    __ex_backtrace_setup();

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
    std::cout << "is lock: " << testTrue << std::endl;

    __ex_lock_ptr mtx = __ex_lock_create();

    auto lk = __ex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    __ex_task_ptr task = __ex_task_create();
    struct xline_object kv;
    xline_make_object(&kv, 1024);
    xline_object_add_ptr(&kv, "func", (void*)mutex_task);
    xline_object_add_ptr(&kv, "ctx", (void*)&targ);
    __ex_task_post(task, &kv);

    // ___thread_ptr tid = ___thread_create(mutex_task, &targ);

    __ex_timed_wait(mtx, *(CxxMutex::CxxLock*)(&lk), 3000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    std::cout << "waitting\n";
    __ex_wait(mtx, lk);
    std::cout << "wake\n";
    __ex_broadcast(mtx);
    __ex_unlock(mtx, lk);

    std::cout << "join thread " << __ex_thread_id() << std::endl;
    // ___thread_join(tid);
    __ex_task_destroy(&task);

    __ex_lock_destroy(mtx);
#endif
    __ex_log_file_close();

    std::cout << "exit\n";

	return 0;
}
