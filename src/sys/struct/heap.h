#ifndef __HEAP_H__
#define __HEAP_H__


#include <env/env.h>


struct heapnode {
    size_t pos;
    uint64_t key;
    void *value;
};
typedef struct heapnode* heapnode_ptr;

struct heap {
    size_t pos, len;
    heapnode_ptr array[1];
};
typedef struct heap* heap_ptr;

#define HEAP_TOP            1
#define __heap_min(h)       (h)->array[HEAP_TOP]

#define __heap_top(i)       (i>>1)
#define __heap_left(i)      (i<<1)
#define __heap_right(i)     ((i<<1)+1)


static inline heap_ptr heap_create(uint32_t size)
{
    heap_ptr heap = (heap_ptr) malloc(sizeof(struct heap) + sizeof(heapnode_ptr) * size);
    // heap 结构体中已经有了一个 heapnode，所以数组的实际长度是 1 + size，
    // 下标 0 不存储元素，从索引 1 开始存储，范围是 1 到 len，容量是 len
    heap->len = size;
    // pos 同时作为 heap 中可读元素的个数 和 当前位置索引
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

static inline size_t heap_push(heap_ptr h, heapnode_ptr node)
{
    // pos 可以等于 len，数组的实际长度是 1 + size
    if(h->pos < h->len){
        // 数组增加一个元素，索引加 1
        h->pos++;
        // 先将新节点放在最下层
        size_t i = h->pos;
        // 与父节点比较
        while (__heap_top(i) > 0 && node->key < h->array[__heap_top(i)]->key) {
            // 与父节点交换，上移
            h->array[i] = h->array[__heap_top(i)];
            h->array[i]->pos = i;
            i = __heap_top(i);
        }
        // 小于父节点时，找到自己的位置
        h->array[i] = node;
        h->array[i]->pos = i;
        return i;
    }

    return 0;
}

static inline heapnode_ptr heap_pop(heap_ptr h)
{
    heapnode_ptr tmp;
    // smallest 指向堆顶端
    uint32_t left, right, index, smallest = HEAP_TOP;

    if (h->pos < HEAP_TOP){
        return NULL;
    }

    if (h->pos == HEAP_TOP){
        h->pos = 0;
        return h->array[1];
    }

    // 把堆顶端最小的元素先放到数组的 0 下标处
    h->array[0] = h->array[HEAP_TOP];
    // 把堆底端最大元素放到堆顶端
    h->array[HEAP_TOP] = h->array[h->pos];
    // 更新索引
    h->array[HEAP_TOP]->pos = HEAP_TOP;

    do {
        // 缓存当前索引
        index = smallest;
        // 找到左右孩子索引
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        // 与左孩子进行比较，不必比较数组的最后一个元素，因为最后一个元素已经被移除
        if (left < h->pos && h->array[smallest]->key > h->array[left]->key) {
            // 与比自己小的孩子交换位置，缓存下标
            smallest = left;
        }

        // 与右孩子进行比较
        if (right < h->pos && h->array[smallest]->key > h->array[right]->key) {
            // 与最小的孩子交换位置，缓存下标
            smallest = right;
        }

        // 检测是否需要交换节点
        if (smallest != index){
            // 进行交换，节点下沉
            tmp = h->array[index];
            h->array[index] = h->array[smallest];
            // 更新索引
            h->array[index]->pos = index;
            h->array[smallest] = tmp;
            h->array[smallest]->pos = smallest;
        }

    } while (smallest != index);

    h->pos--;
    return h->array[0];
}

static inline heapnode_ptr heap_delete(heap_ptr h, heapnode_ptr node)
{
    // smallest 指向要移除节点的位置
    uint32_t left, right, index, smallest = node->pos;

    if (h->pos < HEAP_TOP){
        return NULL;
    }

    if (h->pos == HEAP_TOP){
        h->pos = 0;
        return h->array[1];
    }

    if (h->array[smallest]->value != node->value){
        __logi("heap_delete h->array[smallest]->key != node->key");
        exit(0);
    }

    // 把堆顶端最小的元素先放到数组的 0 下标处
    h->array[0] = h->array[smallest];
    // 把堆底端最大元素放到堆顶端
    h->array[smallest] = h->array[h->pos];
    // 更新索引
    h->array[smallest]->pos = smallest;

    do {
        // 缓存当前索引
        index = smallest;
        // 找到左右孩子索引
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        // 与左孩子进行比较，不必比较数组的最后一个元素，因为最后一个元素已经被移除
        if (left < h->pos && h->array[smallest]->key > h->array[left]->key) {
            // 与比自己小的孩子交换位置，缓存下标
            smallest = left;
        }

        // 与右孩子进行比较
        if (right < h->pos && h->array[smallest]->key > h->array[right]->key) {
            // 与最小的孩子交换位置，缓存下标
            smallest = right;
        }

        // 检测是否需要交换节点
        if (smallest != index){
            // 进行交换，节点下沉
            node = h->array[index];
            h->array[index] = h->array[smallest];
            // 更新索引
            h->array[index]->pos = index;
            h->array[smallest] = node;
            h->array[smallest]->pos = smallest;
        }

    } while (smallest != index);

    h->pos--;
    return h->array[0];
}



#endif //__HEAP_H__