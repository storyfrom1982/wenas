#ifndef __ENV_TASK_QUEUE__
#define __ENV_TASK_QUEUE__

#include "env.h"
#include "linearkv.h"
#include "lineardb_pipe.h"

typedef struct env_task_queue {
    uint8_t running;
    uint8_t write_waiting;
    uint8_t read_waiting;
    env_thread_t ethread;
    env_mutex_t emutex;
    lineardb_pipe_t *lpipe;
}env_taskqueue_t;


typedef void (*env_task_func)(linearkv_parser_t parser);

static inline void* task_main_loop(void *ctx)
{
    env_taskqueue_t *tq = (env_taskqueue_t *)ctx;
    lineardb_t *ldb;
    linearkv_builder_t lkv;
    lkv_clear(&lkv);
    
    while (1) {

        env_mutex_lock(&tq->emutex);
        // ldb = lineardbPipe_hold_block(tq->lpipe);
        lkv.pos = lineardb_pipe_read(tq->lpipe, lkv.head, lkv.len);
        if (lkv.pos > lkv.len){
            fprintf(stderr, "Cannot read block: size %u\n", lkv.pos);
            break;
        }
        // if (ldb)
        if (lkv.pos != 0)
        {
            // fprintf(stderr, "task working\n");
            // linearkv_bind_buffer(&lkv, buf, 10240);
            // lkv_load_lineardb(&lkv, ldb);
            // fprintf(stdout, "task working hold kv size %u\n", __sizeof_data(ldb));
            // lkv.pos = __sizeof_data(ldb);
            // memcpy(buf, __dataof_block(ldb), lkv.pos);
            // fprintf(stderr, "task working 1\n");
            // lineardbPipe_free_block(tq->lpipe, ldb);
            // fprintf(stderr, "task working 2\n");
            if (tq->write_waiting){
                env_mutex_signal(&tq->emutex);
            }
            env_mutex_unlock(&tq->emutex);

            // fprintf(stderr, "task working 3\n");
            ldb = lkv_find(&lkv, "func");
            // ldb = lkv_find(&lkv, "string");
            // fprintf(stderr, "task working %s\n", __dataof_block(ldb));
            ((env_task_func)__b2n64(ldb))(&lkv);
            // fprintf(stderr, "task working 5\n");

        }else {
            // fprintf(stderr, "task waiting\n");
            if (!tq->running){
                env_mutex_unlock(&tq->emutex);
                break;
            }
            tq->read_waiting = 1;
            env_mutex_wait(&tq->emutex);
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
        fprintf(stderr, "1\n");
        return NULL;
    }

    tq->lpipe = lineardb_pipe_build(1 << 16);
    if (tq->lpipe == NULL){
        fprintf(stderr, "2\n");
        goto Clear;
    }

    ret = env_mutex_init(&tq->emutex);
    if (ret != 0){
        fprintf(stderr, "3\n");
        goto Clear;
    }

    tq->running = 1;
    ret = env_thread_create(&tq->ethread, task_main_loop, tq);
    if (ret != 0){
        fprintf(stderr, "4\n");
        goto Clear;
    }

    return tq;

Clear:
    if (tq->lpipe) lineardb_pipe_destroy(&tq->lpipe);
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
        lineardb_pipe_destroy(&tq->lpipe);
        free(tq);
    }
}

static inline void env_taskqueue_push(env_taskqueue_t *tq, linearkv_builder_t *lkv)
{
    env_mutex_lock(&tq->emutex);
    while (lineardb_pipe_write(tq->lpipe, lkv->head, lkv->pos) == 0) {
        tq->write_waiting = 1;
        env_mutex_wait(&tq->emutex);
        tq->write_waiting = 0;
    }
    if (tq->read_waiting){
        env_mutex_signal(&tq->emutex);
    }
    env_mutex_unlock(&tq->emutex);
}


#endif //__ENV_TASK_QUEUE__