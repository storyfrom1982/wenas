#include "linetask.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "sys/struct/heap.h"


#define LINETASK_FALG_POST          (LINEKV_FLAG_EXPANDED << 1)
#define LINETASK_FALG_TIMER         (LINEKV_FLAG_EXPANDED << 2)


struct linetask {
    uint16_t length;
    int push_waiting;
    int pop_waiting;
    bool running;
    pthread_t tid;
    pthread_cond_t cond[1];
    pthread_mutex_t mutex[1];
    heap_t *timer_list;
    struct linekv head, end;
};


static inline int linetask_push_front(linetask_ptr queue, linekv_ptr node)
{
    assert(queue != NULL && node != NULL);
    if (queue->length == UINT16_MAX){
        return -1;
    }    
    node->next = queue->head.next;
    node->next->prev = node;
    node->prev = &(queue->head);
    queue->head.next = node;
    __atom_add(queue->length, 1);
    return queue->length;
}

static inline int linetask_push_back(linetask_ptr queue, linekv_ptr node)
{
    assert(queue != NULL && node != NULL);
    if (queue->length == UINT16_MAX){
        return -1;
    }
    node->prev = queue->end.prev;
    node->next = &(queue->end);
    node->prev->next = node;
    queue->end.prev = node;
    __atom_add(queue->length, 1);
    return queue->length;
}

static inline int linetask_pop_front(linetask_ptr queue, linekv_ptr *pp_node)
{
    assert(queue != NULL && pp_node != NULL);
    if (queue->length == 0){
        return -1;
    }
    (*pp_node) = queue->head.next;
    queue->head.next = (*pp_node)->next;
    (*pp_node)->next->prev = &(queue->head);
    __atom_sub(queue->length, 1);
    return queue->length;
}

static inline int linetask_pop_back(linetask_ptr queue, linekv_ptr *pp_node)
{
    assert(queue != NULL && pp_node != NULL);
    if (queue->length == 0){
        return -1;
    }
    (*pp_node) = queue->end.prev;
    queue->end.prev = (*pp_node)->prev;
    (*pp_node)->prev->next = &(queue->end);
    __atom_sub(queue->length, 1);
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
    linetask_ptr ltq = (linetask_ptr)p;

    __ex_logi("linetask_loop(0x%X) enter\n", pthread_self());
    
    while (__is_true(ltq->running)) {

        while (ltq->timer_list->pos > 0 && ltq->timer_list->array[1].key <= env_time()){
            element = xheap_pop(ltq->timer_list);
            timer = (linekv_ptr) element.value;
            timer_func = (linetask_timer_func)linekv_find_ptr(timer, "func");
            element.key = timer_func(timer); 
            if (element.key != 0){
                element.key += env_time();
                xheap_push(ltq->timer_list, element);
            }
        }

        pthread_mutex_lock(ltq->mutex);

        if (linetask_pop_front(ltq, &task) == -1){

            if (__is_false(ltq->running)){
                pthread_mutex_unlock(ltq->mutex);
                break;
            }

            __atom_add(ltq->pop_waiting, 1);
            if (ltq->timer_list->pos > 0){
                timeout = ltq->timer_list->array[1].key - env_time();
                linetask_timedwait(ltq, timeout);
            }else {
                pthread_cond_wait(ltq->cond, ltq->mutex);
            }
            __atom_sub(ltq->pop_waiting, 1);

            pthread_mutex_unlock(ltq->mutex);

        }else {

            if (ltq->push_waiting){
                pthread_cond_signal(ltq->cond);
            }

            pthread_mutex_unlock(ltq->mutex);

            if (task->flag & LINETASK_FALG_TIMER){

                element.key = linekv_find_uint64(task, "delay") + env_time();
                element.value = task;
                xheap_push(ltq->timer_list, element);

            }else {

                post_func = (linetask_post_func)linekv_find_ptr(task, "func");
                (post_func)(task);
            }
        }
    }

    __ex_logi("linetask_loop(0x%X) exit\n", pthread_self());

    return NULL;
}


linetask_ptr linetask_create()
{
    int ret;
    linetask_ptr ltq = (linetask_ptr)malloc(sizeof(struct linetask));
    assert(ltq);
    ret = pthread_mutex_init(ltq->mutex, NULL);
    assert(ret == 0);
    ret = pthread_cond_init(ltq->cond, NULL);
    assert(ret == 0);

    ltq->length = 0;
    ltq->push_waiting = 0;
    ltq->pop_waiting = 0;

    ltq->head.prev = NULL;
    ltq->head.next = &(ltq->end);
    ltq->end.next = NULL;
    ltq->end.prev = &(ltq->head);

    ltq->timer_list = xheap_create(UINT8_MAX);
    assert(ltq->timer_list);

    ltq->running = true;
    ret = pthread_create(&ltq->tid, NULL, task_loop, ltq);
    assert(ret == 0);

    return ltq;
}

