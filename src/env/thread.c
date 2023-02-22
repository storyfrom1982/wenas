#include "env/env.h"


#if defined(OS_WINDOWS)
#include <Windows.h>
#include <pthread.h>
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;
#else //!defined(OS_WINDOWS)
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;
#endif //defined(OS_WINDOWS)

#include <stdlib.h>
#include <string.h>


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


int32_t env_thread_create(env_thread_ptr *p_ptr, env_thread_cb cb, __ptr ctx)
{
    env_thread_ptr ptr = malloc(sizeof(pthread_t));
    if (ptr == NULL) {
        return -1;
    }
    if (pthread_create((pthread_t*)ptr, NULL, cb, ctx) != 0) {
        free(ptr);
        return -1;
    }
    *p_ptr = ptr;
    return 0;
}

uint64_t env_thread_self()
{
#if defined(OS_WINDOWS)
    return (uint64_t)pthread_self().p;
#else
    return (uint64_t)pthread_self();
#endif
}

uint64_t env_thread_id(env_thread_ptr thread_ptr)
{
#if defined(OS_WINDOWS)
    return (uint64_t)((pthread_t*)thread_ptr)->p;
#else
    return (uint64_t)*((pthread_t*)thread_ptr);
#endif
}

void env_thread_destroy(env_thread_ptr *p_ptr)
{
    if (p_ptr && *p_ptr) {
        env_thread_ptr ptr = *p_ptr;
        *p_ptr = NULL;
        pthread_join(*(pthread_t*)ptr, NULL);
        free(ptr);
    }
}

void env_thread_sleep(uint64_t nano_seconds)
{
#if defined(OS_WINDOWS)
    nano_seconds /= 1000000ULL;
	if (nano_seconds < 1) nano_seconds = 1;
    Sleep(nano_seconds);
#else
    nano_seconds /= 1000ULL;
    if (nano_seconds < 1) nano_seconds = 1;
	usleep(nano_seconds);
#endif
}

