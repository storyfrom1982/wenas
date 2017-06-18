//
// Created by kly on 17-6-15.
//

#include "sr_queue.h"


#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "sr_malloc.h"


#define	ISTRUE(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	ISFALSE(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	SETTRUE(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	SETFALSE(x)		__sync_bool_compare_and_swap(&(x), true, false)


#define ATOM_SUB(x, y)      __sync_sub_and_fetch(&(x), (y))
#define ATOM_ADD(x, y)      __sync_add_and_fetch(&(x), (y))
#define ATOM_LOCK(x)        while(!SETTRUE(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define ATOM_TRYLOCK(x)     SETTRUE(x)
#define ATOM_UNLOCK(x)      SETFALSE(x)


struct Sr_queue {
    bool lock;
    bool stopped;
    int size;
    int pushable;
    int popable;
    void (*clean)(Sr_node*);
    Sr_node head;
    Sr_node end;
};


Sr_queue* sr_queue_create(int max_node_number)
{
    Sr_queue *queue = (Sr_queue*)calloc(1, sizeof(Sr_queue));
    if (queue == NULL){
        return NULL;
    }

    queue->lock = false;
    queue->stopped = false;
    queue->size = max_node_number;
    queue->pushable = max_node_number;
    queue->popable = 0;
    queue->head.prev = NULL;
    queue->head.next = &(queue->end);
    queue->end.next = NULL;
    queue->end.prev = &(queue->head);
    queue->clean = NULL;

    return queue;
}

void sr_queue_release(Sr_queue **pp_queue)
{
    if (pp_queue && *pp_queue){
        Sr_queue *queue = *pp_queue;
        *pp_queue = NULL;
        sr_queue_clean(queue);
        free(queue);
    }
}

void sr_queue_set_clean_callback(Sr_queue *queue, void (*cb)(Sr_node*))
{
	if (queue != NULL){
		queue->clean = cb;
	}
}

int sr_queue_push_front(Sr_queue *queue, Sr_node *node)
{
    if (NULL == queue || NULL == node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }

    if (ISTRUE(queue->stopped)){
        ATOM_UNLOCK(queue->lock);
        return QUEUE_RESULT_EOF;
    }

    if (queue->pushable == 0){
        ATOM_UNLOCK(queue->lock);
        return QUEUE_RESULT_TRYAGAIN;
    }

    node->prev = queue->end.prev;
    node->prev->next = node;
    queue->end.prev = node;
    node->next = &(queue->end);

    node->next = queue->head.next;
    node->next->prev = node;
    node->prev = &(queue->head);
    queue->head.next = node;

    queue->popable++;
    queue->pushable--;

    ATOM_UNLOCK(queue->lock);

    return  0;
}

int sr_queue_push_back(Sr_queue *queue, Sr_node *node)
{
    if (NULL == queue || NULL == node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }

    if (ISTRUE(queue->stopped)){
        ATOM_UNLOCK(queue->lock);
        return QUEUE_RESULT_EOF;
    }

    if (queue->pushable == 0){
        ATOM_UNLOCK(queue->lock);
        return QUEUE_RESULT_TRYAGAIN;
    }

    node->prev = queue->end.prev;
    node->prev->next = node;
    queue->end.prev = node;
    node->next = &(queue->end);

    queue->popable++;
    queue->pushable--;

    ATOM_UNLOCK(queue->lock);

    return  0;
}

int sr_queue_pop_front(Sr_queue *queue, Sr_node **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }
    if (queue->popable == 0 || queue->head.next == &(queue->end)){
        ATOM_UNLOCK(queue->lock);
        if (ISTRUE(queue->stopped)){
            return QUEUE_RESULT_EOF;
        }
        return QUEUE_RESULT_TRYAGAIN;
    }

    (*pp_node) = queue->head.next;
    queue->head.next = (*pp_node)->next;
    (*pp_node)->next->prev = &(queue->head);

    queue->pushable++;
    queue->popable--;

    ATOM_UNLOCK(queue->lock);

    return 0;
}

