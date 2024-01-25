#ifndef __MUTEX_TASK_H__
#define __MUTEX_TASK_H__

#define ___TIMEUP           1

#define ___NANO_SECONDS     1000000000ULL

#ifndef __cplusplus

#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static inline uint64_t ___sys_time(void)
{
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * ___NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (__uint64)tv.tv_sec * ___NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

///@return nanoseconds(relative time)
static inline uint64_t ___sys_clock(void)
{
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * ___NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * ___NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

static inline uint64_t ___sys_strftime(char *buf, uint64_t size, uint64_t seconds)
{
	time_t sec = (time_t)seconds;
    struct tm t;
    localtime_r(&sec, &t);
    return strftime(buf, size, "%Y/%m/%d-%H:%M:%S", &t);
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef pthread_t ___thread_ptr;

#define ___BAD_THREAD   0

static inline ___thread_ptr ___thread_create(void*(*task_func)(void*), void *ctx)
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

static inline void ___thread_join(___thread_ptr ptr)
{
    pthread_join(ptr, NULL);
}

static inline uint64_t ___thread_id()
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

#define ___atom_lock(x)			while(!___set_true(x)) nanosleep((const struct timespec[]){{0, 100L}}, NULL)
#define ___atom_try_lock(x)		___set_true(x)
#define ___atom_unlock(x)		___set_false(x)


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef struct mutex {
    pthread_cond_t cond[1];
    pthread_mutex_t mutex[1];
}*___mutex_ptr;

typedef pthread_mutex_t*      ___lock;

static inline ___mutex_ptr ___mutex_create()
{
    int ret;
    ___mutex_ptr mptr = (___mutex_ptr)malloc(sizeof(struct mutex));
    assert(mptr);
    ret = pthread_mutex_init(mptr->mutex, NULL);
    assert(ret == 0);
    ret = pthread_cond_init(mptr->cond, NULL);
    assert(ret == 0);
    return mptr;
}

static inline void ___mutex_release(___mutex_ptr mptr)
{
    int ret;
    assert(mptr);
    ret = pthread_mutex_destroy(mptr->mutex);
    assert(ret == 0);
    ret = pthread_cond_destroy(mptr->cond);
    assert(ret == 0);
    free(mptr);
}

static inline pthread_mutex_t* ___mutex_lock(___mutex_ptr mptr)
{
    assert(mptr);
    pthread_mutex_lock(mptr->mutex);
    return mptr->mutex;
}

static inline void ___mutex_notify(___mutex_ptr mptr)
{
    assert(mptr);
    pthread_cond_signal(mptr->cond);
}

static inline void ___mutex_broadcast(___mutex_ptr mptr)
{
    assert(mptr);
    pthread_cond_broadcast(mptr->cond);
}

static inline void ___mutex_wait(___mutex_ptr mptr, pthread_mutex_t *lock)
{
    assert(mptr);
    pthread_cond_wait(mptr->cond, lock);
}

static inline int ___mutex_timer(___mutex_ptr mptr, pthread_mutex_t *lock, uint64_t delay)
{
    assert(mptr);
    struct timespec ts;
    delay += ___sys_time();
    ts.tv_sec = delay / ___NANO_SECONDS;
    ts.tv_nsec = delay % ___NANO_SECONDS;
    if (pthread_cond_timedwait(mptr->cond, lock, &ts) == ETIMEDOUT){
        return ___TIMEUP;
    }
    return 0;
}

static inline void ___mutex_unlock(___mutex_ptr mptr, pthread_mutex_t *lock)
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


static inline uint64_t ___sys_time()
{
    return std::chrono::system_clock::now().time_since_epoch().count() * 1000;
}

static inline uint64_t ___sys_clock()
{ 
    return std::chrono::steady_clock::now().time_since_epoch().count();
}

static inline uint64_t ___sys_strftime(char *buf, uint64_t size, uint64_t seconds)
{
    return std::strftime(buf, size, "%Y/%m/%d-%H:%M:%S", std::localtime((const time_t*)&seconds));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef void* ___thread_ptr;

#define ___BAD_THREAD   nullptr

static inline ___thread_ptr ___thread_create(void*(*task_func)(void*), void *ctx)
{
    // std::thread *thread_ptr = new std::thread([](void*(*task_func)(void*), void *ctx){
    //     task_func(ctx);
    // }, task_func, ctx);

    std::thread *thread_ptr = new std::thread(task_func, ctx);
    if (thread_ptr != nullptr){
        return (___thread_ptr)thread_ptr;
    }
    return ___BAD_THREAD;
}

static inline void ___thread_join(___thread_ptr ptr)
{
    std::thread *thread_ptr = (std::thread *)ptr;
    thread_ptr->join();
    delete thread_ptr;
}

static inline uint64_t ___thread_id()
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
        std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::nanoseconds(100))
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
            return ___TIMEUP;
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
typedef struct CxxMutex*    ___mutex_ptr;

static inline ___mutex_ptr ___mutex_create()
{
    ___mutex_ptr ptr = new CxxMutex();
    assert(ptr);
    return ptr;
}

static inline void ___mutex_release(___mutex_ptr mtx)
{
    if (mtx){
        delete mtx;
    }
}

#define ___mutex_lock(mtx)                 (mtx)->lock()
#define ___mutex_notify(mtx)               (mtx)->notify()
#define ___mutex_broadcast(mtx)            (mtx)->broadcast()
#define ___mutex_wait(mtx, lock)           (mtx)->wait((lock))
#define ___mutex_timer(mtx, lock, delay)   (mtx)->timer((lock), (delay))
#define ___mutex_unlock(mtx, lock)         (mtx)->unlock((lock))


#endif //__cplusplus


#ifdef __cplusplus
extern "C" {
#endif

#include "sys/struct/heap.h"
#include "sys/struct/xline.h"

#ifdef __cplusplus
}
#endif

#define EX_TASK_BUF_SIZE        256

typedef void (*ex_task_func)(xline_object_ptr ctx);

typedef struct taskqueue {
    ___atom_bool running;
    ___atom_bool push_waiting;
    ___atom_bool pop_waiting;
    ___mutex_ptr lock;
    ___thread_ptr tid;
    ___atom_size range, rpos, wpos;
    xline_ptr *buf;
}*taskqueue_ptr;


#define __task_queue_readable(q)      ((uint8_t)((q)->wpos - (q)->rpos))
#define __task_queue_writable(q)      ((uint8_t)((q)->range - 1 - (q)->wpos + (q)->rpos))


static inline int taskqueue_push(taskqueue_ptr queue, xline_ptr task)
{
    assert(queue != NULL && task != NULL);
    if (__task_queue_writable(queue)){
        queue->buf[queue->wpos] = task;
        ___atom_add(&queue->wpos, 1);
        if (___is_true(&queue->pop_waiting)){
            ___mutex_notify(queue->lock);
        }        
        return 0;
    }
    return -1;
}

static inline xline_ptr taskqueue_pop(taskqueue_ptr queue)
{
    assert(queue != NULL);
    if (__task_queue_readable(queue)){
        xline_ptr task = queue->buf[queue->rpos];
        ___atom_add(&queue->rpos, 1);
        if (___is_true(&queue->push_waiting)){
            ___mutex_notify(queue->lock);
        }
        return task;
    }
    return NULL;
}

static void* taskqueue_mainloop(void *p)
{
    int64_t timeout = 0;
    xline_ptr task_ctx;
    struct xline_object task_parser;
    ex_task_func post_func;
    taskqueue_ptr tq = (taskqueue_ptr)p;

    __ex_logi("taskqueue_mainloop(0x%X) enter\n", ___thread_id());
    
    while (___is_true(&tq->running)) {

        if ((task_ctx = taskqueue_pop(tq)) == NULL){

            if (___is_false(&tq->running)){
                break;
            }

            ___lock lk = ___mutex_lock(tq->lock);
            ___set_true(&tq->pop_waiting);
            ___mutex_wait(tq->lock, lk);
            ___set_false(&tq->pop_waiting);
            ___mutex_unlock(tq->lock, lk);

        }else {
            printf("ctx size=%lu\n", __xline_sizeof(task_ctx));
            xline_object_parse(&task_parser, task_ctx);
            post_func = (ex_task_func)xline_object_find_ptr(&task_parser, "func");
            if (post_func){
                (post_func)(&task_parser);
            }
            free(task_ctx);
        }
    }

    __ex_logi("taskqueue_mainloop(0x%X) exit\n", ___thread_id());

    return NULL;
}


static inline taskqueue_ptr taskqueue_create()
{
    printf("taskqueue_create() enter\n");
    int ret;
    taskqueue_ptr tq = (taskqueue_ptr)malloc(sizeof(struct taskqueue));
    assert(tq);
    printf("taskqueue_create() 1\n");
    tq->lock = ___mutex_create();
    assert(tq->lock);
    printf("taskqueue_create() 2\n");

    tq->rpos = 0;
    tq->wpos = 0;
    tq->range = EX_TASK_BUF_SIZE;
    tq->push_waiting = 0;
    tq->pop_waiting = 0;

    printf("taskqueue_create() 3\n");
    tq->buf = calloc(tq->range, sizeof(xline_ptr));

    printf("taskqueue_create() 4\n");
    tq->running = true;
    tq->tid = ___thread_create(taskqueue_mainloop, tq);
    printf("tid=============%0x\n", tq->tid);
    assert(tq->tid);

    return tq;
}

static inline void taskqueue_release(taskqueue_ptr *pptr)
{
    if (pptr && *pptr) {
        printf("taskqueue_release enter\n");
        taskqueue_ptr tq = *pptr;
        *pptr = NULL;
        ___set_false(&tq->running);

        while (tq->pop_waiting > 0 || tq->push_waiting > 0){
            ___lock lk = ___mutex_lock(tq->lock);
            ___mutex_notify(tq->lock);
            ___mutex_timer(tq->lock, lk, 1000);
            ___mutex_unlock(tq->lock, lk);
        }

        printf("join tid=============%0x\n", tq->tid);
        ___thread_join(tq->tid);
        // pthread_join(tq->tid, NULL);
        ___mutex_release(tq->lock);

        xline_ptr x;
        while ((x = taskqueue_pop(tq)) != NULL){
            free(x);
        }

        free(tq->buf);
        free(tq);
        printf("taskqueue_release exit\n");
    }
}


static inline int taskqueue_post(taskqueue_ptr tq, xline_ptr ctx)
{
    while (taskqueue_push(tq, ctx) == -1){
        if (___is_true(&tq->running)){
            ___lock lk = ___mutex_lock(tq->lock);
            ___set_true(&tq->push_waiting);
            ___mutex_wait(tq->lock, lk);
            ___set_false(&tq->push_waiting);
            ___mutex_unlock(tq->lock, lk);
        }else {
            return -1;
        }
    }
    return 0;
}


static inline taskqueue_ptr taskqueue_run(ex_task_func func, void *ctx)
{
    taskqueue_ptr tq = taskqueue_create();
    struct xline_object task_obj;
    xline_make_object(&task_obj, 64);
    xline_object_add_ptr(&task_obj, "func", (void*)func);
    xline_object_add_ptr(&task_obj, "ctx", ctx);
    taskqueue_post(tq, (xline_ptr)task_obj.addr);
    return tq;
}


#endif //__MUTEX_TASK_H__
