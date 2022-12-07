#ifndef __ENV_TASK_QUEUE__
#define __ENV_TASK_QUEUE__

#include "env.h"
#include "sys/struct/linearkv.h"
#include "sys/struct/lineardb.h"
#include "sys/struct/heap.h"

typedef struct env_task_queue {
    uint8_t running;
    uint8_t write_waiting;
    uint8_t read_waiting;
    env_thread_t tid;
    env_mutex_t mutex;
    heap_t *timed_task;
    linedb_pipe_t *immediate_task;
}env_taskqueue_t;

typedef linekv_parser_t task_ctx_t;

typedef void (*env_task_func)(task_ctx_t ctx);
typedef uint64_t (*env_timedtask_func)(task_ctx_t ctx);
typedef linekey_t* (*env_jointask_func)(task_ctx_t ctx);

static inline void* env_taskqueue_loop(void *ctx)
{
    env_taskqueue_t *tq = (env_taskqueue_t *)ctx;
    uint64_t timer = 0;
    linedb_t *ldb;
    task_ctx_t task_ctx = linekv_create(10240);
    __env_check(tq == NULL, "linekv_create()");

    while (1) {

        env_mutex_lock(&tq->mutex);

        while (tq->timed_task->pos > 0 && tq->timed_task->array[1].key <= env_time()){
            heapment_t element = min_heapify_pop(tq->timed_task);
            task_ctx_t ctx = (task_ctx_t) element.value;
            ldb = linekv_find(ctx, "func");
            element.key = ((env_timedtask_func)__b2n64(ldb))(ctx); 
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
            
            env_mutex_t *join = (env_mutex_t *)linekv_find_ptr(task_ctx, "join");
            if (join){
                linekey_t **result = (linekey_t **)linekv_find_ptr(task_ctx, "result");
                *result = ((env_jointask_func)__b2n64(ldb))(task_ctx);
                env_mutex_lock(join);
                env_mutex_signal(join);
                env_mutex_unlock(join);
            }else {
                ((env_task_func)__b2n64(ldb))(task_ctx);
            }

        }else {

            if (!tq->running){
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

    linekv_destroy(&task_ctx);

    LOGI("TASK QUEUE", "env_taskqueue_loop(0x%X) exit\n", env_thread_self());

    return NULL;
}


static inline env_taskqueue_t* env_taskqueue_create()
{
    int ret;

    env_taskqueue_t *tq = (env_taskqueue_t *)calloc(1, sizeof(env_taskqueue_t));
    __env_check(tq == NULL, "calloc()");

    tq->immediate_task = linedb_pipe_create(1 << 14);
    __env_check(tq->immediate_task == NULL, "linedb_pipe_create()");

    tq->timed_task = heap_create(1 << 8);
    __env_check(tq->timed_task == NULL, "heap_create()");

    ret = env_mutex_init(&tq->mutex);
    __env_check(ret != 0, "env_mutex_init()");

    tq->running = 1;
    ret = env_thread_create(&tq->tid, env_taskqueue_loop, tq);
    __env_check(ret != 0, "env_thread_create()");

    return tq;

Reset:
    if (tq->immediate_task) linedb_pipe_destroy(&tq->immediate_task);
    if (tq->timed_task) heap_destroy(&tq->timed_task);
    if (tq) free(tq);
    return NULL;
}

static inline void env_taskqueue_exit(env_taskqueue_t *tq)
{
    env_mutex_lock(&tq->mutex);
    if (tq->running){
        tq->running = 0;
        env_mutex_broadcast(&tq->mutex);
        env_mutex_unlock(&tq->mutex);
        int ret = env_thread_join(tq->tid);
        if (ret != 0){
            LOGE("TASK QUEUE", "env_thread_join(0x%X) failed. error: %s\n", tq->tid, strerror(ret));
        }
    }else {
        env_mutex_unlock(&tq->mutex);
    }
}

static inline void env_taskqueue_destroy(env_taskqueue_t **pp_tq)
{
    if (pp_tq && *pp_tq){
        env_taskqueue_t *tq = *pp_tq;
        *pp_tq = NULL;
        env_taskqueue_exit(tq);
        env_mutex_destroy(&tq->mutex);
        while (tq->timed_task->pos > 0){
            heapment_t element = min_heapify_pop(tq->timed_task);
            if (element.value){
                linekv_destroy((linekv_t**)&(element.value));
            }
        }
        heap_destroy(&tq->timed_task);
        linedb_pipe_destroy(&tq->immediate_task); 
        free(tq);
    }
}

static inline void env_taskqueue_post_task(env_taskqueue_t *tq, linekv_t *lkv)
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

static inline void env_taskqueue_insert_timedtask(env_taskqueue_t *tq, linekv_t *lkv)
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

static inline linekv_t* env_taskqueue_join_task(env_taskqueue_t *tq, linekv_t *lkv)
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
    env_mutex_destroy(&mutex);

    return result;
}


#endif //__ENV_TASK_QUEUE__