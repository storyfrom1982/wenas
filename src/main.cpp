#include "env/env.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include <iostream>



struct CxxMutex {
    
public:
    using CxxLock = std::unique_lock<std::mutex>;

    CxxLock lock(){
        CxxLock _lock(_mutex);
        return _lock;
    }

    void unlock(CxxLock &_lock){
        _lock.unlock();
    }

    void wait(CxxLock &_lock){
        _cond.wait(_lock);
    }

    void timer(CxxLock &_lock, uint64_t nanos){
        _cond.wait_until(_lock, std::chrono::system_clock::now() + std::chrono::nanoseconds(nanos));
    }

    void notify(){
        _cond.notify_one();
    }

    void broadcast(){
        _cond.notify_all();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cond;
};


typedef struct CxxMutex*   ___mutex_ptr;

#define ___mutex_create()                  new CxxMutex()
#define ___mutex_release(mtx)              delete (mtx)
#define ___mutex_lock(mtx)                 (mtx)->lock()
#define ___mutex_notify(mtx)               (mtx)->notify()
#define ___mutex_broadcast(mtx)            (mtx)->broadcast()
#define ___mutex_wait(mtx, lock)           (mtx)->wait((lock))
#define ___mutex_timer(mtx, lock, delay)   (mtx)->timer((lock), (delay))
#define ___mutex_unlock(mtx, lock)         (mtx)->unlock((lock))



typedef std::atomic<size_t>     ___atom_bool;
typedef std::atomic<size_t>     ___atom_size;


#define ___atom_sub(x, y)       (x)->fetch_sub((y))
#define ___atom_add(x, y)       (x)->fetch_add((y))

static inline bool ___set_true(___atom_bool *obj)
{
    size_t ___atom_false = false;
    return obj->compare_exchange_strong(___atom_false, true);
}

static inline bool ___set_false(___atom_bool *obj)
{
    size_t ___atom_true = true;
    return obj->compare_exchange_strong(___atom_true, false);
}

#define	___is_true(x)           (x)->load()
#define	___is_false(x)          (x)->load()

#define ___atom_lock(x) \
    while(!___set_true(x)) \
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::nanoseconds(100))
#define ___atom_try_lock(x)     ___set_true(x)
#define ___atom_unlock(x)       ___set_false(x)


int main(int argc, char *argv[])
{
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

    std::thread s_th([mtx, &testTrue](){
        std::cout << "tt: " << testTrue << std::endl;
        while (1)
        {
            bool ret = ___atom_try_lock(&testTrue);
            // std::cout << "__atom_try_lock: " << ret << std::endl;
            if (ret){
                // std::cout << "__atom_try_lock: " << ret << std::endl;
                break;
            }
        }
        std::cout << "tt: " << testTrue << std::endl;

        std::cout << "thread enter\n";
        auto lk = ___mutex_lock(mtx);
        std::cout << "sleep_until\n";
        ___mutex_timer(mtx, *(CxxMutex::CxxLock*)(&lk), 1000000000);
        // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
        ___mutex_notify(mtx);
        std::cout << "notify\n";
        ___mutex_wait(mtx, lk);
        ___mutex_unlock(mtx, lk);
        std::cout << "exit\n";
    });

    ___mutex_timer(mtx, *(CxxMutex::CxxLock*)(&lk), 1000000000);
    ___atom_unlock(&testTrue);

    // std::this_thread::sleep_until(std::chrono::steady_clock::now() + 1000ms);
    std::cout << "waitting\n";
    ___mutex_wait(mtx, lk);
    std::cout << "wake\n";
    ___mutex_broadcast(mtx);
    ___mutex_unlock(mtx, lk);

    s_th.join();

    ___mutex_release(mtx);

    std::cout << "exit\n";

	return 0;
}