int32_t env_thread_mutex_init(env_thread_mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

void env_thread_mutex_destroy(env_thread_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

void env_thread_mutex_lock(env_thread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

void env_thread_mutex_unlock(env_thread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

int32_t env_thread_cond_init(env_thread_cond_t *cond)
{
    return pthread_cond_init(cond, NULL);
}

void env_thread_cond_destroy(env_thread_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

void env_thread_cond_signal(env_thread_cond_t *cond)
{
    pthread_cond_signal(cond);
}

void env_thread_cond_broadcast(env_thread_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

void env_thread_cond_wait(env_thread_cond_t *cond, env_thread_mutex_t *mutex)
{
    pthread_cond_wait(cond, mutex);
}

int32_t env_thread_cond_timedwait(env_thread_cond_t *cond, env_thread_mutex_t *mutex, uint64_t timeout)
{
    int32_t r = 0;
    struct timespec ts;
    timeout += env_time();
    ts.tv_sec = timeout / NANO_SECONDS;
    ts.tv_nsec = timeout % NANO_SECONDS;
    r = pthread_cond_timedwait(cond, mutex, &ts);
    errno = r;
    return r == 0 ? 0 : r == ETIMEDOUT ? ENV_TIMEDOUT : -1;
}

static inline int32_t env_mutex_init(env_mutex_t *mutex)
{
    int32_t r;
    r = env_thread_mutex_init(mutex->mutex);
    if (r == 0){
        r = env_thread_cond_init(mutex->cond);
        if (r != 0){
            env_thread_mutex_destroy(mutex->mutex);
        }
    }
    return r;
}

static inline void env_mutex_uninit(env_mutex_t *mutex)
{
    if (NULL != (void*)mutex->cond){
        env_thread_cond_destroy(mutex->cond);
    }
    if (NULL != (void*)mutex->mutex){
        env_thread_mutex_destroy(mutex->mutex);
    }
}

env_mutex_t* env_mutex_create(void)
{
    env_mutex_t *mutex = (env_mutex_t*)malloc(sizeof(env_mutex_t));
    if (mutex == NULL){
        return NULL;
    }
    if (env_mutex_init(mutex) != 0){
        free(mutex);
        mutex = NULL;
    }
    return mutex;
}

void env_mutex_destroy(env_mutex_t **pp_mutex)
{
    if (pp_mutex && *pp_mutex){
        env_mutex_t *mutex = *pp_mutex;
        *pp_mutex = NULL;
        env_mutex_uninit(mutex);
        free(mutex);
    }
}

void env_mutex_lock(env_mutex_t *mutex)
{
    env_thread_mutex_lock(mutex->mutex);
}

void env_mutex_unlock(env_mutex_t *mutex)
{
    env_thread_mutex_unlock(mutex->mutex);
}

void env_mutex_signal(env_mutex_t *mutex)
{
    env_thread_cond_signal(mutex->cond);
}

void env_mutex_broadcast(env_mutex_t *mutex)
{
    env_thread_cond_broadcast(mutex->cond);
}

void env_mutex_wait(env_mutex_t *mutex)
{
    env_thread_cond_wait(mutex->cond, mutex->mutex);
}

int32_t env_mutex_timedwait(env_mutex_t *mutex, uint64_t timeout)
{
    return env_thread_cond_timedwait(mutex->cond, mutex->mutex, timeout);
}


typedef struct env_pipe {
    uint64_t len;
    uint64_t leftover;
    __atombool writer;
    __atombool reader;

    char *buf;
    __atombool write_waiting;
    __atombool read_waiting;

    __atombool stopped;
    env_mutex_t mutex;
}env_pipe_t;


env_pipe_t* env_pipe_create(uint64_t len)
{
    env_pipe_t *pipe = (env_pipe_t *)malloc(sizeof(env_pipe_t));
    __pass(pipe != NULL);

   if ((len & (len - 1)) == 0){
        pipe->len = len;
    }else{
        pipe->len = 1U;
        do {
            pipe->len <<= 1;
        } while(len >>= 1);
    }

    // if (pipe->len < (1U << 16)){
    //     pipe->len = (1U << 16);
    // }

    pipe->leftover = pipe->len;

    pipe->buf = (char *)malloc(pipe->len);
    __pass(pipe->buf != NULL);

    __pass(env_mutex_init(&pipe->mutex) == 0);

    pipe->read_waiting = pipe->write_waiting = 0;
    pipe->reader = pipe->writer = 0;
    pipe->stopped = false;

    return pipe;

Reset:
    env_mutex_uninit(&pipe->mutex);
    if (pipe->buf) free(pipe->buf);
    if (pipe) free(pipe);
    return NULL;
}

void env_pipe_destroy(env_pipe_t **pp_pipe)
{
    if (pp_pipe && *pp_pipe){
        env_pipe_t *pipe = *pp_pipe;
        *pp_pipe = NULL;
        env_pipe_clear(pipe);
        env_pipe_stop(pipe);
        env_mutex_uninit(&pipe->mutex);
        free(pipe->buf);
        free(pipe);
    }
}

uint64_t env_pipe_write(env_pipe_t *pipe, __ptr data, uint64_t len)
{
    if (pipe->buf == NULL || data == NULL || len == 0){
        return 0;
    }

    if (__is_true(pipe->stopped)){
        return 0;
    }

    env_mutex_lock(&pipe->mutex);

    uint64_t writable, pos = 0;

    while (pos < len) {

        writable = pipe->len - pipe->writer + pipe->reader;
        if (writable > len - pos){
            writable = len - pos;
        }

        if (writable > 0){

            pipe->leftover = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );
            if (pipe->leftover >= writable){
                memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data) + pos, writable);
            }else {
                memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data) + pos, pipe->leftover);
                memcpy(pipe->buf, ((char*)data) + (pos + pipe->leftover), writable - pipe->leftover);
            }
            __atom_add(pipe->writer, writable);
            pos += writable;

            if (pipe->read_waiting > 0){
                env_mutex_signal(&pipe->mutex);
            }

        }else {

            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            }

            __atom_add(pipe->write_waiting, 1);
            env_mutex_wait(&pipe->mutex);
            __atom_sub(pipe->write_waiting, 1);

            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            }  
        }
    }

    if (pipe->read_waiting > 0){
        env_mutex_signal(&pipe->mutex);
    }
    env_mutex_unlock(&pipe->mutex);

    return pos;
}

