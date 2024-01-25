#ifndef __LINETASK_H__
#define __LINETASK_H__

#include "sys/struct/linekv.h"

typedef struct linetask* linetask_ptr;
typedef void (*linetask_post_func)(linekv_ptr ctx);
typedef uint64_t (*linetask_timer_func)(linekv_ptr ctx);

linetask_ptr linetask_create();
void linetask_release(linetask_ptr *pptr);

void linetask_lock(linetask_ptr ptr);
void linetask_unlock(linetask_ptr ptr);

void linetask_signal(linetask_ptr ptr);
void linetask_broadcast(linetask_ptr ptr);

void linetask_wait(linetask_ptr ptr);
int linetask_timedwait(linetask_ptr ptr, uint64_t timeout);

int linetask_post(linetask_ptr ptr, linekv_ptr ctx);
int linetask_timer(linetask_ptr ptr, linekv_ptr ctx);
int linetask_immediately(linetask_ptr ptr, linekv_ptr ctx);


#endif