void linetask_release(linetask_ptr *pptr)
{
    if (pptr && *pptr) {
        linetask_ptr ptr = *pptr;
        *pptr = NULL;
        __set_false(ptr->running);

        while (ptr->pop_waiting > 0 || ptr->push_waiting > 0){
            pthread_mutex_lock(ptr->mutex);
            pthread_cond_signal(ptr->cond);
            linetask_timedwait(ptr, 1000);
            pthread_mutex_unlock(ptr->mutex);
        }

        pthread_join(ptr->tid, NULL);
        pthread_cond_destroy(ptr->cond);
        pthread_mutex_destroy(ptr->mutex);

        while ((ptr)->head.next != &((ptr)->end)){
            (ptr)->head.prev = (ptr)->head.next;
            (ptr)->head.next = (ptr)->head.next->next;
            linekv_release(&(ptr->head.prev));
        }

        while (ptr->timer_list->pos > 0){
            heapment_t element = xheap_pop(ptr->timer_list);
            if (element.value){
                linekv_release((linekv_ptr*)&(element.value));
            }
        }

        xheap_free(&ptr->timer_list);

        free(ptr);
    }
}

void linetask_lock(linetask_ptr ptr)
{
    pthread_mutex_lock(ptr->mutex);
}

void linetask_unlock(linetask_ptr ptr)
{
    pthread_mutex_unlock(ptr->mutex);
}

void linetask_signal(linetask_ptr ptr)
{
    pthread_cond_signal(ptr->cond);
}

void linetask_broadcast(linetask_ptr ptr)
{
    pthread_cond_broadcast(ptr->cond);
}

void linetask_wait(linetask_ptr ptr)
{
    pthread_cond_wait(ptr->cond, ptr->mutex);
}

int linetask_timedwait(linetask_ptr ptr, uint64_t timeout)
{
    struct timespec ts;
    timeout += env_time();
    ts.tv_sec = timeout / NANO_SECONDS;
    ts.tv_nsec = timeout % NANO_SECONDS;
    return pthread_cond_timedwait(ptr->cond, ptr->mutex, &ts);
}

int linetask_post(linetask_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_POST;
    pthread_mutex_lock(ptr->mutex);
    while (linetask_push_back(ptr, ctx) == -1){
        if (__is_true(ptr->running)){
            __atom_add(ptr->push_waiting, 1);
            pthread_cond_wait(ptr->cond, ptr->mutex);
            __atom_sub(ptr->push_waiting, 1);
        }else {
            pthread_mutex_unlock(ptr->mutex);        
            return -1;
        }
    }
    if (ptr->pop_waiting){
        pthread_cond_signal(ptr->cond);
    }
    pthread_mutex_unlock(ptr->mutex);
    return ptr->length;
}

int linetask_timer(linetask_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_TIMER;
    pthread_mutex_lock(ptr->mutex);
    while (linetask_push_back(ptr, ctx) == -1){
        if (__is_true(ptr->running)){
            __atom_add(ptr->push_waiting, 1);
            pthread_cond_wait(ptr->cond, ptr->mutex);
            __atom_sub(ptr->push_waiting, 1);
        }else {
            pthread_mutex_unlock(ptr->mutex);        
            return -1;
        }
    }
    if (ptr->pop_waiting){
        pthread_cond_signal(ptr->cond);
    }
    pthread_mutex_unlock(ptr->mutex);
    return ptr->length;
}

int linetask_immediately(linetask_ptr ptr, linekv_ptr ctx)
{
    ctx->flag |= LINETASK_FALG_POST;
    pthread_mutex_lock(ptr->mutex);
    while (linetask_push_front(ptr, ctx) == -1){
        if (__is_true(ptr->running)){
            __atom_add(ptr->push_waiting, 1);
            pthread_cond_wait(ptr->cond, ptr->mutex);
            __atom_sub(ptr->push_waiting, 1);
        }else {
            pthread_mutex_unlock(ptr->mutex);        
            return -1;
        }
    }
    if (ptr->pop_waiting){
        pthread_cond_signal(ptr->cond);
    }
    pthread_mutex_unlock(ptr->mutex);
    return ptr->length;
}