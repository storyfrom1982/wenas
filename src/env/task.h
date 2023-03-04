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
    return strftime(buf, size, "%Y/%m/%d-%H:%M:%S", localtime(&sec));
}


//////////////////////////////////////
//////////////////////////////////////
//////////////////////////////////////


typedef pthread_t ___thread_ptr;

#define ___BAD_THREAD   0

static inline ___thread_ptr ___thread_create(void*(*task_func)(void*), void *ctx)
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, task_func, ctx);
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

#define	___is_true(x)	    	__sync_bool_compare_and_swap((x), true, true)
#define	___is_false(x)	    	__sync_bool_compare_and_swap((x), false, false)
#define	___set_true(x)		    __sync_bool_compare_and_swap((x), false, true)
#define	___set_false(x)		    __sync_bool_compare_and_swap((x), true, false)

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

#define ___atom_sub(x, y)                   (x)->fetch_sub((y))
#define ___atom_add(x, y)                   (x)->fetch_add((y))

#define	___is_true(x)                       (x)->load()
#define	___is_false(x)                      (x)->load()

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
        CxxLock _lock(_mutex);
        return _lock;
    }

    void unlock(CxxLock &_lock){
        _lock.unlock();
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
#include "sys/struct/linekv.h"

#ifdef __cplusplus
}
#endif

#define LINETASK_FALG_POST          (LINEKV_FLAG_EXPANDED << 1)
#define LINETASK_FALG_TIMER         (LINEKV_FLAG_EXPANDED << 2)

typedef struct task* task_ptr;
typedef void (*linetask_post_func)(linekv_ptr ctx);
typedef uint64_t (*linetask_timer_func)(linekv_ptr ctx);

struct task {
    ___atom_bool running;
    ___atom_size length;
    ___atom_size push_waiting;
    ___atom_size pop_waiting;
    ___mutex_ptr mtx;
    ___thread_ptr tid;
    heap_t *timer_list;
    struct linekv head, end;
};

typedef struct task* task_ptr;


static inline int task_push_front(task_ptr queue, linekv_ptr node)
{
    assert(queue != NULL && node != NULL);
    if (queue->length == UINT16_MAX){
        return -1;
    }    
    node->next = queue->head.next;
    node->next->prev = node;
    node->prev = &(queue->head);
    queue->head.next = node;
    ___atom_add(&queue->length, 1);
    return queue->length;
}

static inline int task_push_back(task_ptr queue, linekv_ptr node)
{
    assert(queue != NULL && node != NULL);
    if (queue->length == UINT16_MAX){
        return -1;
    }
    node->prev = queue->end.prev;
    node->next = &(queue->end);
    node->prev->next = node;
    queue->end.prev = node;
    ___atom_add(&queue->length, 1);
    return queue->length;
}

static inline int task_pop_front(task_ptr queue, linekv_ptr *pp_node)
{
    assert(queue != NULL && pp_node != NULL);
    if (queue->length == 0){
        return -1;
    }
    (*pp_node) = queue->head.next;
    queue->head.next = (*pp_node)->next;
    (*pp_node)->next->prev = &(queue->head);
    ___atom_sub(&queue->length, 1);
    return queue->length;
}

static inline int task_pop_back(task_ptr queue, linekv_ptr *pp_node)
{
    assert(queue != NULL && pp_node != NULL);
    if (queue->length == 0){
        return -1;
    }
    (*pp_node) = queue->end.prev;
    queue->end.prev = (*pp_node)->prev;
    (*pp_node)->prev->next = &(queue->end);
    ___atom_sub(&queue->length, 1);
    return queue->length;
}


static void* task_loop(void *p)
{
    uint64_t timeout = 0;
    linekv_ptr timer;
    linekv_ptr task;
    heapment_t element;
    linetask_post_func post_func;
    linetask_timer_func timer_func;
    task_ptr ltq = (task_ptr)p;

    __logi("task_loop(0x%X) enter\n", ___thread_id());
    
    while (___is_true(&ltq->running)) {

        while (ltq->timer_list->pos > 0 && ltq->timer_list->array[1].key <= env_time()){
            element = min_heapify_pop(ltq->timer_list);
            timer = (linekv_ptr) element.value;
            timer_func = (linetask_timer_func)linekv_find_ptr(timer, "func");
            element.key = timer_func(timer); 
            if (element.key != 0){
                element.key += env_time();
                min_heapify_push(ltq->timer_list, element);
            }
        }

        ___lock lk = ___mutex_lock(ltq->mtx);

        if (task_pop_front(ltq, &task) == -1){

            if (___is_false(&ltq->running)){
                ___mutex_unlock(ltq->mtx, lk);
                break;
            }

            ___atom_add(&ltq->pop_waiting, 1);
            if (ltq->timer_list->pos > 0){
                timeout = ltq->timer_list->array[1].key - env_time();
                ___mutex_timer(ltq->mtx, lk, timeout);
            }else {
                ___mutex_wait(ltq->mtx, lk);
            }
            ___atom_sub(&ltq->pop_waiting, 1);

            ___mutex_unlock(ltq->mtx, lk);

        }else {

            if (ltq->push_waiting){
                ___mutex_notify(ltq->mtx);
            }

            ___mutex_unlock(ltq->mtx, lk);

            if (task->flag & LINETASK_FALG_TIMER){

                element.key = linekv_find_uint64(task, "delay") + env_time();
                element.value = task;
                min_heapify_push(ltq->timer_list, element);

            }else {

                post_func = (linetask_post_func)linekv_find_ptr(task, "func");
                (post_func)(task);
            }
        }
    }

    __logi("task_loop(0x%X) exit\n", ___thread_id());

    return NULL;
}


