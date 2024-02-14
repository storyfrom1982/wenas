#include "ex/ex.h"
#include "ex/task.h"
#include "ex/xatom.h"


#ifdef __cplusplus
extern "C" {
#endif


#include "sys/struct/xline.h"


#define EX_TASK_BUF_SIZE        256


struct ex_task {
    __atom_bool running;
    __ex_thread_ptr tid;
    __ex_msg_pipe *pipe;
};


#define __task_queue_readable(q)      ((uint8_t)((q)->wpos - (q)->rpos))
#define __task_queue_writable(q)      ((uint8_t)((q)->range - 1 - (q)->wpos + (q)->rpos))


static void* __ex_task_loop(void *p)
{
    xmaker_ptr ctx;
    __ex_task_func post_func;
    __ex_task_ptr task = (__ex_task_ptr)p;

    __xlogi("__ex_task_loop(0x%X) enter\n", __ex_thread_id());
    
    while (__is_true(task->running)) {

        
        if ((ctx = __ex_msg_pipe_hold_reader(task->pipe)) == NULL){
            break;
        }

        post_func = (__ex_task_func)xline_find_ptr(ctx, "func");
        if (post_func){
            (post_func)(ctx);
        }

        __ex_msg_pipe_update_reader(task->pipe);
    }

    __xlogi("__ex_task_loop(0x%X) exit\n", __ex_thread_id());

    return NULL;
}


__ex_task_ptr __ex_task_create()
{
    __xlogi("__ex_task_create enter\n");

    int ret;
    __ex_task_ptr task = (__ex_task_ptr)malloc(sizeof(struct ex_task));
    assert(task);

    task->pipe = __ex_msg_pipe_create(256);
    assert(task->pipe);

    task->running = true;
    task->tid = __ex_thread_create(__ex_task_loop, task);
    assert(task->tid);

    __xlogi("__ex_task_create exit\n");

    return task;
}

void __ex_task_free(__ex_task_ptr *pptr)
{
    __xlogi("__ex_task_free enter\n");

    if (pptr && *pptr) {
        __ex_task_ptr task = *pptr;
        *pptr = NULL;

        __ex_msg_pipe_break(task->pipe);
        __ex_thread_join(task->tid);
        __ex_msg_pipe_free(&task->pipe);

        free(task);
    }

    __xlogi("__ex_task_free exit\n");
}


xmaker_ptr __ex_task_hold_pusher(__ex_task_ptr task)
{
    return __ex_msg_pipe_hold_writer(task->pipe);
}

void __ex_task_update_pusher(__ex_task_ptr task)
{
    __ex_msg_pipe_update_writer(task->pipe);
}

__ex_task_ptr __ex_task_run(__ex_task_func func, void *ctx)
{
    __ex_task_ptr task = __ex_task_create();
    xmaker_ptr maker = __ex_task_hold_pusher(task);
    xline_add_ptr(maker, "func", (void*)func);
    xline_add_ptr(maker, "ctx", ctx);
    __ex_task_update_pusher(task);
    return task;
}



#ifdef __cplusplus
}
#endif