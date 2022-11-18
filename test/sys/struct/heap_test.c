#include "heap.h"
#include <stdio.h>

void heap_test()
{
    heapment_t hm;
    uint64_t array[] = {99, 50, 22, 97, 56, 44, 7, 10, 3, 21, 1, 0, 33};
    heap_t *h = heap_build(256);
    for (int i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        hm.key = array[i];
        heap_aes_push(h, hm);
    }

    fprintf(stdout, "len ==== %d %lu\n", sizeof(array) / sizeof(uint64_t), h->pos);

    for (int i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        hm = heap_aes_pop(h);
        fprintf(stdout, "key = %lu\n", hm.key);
    }

    heap_destroy(&h);
}