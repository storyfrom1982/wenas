#ifndef __HEAP_H__
#define __HEAP_H__

#include <stdlib.h>
#include <stdint.h>


typedef struct heap_element {
    uint64_t key;
    void *value;
}heapment_t;


typedef struct heap {
    uint32_t len, pos;
    heapment_t array[1];
}heap_t;


#define __heap_top(i)       (i/2)
#define __heap_left(i)      (2*i+1)
#define __heap_right(i)     (2*i+2)



static inline heap_t* heap_build(uint32_t size)
{
    heap_t *heap = malloc(sizeof(heap_t) + sizeof(heapment_t) * size);
    heap->len = size;
    heap->pos = 0;
    return heap;
}

static inline void heap_destroy(heap_t **pp_heap)
{
    if (pp_heap && *pp_heap) {
        heap_t *heap = *pp_heap;
        *pp_heap = NULL;
        free(heap);
    }
}

//ascending/descending 升序/降序 最小堆/最大堆
static inline int heap_aes_push(heap_t *h, heapment_t m)
{
    int i = h->pos;

    while (i > 0) {
        if (m.key < h->array[__heap_top(i)].key){
            h->array[i] = h->array[__heap_top(i)];
            i = __heap_top(i);
        }else {
            h->array[i] = m;
            h->pos ++;
            return h->pos;
        }
    }

    h->array[0] = m;
    h->pos ++;
    return h->pos;
}

static inline heapment_t heap_aes_pop(heap_t *h)
{
    int i = 0;
    h->pos --;
    h->array[h->len] = h->array[0];
    h->array[0] = h->array[h->pos];
    h->array[h->pos] = h->array[h->len];

    while (__heap_left(i) < h->pos)
    {
        if (__heap_right(i) >= h->pos || h->array[__heap_left(i)].key < h->array[__heap_right(i)].key){
            if (h->array[__heap_left(i)].key < h->array[i].key){
                h->array[h->len] = h->array[i];
                h->array[i] = h->array[__heap_left(i)];
                h->array[__heap_left(i)] = h->array[h->len];
                i = __heap_left(i);
            }else {
                break;
            }
        }else {
            if (h->array[__heap_right(i)].key < h->array[i].key){
                h->array[h->len] = h->array[i];
                h->array[i] = h->array[__heap_right(i)];
                h->array[__heap_right(i)] = h->array[h->len];
                i = __heap_right(i);
            }else {
                break;
            }
        }
    }

    return h->array[h->pos];
}

#endif //__HEAP_H__