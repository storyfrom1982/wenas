#include <stdio.h>

#include "ex/ex.h"
#include "ex/task.h"
#include "ex/malloc.h"


struct targs {
    ___mutex_ptr mtx;
    ___atom_bool *testTrue;
};

static void mutex_task(xline_object_ptr kv)
{
    struct targs *targ = (struct targs *)xline_object_find_ptr(kv, "ctx");
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
    ___lock lk = ___mutex_lock(targ->mtx);
    __ex_logi("sleep_until\n");
    
    ___mutex_timer(targ->mtx, lk, 3000000);

    ___mutex_notify(targ->mtx);
    __ex_logi("notify\n");
    ___mutex_wait(targ->mtx, lk);
    ___mutex_unlock(targ->mtx, lk);
    __ex_logi("===============exit\n");
}

extern void test();

int main(int argc, char *argv[])
{
    char buf[BUFSIZ];
    setvbuf(stdout, buf, _IONBF, BUFSIZ);
    __ex_log_file_open("./tmp/log", NULL);
    env_backtrace_setup();

#if 1
    char text[1024] = {0};

    uint64_t millisecond = ___sys_time();
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

    ___mutex_ptr mtx = ___mutex_create();

    ___lock lk = ___mutex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    taskqueue_ptr task = taskqueue_create();
    struct xline_object kv;
    xline_make_object(&kv, 1024);
    xline_object_add_ptr(&kv, "func", (void*)mutex_task);
    xline_object_add_ptr(&kv, "ctx", (void*)&targ);
    taskqueue_post(task, kv.addr);

    // ___mutex_timer(mtx, lk, 3000000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    
    __ex_logi("waitting\n");
    ___mutex_wait(mtx, lk);
    __ex_logi("wake\n");
    
    ___mutex_broadcast(mtx);
    ___mutex_unlock(mtx, lk);

    __ex_logi("join thread %lu\n", ___thread_id());
    // ___thread_join(tid);
    taskqueue_release(&task);

    ___mutex_release(mtx);

    
#endif
    __ex_log_file_close();
    __ex_logi("exit\n");

	return 0;
}
