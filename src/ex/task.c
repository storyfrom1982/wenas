#include "ex/ex.h"
#include "ex/task.h"


#ifdef __cplusplus
extern "C" {
#endif

#include "sys/struct/heap.h"
#include "sys/struct/xline.h"


#define EX_TASK_BUF_SIZE        256


struct ex_task {
    ___atom_bool running;
    __ex_lock_ptr lock;
    __ex_thread_ptr tid;
    ___atom_size range, rpos, wpos;
    xline_ptr *buf;
};


#define __task_queue_readable(q)      ((uint8_t)((q)->wpos - (q)->rpos))
#define __task_queue_writable(q)      ((uint8_t)((q)->range - 1 - (q)->wpos + (q)->rpos))


static inline int __ex_taskqueue_push(__ex_task_ptr queue, xline_ptr task)
{
    assert(queue != NULL && task != NULL);
    if (__task_queue_writable(queue)){
        queue->buf[queue->wpos] = task;
        ___atom_add(&queue->wpos, 1);
        __ex_notify(queue->lock);     
        return 0;
    }
    return -1;
}

static inline xline_ptr __ex_taskqueue_pop(__ex_task_ptr queue)
{
    assert(queue != NULL);
    if (__task_queue_readable(queue)){
        xline_ptr task = queue->buf[queue->rpos];
        ___atom_add(&queue->rpos, 1);
        __ex_notify(queue->lock);
        return task;
    }
    return NULL;
}

void* __ex_taskqueue_loop(void *p)
{
    int64_t timeout = 0;
    xline_ptr task_ctx;
    struct xline_object task_parser;
    __ex_task_func post_func;
    __ex_task_ptr task = (__ex_task_ptr)p;

    __ex_logi("__ex_taskqueue_loop(0x%X) enter\n", __ex_thread_id());
    
    while (___is_true(&task->running)) {

        if ((task_ctx = __ex_taskqueue_pop(task)) == NULL){

            ___lock lk = __ex_lock(task->lock);
            __ex_notify(task->lock);
			if (___is_true(&task->running)){
				__ex_wait(task->lock, lk);
			}
            __ex_unlock(task->lock, lk);

        }else {

            xline_object_parse(&task_parser, task_ctx);
            post_func = (__ex_task_func)xline_object_find_ptr(&task_parser, "func");
            if (post_func){
                (post_func)(&task_parser);
            }
            free(task_ctx);
        }
    }

    __ex_logi("__ex_taskqueue_loop(0x%X) exit\n", __ex_thread_id());

    return NULL;
}


__ex_task_ptr __ex_task_create()
{
    __ex_logi("__ex_task_create enter\n");

    int ret;
    __ex_task_ptr task = (__ex_task_ptr)malloc(sizeof(struct ex_task));
    assert(task);
    task->lock = __ex_lock_create();
    assert(task->lock);

    task->rpos = 0;
    task->wpos = 0;
    task->range = EX_TASK_BUF_SIZE;

    task->buf = (xline_ptr*)calloc(task->range, sizeof(xline_ptr));

    task->running = true;
    task->tid = __ex_thread_create(__ex_taskqueue_loop, task);
    assert(task->tid);

    __ex_logi("__ex_task_create exit\n");

    return task;
}

void __ex_task_destroy(__ex_task_ptr *pptr)
{
    __ex_logi("__ex_task_destroy enter\n");

    if (pptr && *pptr) {
        __ex_task_ptr task = *pptr;
        *pptr = NULL;

        {			
            ___lock lk = __ex_lock(task->lock);
			___set_false(&task->running);
            __ex_broadcast(task->lock);
            __ex_unlock(task->lock, lk);
        }

        __ex_thread_join(task->tid);
        __ex_lock_destroy(task->lock);

        xline_ptr x;
        while ((x = __ex_taskqueue_pop(task)) != NULL){
            free(x);
        }

        free(task->buf);
        free(task);
    }

    __ex_logi("__ex_task_destroy exit\n");
}


int __ex_task_post(__ex_task_ptr task, __ex_task_ctx_ptr ctx)
{
    while (__ex_taskqueue_push(task, (xline_ptr)ctx->addr) == -1){
		if (___is_true(&task->running)){
			___lock lk = __ex_lock(task->lock);
			__ex_notify(task->lock);
			if (___is_true(&task->running)){
				__ex_wait(task->lock, lk);
			}
			__ex_unlock(task->lock, lk);
		}else {
			return -1;
		}
    }
    return 0;
}


__ex_task_ptr __ex_task_run(__ex_task_func func, void *ctx)
{
    __ex_task_ptr task = __ex_task_create();
    struct xline_object task_obj;
    xline_make_object(&task_obj, 64);
    xline_object_add_ptr(&task_obj, "func", (void*)func);
    xline_object_add_ptr(&task_obj, "ctx", ctx);
    __ex_task_post(task, &task_obj);
    return task;
}



#ifdef __cplusplus
}
#endif