int sr_queue_pop_back(Sr_queue *queue, Sr_node **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }
    if (queue->popable == 0 || queue->head.next == &(queue->end)){
        ATOM_UNLOCK(queue->lock);
        if (ISTRUE(queue->stopped)){
            return QUEUE_RESULT_EOF;
        }
        return QUEUE_RESULT_TRYAGAIN;
    }

    (*pp_node) = queue->end.prev;
    queue->end.prev = (*pp_node)->prev;
    (*pp_node)->prev->next = &(queue->end);

    queue->pushable++;
    queue->popable--;

    ATOM_UNLOCK(queue->lock);

    return 0;
}

int sr_queue_remove(Sr_queue *queue, Sr_node *node)
{
    if (NULL == queue
        || queue->popable <= 0
        || NULL == node
        || NULL == node->prev
        || NULL == node->next){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    ATOM_LOCK(queue->lock);

    if (ISTRUE(queue->stopped)){
        ATOM_UNLOCK(queue->lock);
        return QUEUE_RESULT_EOF;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;

    queue->pushable++;
    queue->popable--;

    ATOM_UNLOCK(queue->lock);

    return  0;
}

int sr_queue_get_front(Sr_queue *queue, Sr_node **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }
    if (queue->popable == 0 || queue->head.next == &(queue->end)){
        ATOM_UNLOCK(queue->lock);
        if (ISTRUE(queue->stopped)){
            return QUEUE_RESULT_EOF;
        }
        return QUEUE_RESULT_TRYAGAIN;
    }

    (*pp_node) = queue->head.next;

    ATOM_UNLOCK(queue->lock);

    return 0;
}

int sr_queue_get_back(Sr_queue *queue, Sr_node **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }

    if (!ATOM_TRYLOCK(queue->lock)){
        return QUEUE_RESULT_TRYAGAIN;
    }
    if (queue->popable == 0 || queue->head.next == &(queue->end)){
        ATOM_UNLOCK(queue->lock);
        if (ISTRUE(queue->stopped)){
            return QUEUE_RESULT_EOF;
        }
        return QUEUE_RESULT_TRYAGAIN;
    }

    (*pp_node) = queue->end.prev;

    ATOM_UNLOCK(queue->lock);

    return 0;
}

void sr_queue_lock(Sr_queue *queue)
{
	if (queue != NULL){
		ATOM_LOCK(queue->lock);
	}
}

void sr_queue_unlock(Sr_queue *queue)
{
	if (queue != NULL){
		ATOM_UNLOCK(queue->lock);
	}
}

Sr_node* sr_queue_front_iterator(Sr_queue *queue)
{
	if (queue != NULL){
		return &(queue->head);
	}
    return NULL;
}

Sr_node* sr_queue_back_iterator(Sr_queue *queue)
{
	if (queue != NULL){
		return &(queue->end);
	}
    return NULL;
}

int sr_queue_pushable(Sr_queue *queue)
{
    if (NULL == queue){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }
    return queue->pushable;
}

int sr_queue_popable(Sr_queue *queue)
{
    if (NULL == queue){
        return QUEUE_RESULT_INVALIED_PARAMATE;
    }
    return queue->popable;
}

void sr_queue_clean(Sr_queue *q)
{
    if (q){
        ATOM_LOCK((q)->lock);
		while ((q)->popable > 0 && (q)->head.next != &((q)->end)){
			(q)->head.prev = (q)->head.next;
			(q)->head.next = (q)->head.next->next;
			if ((q)->clean){
                (q)->clean((q)->head.prev);
            } else{
                free((q)->head.prev);
            }
			(q)->popable--;
		}
		(q)->head.prev = NULL;
		(q)->head.next = &((q)->end);
		(q)->end.next = NULL;
		(q)->end.prev = &((q)->head);
		(q)->pushable = (q)->size;
		(q)->popable = 0;
		ATOM_UNLOCK((q)->lock);
    }
}

void sr_queue_stop(Sr_queue *queue)
{
    if (queue){
        SETTRUE(queue->stopped);
    }
}