uint64_t env_pipe_read(env_pipe_t *pipe, __ptr buf, uint64_t len)
{
    if (pipe->buf == NULL || buf == NULL || len == 0){
        return 0;
    }

    env_mutex_lock(&pipe->mutex);

    uint64_t readable, pos = 0;

    while (pos < len) {

        readable = pipe->writer - pipe->reader;
        if (readable > len - pos){
            readable = len - pos;
        }

        if (readable > 0){

            pipe->leftover = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );
            if (pipe->leftover >= readable){
                memcpy(((char*)buf) + pos, pipe->buf + (pipe->reader & (pipe->len - 1)), readable);
            }else {
                memcpy(((char*)buf) + pos, pipe->buf + (pipe->reader & (pipe->len - 1)), pipe->leftover);
                memcpy(((char*)buf) + (pos + pipe->leftover), pipe->buf, readable - pipe->leftover);
            }
            __atom_add(pipe->reader, readable);
            pos += readable;

            if (pipe->write_waiting > 0){
                env_mutex_signal(&pipe->mutex);
            }

        }else {

            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            }
            
            __atom_add(pipe->read_waiting, 1);
            env_mutex_wait(&pipe->mutex);
            __atom_sub(pipe->read_waiting, 1);

            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            } 
        }
    }

    if (pipe->write_waiting > 0){
        env_mutex_signal(&pipe->mutex);
    }
    env_mutex_unlock(&pipe->mutex);

    return pos;
}

uint64_t env_pipe_readable(env_pipe_t *pipe){
    return pipe->writer - pipe->reader;
}

