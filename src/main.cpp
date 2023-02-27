#include "env/mutex.h"

extern "C" {
    #include "env/env.h"
}


struct targs {
    ___mutex_ptr mtx;
    ___atom_bool *testTrue;
};


#include <iostream>

int main(int argc, char *argv[])
{
    char text[1024] = {0};

    uint64_t millisecond = env_time();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    std::cout << "c time: " << text << std::endl;

    millisecond = ___sys_time();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    std::cout << "cxx time: " << text << std::endl;

    millisecond = env_clock();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);

    std::cout << "c clock: " << text << std::endl;

    millisecond = ___sys_clock();
    ___sys_strftime(text, 1024, millisecond / NANO_SECONDS);
    std::cout << "cxx clock: " << text << std::endl;

    ___atom_size size = 10;
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

    ___mutex_ptr mtx = ___mutex_create();

    auto lk = ___mutex_lock(mtx);

    // ___atom_bool *tt = &testTrue;
    // std::cout << "testTrue: " << *tt << std::endl;

    struct targs targ;
    targ.mtx = mtx;
    targ.testTrue = &testTrue;

    __thread_ptr tid = ___thread_create([](void *p) -> void*
        {
            struct targs *targ = (struct targs *)p;
            std::cout << "tt: " << *targ->testTrue << std::endl;
            while (1)
            {
                bool ret = ___atom_try_lock(targ->testTrue);
                std::cout << "__atom_try_lock: " << *targ->testTrue << std::endl;
                if (ret){
                    std::cout << "__atom_try_lock: " << *targ->testTrue << std::endl;
                    break;
                }
            }
            std::cout << "tt: " << *targ->testTrue << std::endl;

            std::cout << "thread enter\n";
            auto lk = ___mutex_lock(targ->mtx);
            std::cout << "sleep_until\n";
            ___mutex_timer(targ->mtx, *(CxxMutex::CxxLock*)(&lk), 1000000000);
            // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
            ___mutex_notify(targ->mtx);
            std::cout << "notify\n";
            ___mutex_wait(targ->mtx, lk);
            ___mutex_unlock(targ->mtx, lk);
            std::cout << "exit\n";
            return nullptr;
    }, &targ);

    ___mutex_timer(mtx, *(CxxMutex::CxxLock*)(&lk), 1000000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    std::cout << "waitting\n";
    ___mutex_wait(mtx, lk);
    std::cout << "wake\n";
    ___mutex_broadcast(mtx);
    ___mutex_unlock(mtx, lk);

    std::cout << "join thread " << ___thread_id() << std::endl;
    ___thread_join(tid);

    ___mutex_release(mtx);

    std::cout << "exit\n";

	return 0;
}
