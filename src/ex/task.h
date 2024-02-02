#ifndef __EX_TASK_H__
#define __EX_TASK_H__

#define __EX_TIMEDOUT           1

#define __EX_NANO_SECONDS       1000000000ULL

#ifndef __cplusplus

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static inline uint64_t __ex_time(void)
{
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * __EX_NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * __EX_NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

///@return nanoseconds(relative time)
static inline uint64_t __ex_clock(void)
{
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * __EX_NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * __EX_NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

static inline uint64_t __ex_strftime(char *buf, uint64_t size, uint64_t seconds)
{
	time_t sec = (time_t)seconds;
    struct tm t;
    localtime_r(&sec, &t);
    return strftime(buf, size, "%Y/%m/%d-%H:%M:%S", &t);
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef pthread_t __ex_thread_ptr;

#define ___BAD_THREAD   0

static inline __ex_thread_ptr __ex_thread_create(void*(*task_func)(void*), void *ctx)
{
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);    
    int ret = pthread_create(&tid, &attr, task_func, ctx);
    if (ret == 0){
        return tid;
    }
    return ___BAD_THREAD;
}

static inline void __ex_thread_join(__ex_thread_ptr ptr)
{
    pthread_join(ptr, NULL);
}

static inline uint64_t __ex_thread_id()
{
    return (uint64_t)pthread_self();
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef size_t ___atom_bool;
typedef size_t ___atom_size;
typedef uint8_t ___atom_8bit;
typedef uint16_t ___atom_size16bit;

#define	___is_true(x)	    	__sync_bool_compare_and_swap((x), true, true)
#define	___is_false(x)	    	__sync_bool_compare_and_swap((x), false, false)
#define	___set_true(x)		    __sync_bool_compare_and_swap((x), false, true)
#define	___set_false(x)		    __sync_bool_compare_and_swap((x), true, false)

#define ___atom_set(x, y)       __sync_bool_compare_and_swap((x), *(x), (y))

#define ___atom_sub(x, y)		__sync_sub_and_fetch((x), (y))
#define ___atom_add(x, y)		__sync_add_and_fetch((x), (y))

#define ___atom_lock(x)			while(!___set_true(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define ___atom_try_lock(x)		___set_true(x)
#define ___atom_unlock(x)		___set_false(x)


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef struct ex_mutex {
    pthread_cond_t cond[1];
    pthread_mutex_t mutex[1];
}*__ex_mutex_ptr;

typedef pthread_mutex_t*      ___lock;

static inline __ex_mutex_ptr __ex_mutex_create()
{
    int ret;
    __ex_mutex_ptr mptr = (__ex_mutex_ptr)malloc(sizeof(struct ex_mutex));
    assert(mptr);
    ret = pthread_mutex_init(mptr->mutex, NULL);
    assert(ret == 0);
    ret = pthread_cond_init(mptr->cond, NULL);
    assert(ret == 0);
    return mptr;
}

static inline void __ex_mutex_free(__ex_mutex_ptr mptr)
{
    int ret;
    assert(mptr);
    ret = pthread_mutex_destroy(mptr->mutex);
    assert(ret == 0);
    ret = pthread_cond_destroy(mptr->cond);
    assert(ret == 0);
    free(mptr);
}

static inline pthread_mutex_t* __ex_mutex_lock(__ex_mutex_ptr mptr)
{
    assert(mptr);
    pthread_mutex_lock(mptr->mutex);
    return mptr->mutex;
}

static inline void __ex_mutex_notify(__ex_mutex_ptr mptr)
{
    assert(mptr);
    pthread_cond_signal(mptr->cond);
}

static inline void __ex_mutex_broadcast(__ex_mutex_ptr mptr)
{
    assert(mptr);
    pthread_cond_broadcast(mptr->cond);
}

static inline void __ex_mutex_wait(__ex_mutex_ptr mptr, pthread_mutex_t *lock)
{
    assert(mptr);
    pthread_cond_wait(mptr->cond, lock);
}

static inline int __ex_mutex_timed_wait(__ex_mutex_ptr mptr, pthread_mutex_t *lock, uint64_t delay)
{
    assert(mptr);
    struct timespec ts;
    delay += __ex_time();
    ts.tv_sec = delay / __EX_NANO_SECONDS;
    ts.tv_nsec = delay % __EX_NANO_SECONDS;
    if (pthread_cond_timedwait(mptr->cond, lock, &ts) == ETIMEDOUT){
        return __EX_TIMEDOUT;
    }
    return 0;
}

static inline void __ex_mutex_unlock(__ex_mutex_ptr mptr, pthread_mutex_t *lock)
{
    assert(mptr && lock);
    pthread_mutex_unlock(lock);
}



#else //__cplusplus



#include <ctime>
#include <chrono>

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <assert.h>


static inline uint64_t __ex_time()
{
    return std::chrono::system_clock::now().time_since_epoch().count() * 1000;
}

static inline uint64_t __ex_clock()
{ 
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

static inline uint64_t __ex_strftime(char *buf, uint64_t size, uint64_t seconds)
{
    return std::strftime(buf, size, "%Y/%m/%d-%H:%M:%S", std::localtime((const time_t*)&seconds));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef void* __ex_thread_ptr;

#define ___BAD_THREAD   nullptr

static inline __ex_thread_ptr __ex_thread_create(void*(*task_func)(void*), void *ctx)
{
    // std::thread *thread_ptr = new std::thread([](void*(*task_func)(void*), void *ctx){
    //     task_func(ctx);
    // }, task_func, ctx);

    std::thread *thread_ptr = new std::thread(task_func, ctx);
    if (thread_ptr != nullptr){
        return (__ex_thread_ptr)thread_ptr;
    }
    return ___BAD_THREAD;
}

static inline void __ex_thread_join(__ex_thread_ptr ptr)
{
    std::thread *thread_ptr = (std::thread *)ptr;
    thread_ptr->join();
    delete thread_ptr;
}

static inline uint64_t __ex_thread_id()
{
    return (uint64_t)(std::hash<std::thread::id>{}(std::this_thread::get_id()));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef std::atomic<size_t>                 ___atom_bool;
typedef std::atomic<size_t>                 ___atom_size;
typedef std::atomic<uint8_t>                 ___atom_8bit;
typedef std::atomic<uint16_t>                 ___atom_size16bit;


#define ___atom_set(x, y)                   (x)->store((y))
#define ___atom_sub(x, y)                   ((x)->fetch_sub((y)) - 1)
#define ___atom_add(x, y)                   ((x)->fetch_add((y)) + 1)

#define	___is_true(x)                       ((x)->load() == true)
#define	___is_false(x)                      ((x)->load() == false)

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

#define ___atom_lock(x) \
    while(!___set_true(x)) \
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::nanoseconds(1000))
#define ___atom_try_lock(x)                 ___set_true(x)
#define ___atom_unlock(x)                   ___set_false(x)


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


struct CxxMutex {
    
public:
    using CxxLock = std::unique_lock<std::mutex>;

    CxxLock lock(){
        // CxxLock _lock(_mutex, std::defer_lock);
        // _lock.lock();
        CxxLock _lock(_mutex);
        return _lock;
    }

    void unlock(CxxLock &_lock){
        _lock.unlock();
        // _lock.release();
    }

    void wait(CxxLock &_lock){
        _cond.wait(_lock);
    }

    int timer(CxxLock &_lock, uint64_t nanos){
        if (_cond.wait_until(_lock, std::chrono::steady_clock::now() + std::chrono::nanoseconds(nanos)) == std::cv_status::timeout){
            return __EX_TIMEDOUT;
        }
        return 0;
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

using ___lock = CxxMutex::CxxLock;
typedef struct CxxMutex*    __ex_mutex_ptr;

static inline __ex_mutex_ptr __ex_mutex_create()
{
    __ex_mutex_ptr ptr = new CxxMutex();
    assert(ptr);
    return ptr;
}

static inline void __ex_mutex_free(__ex_mutex_ptr mtx)
{
    if (mtx){
        delete mtx;
    }
}

#define __ex_mutex_lock(mtx)                      (mtx)->lock()
#define __ex_mutex_notify(mtx)                    (mtx)->notify()
#define __ex_mutex_broadcast(mtx)                 (mtx)->broadcast()
#define __ex_mutex_wait(mtx, lock)                (mtx)->wait((lock))
#define __ex_mutex_timed_wait(mtx, lock, delay)   (mtx)->timer((lock), (delay))
#define __ex_mutex_unlock(mtx, lock)              (mtx)->unlock((lock))


#endif //__cplusplus


#endif //
