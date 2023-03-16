#ifndef __HEAP_H__
#define __HEAP_H__


#include <env/env.h>


struct heapnode {
    uint64_t key;
    void *value;
};

struct heap {
    uint32_t pos, len;
    struct heapnode array[1];
};
typedef struct heap* heap_ptr;

#define HEAP_TOP            1
#define __heap_min(h)       (h)->array[HEAP_TOP]

#define __heap_top(i)       (i>>1)
#define __heap_left(i)      (i<<1)
#define __heap_right(i)     ((i<<1)+1)


static inline heap_ptr heap_create(uint32_t size)
{
    heap_ptr heap = (heap_ptr) malloc(sizeof(struct heap) + sizeof(struct heapnode) * size);
    heap->len = size;
    heap->pos = 0;
    return heap;
}

static inline void heap_destroy(heap_ptr *pptr)
{
    if (pptr && *pptr) {
        heap_ptr heap = *pptr;
        *pptr = NULL;
        free(heap);
    }
}

//ascending/descending 升序/降序 最小堆/最大堆
static inline int32_t heap_push(heap_ptr h, struct heapnode node)
{
    if(h->pos < h->len){
        h->pos++;
        int32_t i = h->pos;
        while (__heap_top(i) && h->array[__heap_top(i)].key > node.key) {
            h->array[i] = h->array[__heap_top(i)];
            i = __heap_top(i);
        }
        h->array[i] = node;
        return h->pos;
    }

    return 0;
}

static inline struct heapnode heap_pop(heap_ptr h)
{
    if (h->pos < HEAP_TOP){
        return (struct heapnode){0};
    }

    if (h->pos == HEAP_TOP){
        h->pos = 0;
        return h->array[1];
    }

    struct heapnode tmp;
    h->array[0] = h->array[HEAP_TOP];
    h->array[HEAP_TOP] = h->array[h->pos];
    uint32_t left, right, index, smallest = HEAP_TOP;

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

        //TODO 是否需要判断 smallest != index 再交换
        tmp = h->array[index];
        h->array[index] = h->array[smallest];
        h->array[smallest] = tmp;

    } while (smallest != index);

    h->pos--;
    return h->array[0];
}

static inline struct heapnode heap_delete(heap_ptr h, uint64_t key)
{
    if (h->pos < HEAP_TOP){
        return (struct heapnode){0};
    }
        
    struct heapnode tmp = {0};
    h->array[0] = tmp;
    uint32_t left, right, index, smallest = HEAP_TOP;

    do {
        index = smallest;
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        if (left < h->pos && key > h->array[left].key) {
            smallest = left;
        }

        if (right < h->pos && key > h->array[right].key) {
            smallest = right;
        }

        if (smallest == index){
            h->array[0] = h->array[index];
            h->array[index] = h->array[h->pos];
        }

    } while (smallest != index);

    if (h->array[0].value == tmp.value){
        // exit(0);
        return tmp;
    }

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

    } while (smallest != index);
    // __logi("heap_delete key %llu", h->array[0].key);
    h->pos--;
    return h->array[0];
}



#endif //__HEAP_H__