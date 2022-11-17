#ifndef __ENV_TASK_QUEUE__
#define __ENV_TASK_QUEUE__

#include "env.h"
#include "linearkv.h"
#include "lineardb_pipe.h"

typedef struct env_task_queue {
    size_t running;
    EnvThread ethread;
    EnvMutex emutex;
    LineardbPipe *lpipe;
}EnvTaskQueue;


typedef void (*task_func)(Linearkv *lkv);

static void* task_main_loop(void *ctx)
{
    EnvTaskQueue *tq = (EnvTaskQueue *)ctx;
    Lineardb *ldb;
    uint8_t buf[10240];
    Linearkv lkv;
    
    while (tq->running) {

        env_mutex_lock(&tq->emutex);
        ldb = lineardbPipe_hold_block(tq->lpipe);
        if (ldb){
            // fprintf(stderr, "task working\n");
            linearkv_bind_buffer(&lkv, buf, 10240);
            linearkv_load_lineardb(&lkv, ldb);
            // fprintf(stdout, "task working hold kv size %u\n", __sizeof_data(ldb));
            // lkv.pos = __sizeof_data(ldb);
            // memcpy(buf, __dataof_block(ldb), lkv.pos);
            // fprintf(stderr, "task working 1\n");
            lineardbPipe_free_block(tq->lpipe, ldb);
            // fprintf(stderr, "task working 2\n");
            env_mutex_signal(&tq->emutex);
            env_mutex_unlock(&tq->emutex);

            // fprintf(stderr, "task working 3\n");
            ldb = linearkv_find(&lkv, "func");
            // fprintf(stderr, "task working %x\n", __b2n64(ldb));
            ((task_func)__b2n64(ldb))(&lkv);
            // fprintf(stderr, "task working 5\n");

        }else {
            // fprintf(stderr, "task waiting\n");
            if (!tq->running){
                env_mutex_unlock(&tq->emutex);
                break;
            }
            env_mutex_wait(&tq->emutex);
            env_mutex_unlock(&tq->emutex);
        }
    }
    return NULL;
}


static inline EnvTaskQueue* env_taskQueue_create()
{
    int ret;

    EnvTaskQueue *tq = (EnvTaskQueue *)malloc(sizeof(EnvTaskQueue));
    if (tq == NULL){
        fprintf(stderr, "1\n");
        return NULL;
    }

    tq->lpipe = lineardbPipe_create(1 << 16);
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
    if (tq->lpipe) lineardbPipe_release(&tq->lpipe);
    if (tq) free(tq);
    return NULL;
}

static inline void env_taskQueue_push(EnvTaskQueue *tq, Linearkv *lkv)
{
    env_mutex_lock(&tq->emutex);
    // fprintf(stdout, "push kv size %u\n", lkv->pos);
    while (lineardbPipe_write(tq->lpipe, lkv->head, lkv->pos) != lkv->pos) {
        env_mutex_wait(&tq->emutex);
    }
    env_mutex_signal(&tq->emutex);
    env_mutex_unlock(&tq->emutex);
}

static inline void env_taskQueue_flush(EnvTaskQueue *tq)
{
    // tq->running = 0;
    env_thread_join(tq->ethread);
}

#endif //__ENV_TASK_QUEUE__