uint64_t env_pipe_writable(env_pipe_t *pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

void env_pipe_stop(env_pipe_t *pipe){
    if (__set_true(pipe->stopped)){
        while (pipe->write_waiting > 0 || pipe->read_waiting > 0) {
            env_mutex_lock(&pipe->mutex);
            env_mutex_broadcast(&pipe->mutex);
            env_mutex_timedwait(&pipe->mutex, 1000);
            env_mutex_unlock(&pipe->mutex);
        }
    }
}

void env_pipe_clear(env_pipe_t *pipe){
    __atom_sub(pipe->reader, pipe->reader);
    __atom_sub(pipe->writer, pipe->writer);
}


#include "sys/struct/heap.h"
#include "sys/struct/linekv.h"


struct env_task_queue {
    __atombool running;
    uint8_t write_waiting;
    uint8_t read_waiting;
    env_thread_ptr tid;
    env_mutex_t mutex;
    heap_t *timed_task;
    linedb_pipe_t *immediate_task;
};


static inline void* env_taskqueue_loop(void *ctx)
{
    uint64_t timer = 0;
    linedb_t *ldb;
    env_taskqueue_t *tq = (env_taskqueue_t *)ctx;
    __pass(tq != NULL);
    linekv_t* task_ctx = linekv_create(10240);
    __pass(task_ctx != NULL);

    while (1) {

        env_mutex_lock(&tq->mutex);

        while (tq->timed_task->pos > 0 && tq->timed_task->array[1].key <= env_time()){
            heapment_t element = min_heapify_pop(tq->timed_task);
            linekv_t* ctx = (linekv_t*) element.value;
            ldb = linekv_find(ctx, "func");
            element.key = ((env_timed_task_ptr)__b2n64(ldb))(ctx); 
            if (element.key != 0){
                element.key += env_time();
                min_heapify_push(tq->timed_task, element);
            }
        }

        ldb = linedb_pipe_hold_block(tq->immediate_task);
        
        if (ldb){

            linekv_load_object(task_ctx, ldb);
            linedb_pipe_free_block(tq->immediate_task, ldb);
            if (tq->write_waiting){
                env_mutex_signal(&tq->mutex);
            }
            env_mutex_unlock(&tq->mutex);

            ldb = linekv_find(task_ctx, "func");
            __pass(ldb != NULL);
            
            env_mutex_t *join = (env_mutex_t *)linekv_find_ptr(task_ctx, "join");
            if (join){
                linekv_t **result = (linekv_t **)linekv_find_ptr(task_ctx, "result");
                *result = ((env_sync_task_ptr)__b2n64(ldb))(task_ctx);
                env_mutex_lock(join);
                env_mutex_signal(join);
                env_mutex_unlock(join);
            }else {
                ((env_task_ptr)__b2n64(ldb))(task_ctx);
            }

        }else {

            if (__is_false(tq->running)){
                env_mutex_unlock(&tq->mutex);
                break;
            }

            if (tq->timed_task->pos > 0){
                timer = tq->timed_task->array[1].key - env_time();
            }else {
                timer = 0;
            }

            tq->read_waiting = 1;
            if (timer){
                env_mutex_timedwait(&tq->mutex, timer);
            }else {
                env_mutex_wait(&tq->mutex);
            }
            tq->read_waiting = 0;

            env_mutex_unlock(&tq->mutex);
        }
    }

Reset:

    linekv_release(&task_ctx);

    __logi("env_taskqueue_loop(0x%X) exit\n", env_thread_self());

    return NULL;
}


env_taskqueue_t* env_taskqueue_create()
{
    int ret;

    env_taskqueue_t *tq = (env_taskqueue_t *)calloc(1, sizeof(env_taskqueue_t));

    __pass(tq != NULL);

    __pass(
        (tq->immediate_task = linedb_pipe_create(1 << 14)) != NULL
    );

    __pass(
        (tq->timed_task = heap_create(1 << 8)) != NULL
    );

    __pass(
        (ret = env_mutex_init(&tq->mutex)) == 0
    );

    tq->running = 1;

    __pass(
        (ret = env_thread_create(&tq->tid, env_taskqueue_loop, tq)) == 0
    );

    return tq;

Reset:
    if (tq->immediate_task) linedb_pipe_destroy(&tq->immediate_task);
    if (tq->timed_task) heap_destroy(&tq->timed_task);
    if (tq) free(tq);
    return NULL;
}

void env_taskqueue_exit(env_taskqueue_t *tq)
{
    if (__set_false(tq->running)){
        env_mutex_lock(&tq->mutex);
        tq->running = 0;
        env_mutex_broadcast(&tq->mutex);
        env_mutex_unlock(&tq->mutex);
        env_thread_destroy(&tq->tid);
    }
}

void env_taskqueue_destroy(env_taskqueue_t **pp_tq)
{
    if (pp_tq && *pp_tq){
        env_taskqueue_t *tq = *pp_tq;
        *pp_tq = NULL;
        env_taskqueue_exit(tq);
        env_mutex_uninit(&tq->mutex);
        while (tq->timed_task->pos > 0){
            heapment_t element = min_heapify_pop(tq->timed_task);
            if (element.value){
                linekv_release((linekv_t**)&(element.value));
            }
        }
        heap_destroy(&tq->timed_task);
        linedb_pipe_destroy(&tq->immediate_task); 
        free(tq);
    }
}

void env_taskqueue_post_task(env_taskqueue_t *tq, linekv_t *lkv)
{
    env_mutex_lock(&tq->mutex);
    while (linedb_pipe_write(tq->immediate_task, lkv->head, lkv->pos) == 0) {
        tq->write_waiting = 1;
        env_mutex_wait(&tq->mutex);
        tq->write_waiting = 0;
    }
    if (tq->read_waiting){
        env_mutex_signal(&tq->mutex);
    }
    env_mutex_unlock(&tq->mutex);
}

void env_taskqueue_insert_timed_task(env_taskqueue_t *tq, linekv_t *lkv)
{
    env_mutex_lock(&tq->mutex);
    linekv_t *task = linekv_create(lkv->pos);
    task->pos = lkv->pos;
    memcpy(task->head, lkv->head, lkv->pos);
    heapment_t t;
    t.key = env_time() + linekv_find_uint64(task, "time");
    t.value = task;
    min_heapify_push(tq->timed_task, t);
    env_mutex_signal(&tq->mutex);
    env_mutex_unlock(&tq->mutex);
}

linekv_t* env_taskqueue_run_sync_task(env_taskqueue_t *tq, linekv_t *lkv)
{
    linekv_t *result = NULL;
    env_mutex_t mutex;
    env_mutex_init(&mutex);
    env_mutex_lock(&mutex);
    linekv_add_ptr(lkv, "join", &mutex);
    linekv_add_ptr(lkv, "result", &result);

    env_mutex_lock(&tq->mutex);
    while (linedb_pipe_write(tq->immediate_task, lkv->head, lkv->pos) == 0) {
        tq->write_waiting = 1;
        env_mutex_wait(&tq->mutex);
        tq->write_waiting = 0;
    }
    if (tq->read_waiting){
        env_mutex_signal(&tq->mutex);
    }
    env_mutex_unlock(&tq->mutex);

    env_mutex_wait(&mutex);
    env_mutex_unlock(&mutex);
    env_mutex_uninit(&mutex);

    return result;
}