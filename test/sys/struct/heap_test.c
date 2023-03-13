#include "sys/struct/heap.h"


void heap_test()
{
    heapment_t hm;
    uint64_t array[] = {
        2,  3,  4,  5,  22, 11, 6, 6,  6, 6,  6, 4, 4,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        2,  3,  4,  5,  22, 11, 6, 6,  6, 6,  6, 4, 4,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33,        
        99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33
        };
    heap_t *h = heap_create(256);
    for (int32_t i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        hm.key = array[i];
        heap_push(h, hm);
    }

    __logd("len ==== %d %lu\n", sizeof(array) / sizeof(uint64_t), h->pos);

    for (int32_t i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        hm = heap_pop(h);
        __logd("heap.pos %u count %d key = %lu\n", h->pos, i, hm.key);
    }

    heap_destroy(&h);
}