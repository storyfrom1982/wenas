#include "sys/struct/xheap.h"


void heap_test()
{
    xheapnode_ptr node;
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
    xheap_ptr h = xheap_create(256);
    for (int32_t i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        node = malloc(sizeof(struct xheapnode));
        node->key = array[i];
        xheap_push(h, node);
    }

    xheap_remove(h, node);

    __xlogd("len ==== %d %lu\n", sizeof(array) / sizeof(uint64_t), h->pos);

    for (int32_t i = 0; i < sizeof(array) / sizeof(uint64_t); ++i){
        node = xheap_pop(h);
        if (node){
            __xlogd("heap.pos %u count %d key = %lu\n", h->pos, i, node->key);
            free(node);   
        }
    }

    xheap_free(&h);
}

int main(int argc, char *argv[])
{
    heap_test();
    return 0;
}