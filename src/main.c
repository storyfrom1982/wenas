#include "env/env.h"
#include "env/mutex.h"


struct targs {
    ___mutex_ptr mtx;
    ___atom_bool *testTrue;
};


static void* sthread(void *p)
{
    struct targs *targ = (struct targs *)p;
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
    
    ___mutex_timer(targ->mtx, lk, 1000000000);

    ___mutex_notify(targ->mtx);
    __logi("notify");
    ___mutex_wait(targ->mtx, lk);
    ___mutex_unlock(targ->mtx, lk);
    __logi("exit");
    return NULL;
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

    __thread_ptr tid = ___thread_create(sthread, &targ);

    ___mutex_timer(mtx, lk, 1000000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    
    __logi("waitting");
    ___mutex_wait(mtx, lk);
    __logi("wake");
    
    ___mutex_broadcast(mtx);
    ___mutex_unlock(mtx, lk);

    __logi("join thread %lu", ___thread_id());
    ___thread_join(tid);

    ___mutex_release(mtx);

    __logi("exit");

	return 0;
}