static inline task_ptr task_create()
{
    int ret;
    task_ptr ltq = (task_ptr)malloc(sizeof(struct task));
    assert(ltq);
    ltq->mtx = ___mutex_create();
    assert(ltq->mtx);

    ltq->length = 0;
    ltq->push_waiting = 0;
    ltq->pop_waiting = 0;

    ltq->head.prev = NULL;
    ltq->head.next = &(ltq->end);
    ltq->end.next = NULL;
    ltq->end.prev = &(ltq->head);

    ltq->timer_list = heap_create(UINT8_MAX);
    assert(ltq->timer_list);

    ltq->running = true;
    ltq->tid = ___thread_create(task_loop, ltq);
    assert(ltq->tid);

    return ltq;
}

static inline void task_release(task_ptr *pptr)
{
    if (pptr && *pptr) {
        task_ptr ptr = *pptr;
        *pptr = NULL;
        ___set_false(&ptr->running);

        while (ptr->pop_waiting > 0 || ptr->push_waiting > 0){
            ___lock lk = ___mutex_lock(ptr->mtx);
            ___mutex_notify(ptr->mtx);
            ___mutex_timer(ptr->mtx, lk, 1000);
            ___mutex_unlock(ptr->mtx, lk);
        }

        ___thread_join(ptr->tid);
        ___mutex_release(ptr->mtx);

        while ((ptr)->head.next != &((ptr)->end)){
            (ptr)->head.prev = (ptr)->head.next;
            (ptr)->head.next = (ptr)->head.next->next;
            linekv_release(&(ptr->head.prev));
        }

        while (ptr->timer_list->pos > 0){
            heapment_t element = min_heapify_pop(ptr->timer_list);
            if (element.value){
                linekv_release((linekv_ptr*)&(element.value));
            }
        }

        heap_destroy(&ptr->timer_list);

        free(ptr);
    }
}


static inline int task_post(task_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_POST;
    ___lock lk = ___mutex_lock(ptr->mtx);
    while (task_push_back(ptr, ctx) == -1){
        if (___is_true(&ptr->running)){
            ___atom_add(&ptr->push_waiting, 1);
            ___mutex_wait(ptr->mtx, lk);
            ___atom_sub(&ptr->push_waiting, 1);
        }else {
            ___mutex_unlock(ptr->mtx, lk);
            return -1;
        }
    }
    if (ptr->pop_waiting){
        ___mutex_notify(ptr->mtx);
    }
    ___mutex_unlock(ptr->mtx, lk);
    return ptr->length;
}

static inline int task_timer(task_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_TIMER;
    ___lock lk = ___mutex_lock(ptr->mtx);
    while (task_push_back(ptr, ctx) == -1){
        if (___is_true(&ptr->running)){
            ___atom_add(&ptr->push_waiting, 1);
            ___mutex_wait(ptr->mtx, lk);
            ___atom_sub(&ptr->push_waiting, 1);
        }else {
            ___mutex_unlock(ptr->mtx, lk);
            return -1;
        }
    }
    if (ptr->pop_waiting){
        ___mutex_notify(ptr->mtx);
    }
    ___mutex_unlock(ptr->mtx, lk);
    return ptr->length;
}

static inline int task_immediately(task_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_POST;
    ___lock lk = ___mutex_lock(ptr->mtx);
    while (task_push_front(ptr, ctx) == -1){
        if (___is_true(&ptr->running)){
            ___atom_add(&ptr->push_waiting, 1);
            ___mutex_wait(ptr->mtx, lk);
            ___atom_sub(&ptr->push_waiting, 1);
        }else {
            ___mutex_unlock(ptr->mtx, lk);
            return -1;
        }
    }
    if (ptr->pop_waiting){
        ___mutex_notify(ptr->mtx);
    }
    ___mutex_unlock(ptr->mtx, lk);
    return ptr->length;
}


#endif //__MUTEX_TASK_H__