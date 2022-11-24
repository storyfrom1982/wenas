#ifndef __ENV_TASK_QUEUE__
#define __ENV_TASK_QUEUE__

#include "env.h"
#include "linearkv.h"
#include "lineardb_pipe.h"
#include "heap.h"

typedef struct env_task_queue {
    uint8_t running;
    uint8_t write_waiting;
    uint8_t read_waiting;
    env_thread_t ethread;
    env_mutex_t emutex;
    heap_t *timed_task;
    linedb_pipe_t *immediate_task;
}env_taskqueue_t;

typedef linekv_parser_t task_ctx_t;

typedef void (*env_task_func)(task_ctx_t ctx);
typedef uint64_t (*env_timedtask_func)(task_ctx_t ctx);

static inline void* task_main_loop(void *ctx)
{
    env_taskqueue_t *tq = (env_taskqueue_t *)ctx;
    uint64_t timer = 0;
    linedb_t *ldb;
    task_ctx_t task_ctx = linekv_build(10240);

    while (1) {

        env_mutex_lock(&tq->emutex);

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
                env_mutex_signal(&tq->emutex);
            }
            env_mutex_unlock(&tq->emutex);

            ldb = linekv_find(task_ctx, "func");
            ((env_task_func)__b2n64(ldb))(task_ctx);

        }else {

            if (!tq->running){
                env_mutex_unlock(&tq->emutex);
                break;
            }

            if (tq->timed_task->pos > 0){
                timer = tq->timed_task->array[1].key - env_time();
            }else {
                timer = 0;
            }

            tq->read_waiting = 1;
            if (timer){
                env_mutex_timedwait(&tq->emutex, timer);
            }else {
                env_mutex_wait(&tq->emutex);
            }
            tq->read_waiting = 0;

            env_mutex_unlock(&tq->emutex);
        }
    }

    return NULL;
}


static inline env_taskqueue_t* env_taskqueue_build()
{
    int ret;

    env_taskqueue_t *tq = (env_taskqueue_t *)malloc(sizeof(env_taskqueue_t));
    if (tq == NULL){
        return NULL;
    }

    tq->immediate_task = linedb_pipe_build(1 << 16);
    if (tq->immediate_task == NULL){
        goto Clear;
    }

    tq->timed_task = heap_build(1 << 8);
    if (tq->timed_task == NULL){
        goto Clear;
    }

    ret = env_mutex_init(&tq->emutex);
    if (ret != 0){
        goto Clear;
    }

    tq->running = 1;
    ret = env_thread_create(&tq->ethread, task_main_loop, tq);
    if (ret != 0){
        goto Clear;
    }

    return tq;

Clear:
    if (tq->immediate_task) linedb_pipe_destroy(&tq->immediate_task);
    if (tq->timed_task) heap_destroy(&tq->timed_task);
    if (tq) free(tq);
    return NULL;
}

static inline void env_taskqueue_exit(env_taskqueue_t *tq)
{
    env_mutex_lock(&tq->emutex);
    if (tq->running){
        tq->running = 0;
        env_mutex_broadcast(&tq->emutex);
        env_mutex_unlock(&tq->emutex);
        env_thread_join(tq->ethread);
    }else {
        env_mutex_unlock(&tq->emutex);
    }
}

static inline void env_taskqueue_destroy(env_taskqueue_t **pp_tq)
{
    if (pp_tq && *pp_tq){
        env_taskqueue_t *tq = *pp_tq;
        *pp_tq = NULL;
        env_taskqueue_exit(tq);
        env_mutex_destroy(&tq->emutex);
        while (tq->timed_task->pos > 0){
            heapment_t element = min_heapify_pop(tq->timed_task);
            if (element.value){
                free(element.value);
            }
        }
        heap_destroy(&tq->timed_task);
        linedb_pipe_destroy(&tq->immediate_task); 
        free(tq);
    }
}

static inline void env_taskqueue_push_task(env_taskqueue_t *tq, linekv_t *lkv)
{
    env_mutex_lock(&tq->emutex);
    while (linedb_pipe_write(tq->immediate_task, lkv->head, lkv->pos) == 0) {
        tq->write_waiting = 1;
        env_mutex_wait(&tq->emutex);
        tq->write_waiting = 0;
    }
    if (tq->read_waiting){
        env_mutex_signal(&tq->emutex);
    }
    env_mutex_unlock(&tq->emutex);
}

static inline void env_taskqueue_push_timedtask(env_taskqueue_t *tq, linekv_t *lkv)
{
    env_mutex_lock(&tq->emutex);
    linekv_t *task = linekv_build(lkv->pos);
    task->pos = lkv->pos;
    memcpy(task->head, lkv->head, lkv->pos);
    heapment_t t;
    t.key = env_time() + linekv_find_n64(task, "time");
    t.value = task;
    min_heapify_push(tq->timed_task, t);
    env_mutex_signal(&tq->emutex);
    env_mutex_unlock(&tq->emutex);
}


#endif //__ENV_TASK_QUEUE__