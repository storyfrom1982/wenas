//
// Created by kly on 17-6-15.
//

#ifndef __SR_QUEUE_H__
#define __SR_QUEUE_H__


enum {
    QUEUE_RESULT_EOF = -3,
    QUEUE_RESULT_TRYAGAIN = -2,
    QUEUE_RESULT_INVALIED_PARAMATE = -1,
	QUEUE_RESULT_OK = 0
};


typedef struct Sr_node{
    struct Sr_node *prev;
    struct Sr_node *next;
}Sr_node;


typedef struct Sr_queue Sr_queue;


Sr_queue* sr_queue_create(int max_node_number);
void sr_queue_release(Sr_queue **pp_queue);

void sr_queue_set_clean_callback(Sr_queue *queue, void (*cb)(Sr_node*));

void sr_queue_lock(Sr_queue *queue);
void sr_queue_unlock(Sr_queue *queue);

int sr_queue_push_front(Sr_queue *queue, Sr_node *node);
int sr_queue_push_back(Sr_queue *queue, Sr_node *node);

int sr_queue_pop_front(Sr_queue *queue, Sr_node **pp_node);
int sr_queue_pop_back(Sr_queue *queue, Sr_node **pp_node);

int sr_queue_remove(Sr_queue *queue, Sr_node *node);

int sr_queue_get_front(Sr_queue *queue, Sr_node **pp_node);
int sr_queue_get_back(Sr_queue *queue, Sr_node **pp_node);

Sr_node* sr_queue_front_iterator(Sr_queue *queue);
Sr_node* sr_queue_back_iterator(Sr_queue *queue);

int sr_queue_pushable(Sr_queue *queue);
int sr_queue_popable(Sr_queue *queue);

void sr_queue_clean(Sr_queue *queue);
void sr_queue_stop(Sr_queue *queue);



#endif //__SR_QUEUE_H__
