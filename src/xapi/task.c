#include "ex/ex.h"
#include "ex/task.h"
#include "ex/xatom.h"


#ifdef __cplusplus
extern "C" {
#endif


#include "sys/struct/xbuf.h"


#define EX_TASK_BUF_SIZE        256


struct xpeer_task {
    __atom_bool running;
    __xprocess_ptr pid;
    xbuf_ptr buf;
};


// #define __task_queue_readable(q)      ((uint8_t)((q)->wpos - (q)->rpos))
// #define __task_queue_writable(q)      ((uint8_t)((q)->range - 1 - (q)->wpos + (q)->rpos))


static void* xtask_loop(void *p)
{
    xmaker_ptr ctx;
    __xtask_enter post_func;
    xtask_ptr task = (xtask_ptr)p;

    __xlogi("xtask_loop(0x%X) enter\n", __xapi->process_self());
    
    while (__is_true(task->running)) {

        
        if ((ctx = xbuf_hold_reader(task->buf)) == NULL){
            break;
        }

        post_func = (__xtask_enter)xl_find_ptr(ctx, "func");
        if (post_func){
            (post_func)(ctx);
        }

        xbuf_update_reader(task->buf);
    }

    __xlogi("xtask_loop(0x%X) exit\n", __xapi->process_self());

    return NULL;
}


xtask_ptr xtask_create()
{
    __xlogi("xtask_create enter\n");

    int ret;
    xtask_ptr task = (xtask_ptr)malloc(sizeof(struct xtask));
    assert(task);

    task->buf = xbuf_create(2);
    assert(task->buf);

    task->running = true;
    task->pid = __xapi->process_create(xtask_loop, task);
    assert(task->pid);

    __xlogi("xtask_create exit\n");

    return task;
}

void xtask_free(xtask_ptr *pptr)
{
    __xlogi("xtask_free enter\n");

    if (pptr && *pptr) {
        xtask_ptr task = *pptr;
        *pptr = NULL;

        xbuf_break(task->buf);
        // __xapi->process_join(task->tid);
        __xapi->process_free(task->pid);
        xbuf_free(&task->buf);

        free(task);
    }

    __xlogi("xtask_free exit\n");
}


xmaker_ptr xtask_hold_pusher(xtask_ptr task)
{
    return xbuf_hold_writer(task->buf);
}

void xtask_update_pusher(xtask_ptr task)
{
    xbuf_update_writer(task->buf);
}

xtask_ptr xtask_run(__xtask_enter func, void *ctx)
{
    xtask_ptr task = xtask_create();
    xmaker_ptr maker = xtask_hold_pusher(task);
    xl_add_ptr(maker, "func", (void*)func);
    xl_add_ptr(maker, "ctx", ctx);
    xtask_update_pusher(task);
    return task;
}



#ifdef __cplusplus
}
#endif