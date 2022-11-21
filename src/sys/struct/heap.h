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


#define __heap_top(i)       (i>>1)
#define __heap_left(i)      (i<<1)
#define __heap_right(i)     ((i<<1)+1)



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
    if(h->pos < h->len){
        h->pos++;
        int i = h->pos;
        while (i > 0 && h->array[__heap_top(i)].key > m.key) {
            h->array[i] = h->array[__heap_top(i)];
            i = __heap_top(i);
        }
        h->array[i] = m;
        return h->pos;
    }

    return 0;
}

static inline heapment_t heap_aes_pop(heap_t *h)
{
    heapment_t tmp, min = h->array[1];
    h->array[1] = h->array[h->pos];
    uint32_t left, right, index, smallest = 1;

    do {
        index = smallest;
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        if (left < h->pos && h->array[smallest].key > h->array[left].key) {
            smallest = left;
        }
        if (right < h->pos && h->array[smallest].key > h->array[right].key) {
            smallest = right;
        }

        tmp = h->array[index];
        h->array[index] = h->array[smallest];
        h->array[smallest] = tmp;

    }while (smallest != index);

    h->pos--;
    return min;
}

#endif //__HEAP_H__