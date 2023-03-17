#ifndef __HEAP_H__
#define __HEAP_H__


#include <env/env.h>


struct heapnode {
    uint64_t key;
    void *value;
};

struct heap {
    size_t pos, len;
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

static inline size_t heap_push(heap_ptr h, struct heapnode node)
{
    // pos 可以等于 len，数组的实际长度是 1 + size
    if(h->pos < h->len){
        // 数组增加一个元素，索引加 1
        h->pos++;
        // 先将新节点放在最下层
        size_t i = h->pos;
        // 与父节点比较
        while (__heap_top(i) > 0 && node.key < h->array[__heap_top(i)].key) {
            // 与父节点交换，上移
            h->array[i] = h->array[__heap_top(i)];
            i = __heap_top(i);
        }
        // 小于父节点时，找到自己的位置
        h->array[i] = node;
        return i;
    }

    return 0;
}

static inline struct heapnode heap_pop(heap_ptr h)
{
    struct heapnode tmp;
    // smallest 指向堆顶端
    uint32_t left, right, index, smallest = HEAP_TOP;

    if (h->pos < HEAP_TOP){
        return (struct heapnode){0};
    }

    if (h->pos == HEAP_TOP){
        h->pos = 0;
        return h->array[1];
    }

    // 把堆顶端最小的元素先放到数组的 0 下标处
    h->array[0] = h->array[HEAP_TOP];
    // 把堆底端最大元素放到堆顶端
    h->array[HEAP_TOP] = h->array[h->pos];

    do {
        // 缓存当前索引
        index = smallest;
        // 找到左右孩子索引
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        // 与左孩子进行比较，不必比较数组的最后一个元素，因为最后一个元素已经被移除
        if (left < h->pos && h->array[smallest].key > h->array[left].key) {
            // 与比自己小的孩子交换位置，缓存下标
            smallest = left;
        }

        // 与右孩子进行比较
        if (right < h->pos && h->array[smallest].key > h->array[right].key) {
            // 与最小的孩子交换位置，缓存下标
            smallest = right;
        }

        // 检测是否需要交换节点
        if (smallest != index){
            // 进行交换，节点下沉
            tmp = h->array[index];
            h->array[index] = h->array[smallest];
            h->array[smallest] = tmp;
        }

    } while (smallest != index);

    h->pos--;
    return h->array[0];
}

static inline struct heapnode heap_delete(heap_ptr h, uint64_t key)
{
    struct heapnode tmp;
    size_t left, right, index, smallest = HEAP_TOP;

    if (h->pos < HEAP_TOP){
        return (struct heapnode){0};
    }

    // 从最后面的元素开始查找
    if (key == h->array[h->pos].key){
        tmp = h->array[h->pos];
        h->pos --;
        return tmp;
    }

    index = h->pos;
    // 检测父节点索引是否越界
    while (__heap_top(index) > 0) {
        if (key == h->array[__heap_top(index)].key){
            smallest = __heap_top(index);
            break;
        }else if (key < h->array[__heap_top(index)].key){
            // 小于父节点则上移
            index = __heap_top(index);
        }else {
            // key 不等于当前节点，又大于父节点，所以等于兄弟节点
            smallest = __heap_left(__heap_top(index));
            if (smallest == index){
                // 如果当前是左节点，则兄弟节点就是右节点
                smallest = __heap_right(__heap_top(index));
            }
            break;
        }
    }

    if (key != h->array[smallest].key){
        __loge("heap_delete cannot find key %llu", key);
        return (struct heapnode){0};
    }

    // 缓存要移除的结点
    h->array[0] = h->array[smallest];
    // 与最底端的结点交换位置
    h->array[smallest] = h->array[h->pos];

    do {
        // 移除节点
        index = smallest;
        left = __heap_left(smallest);
        right = __heap_right(smallest);

        if (left < h->pos && h->array[smallest].key > h->array[left].key) {
            smallest = left;
        }

        if (right < h->pos && h->array[smallest].key > h->array[right].key) {
            smallest = right;
        }

        if (index != smallest){
            tmp = h->array[index];
            h->array[index] = h->array[smallest];
            h->array[smallest] = tmp;
        }

    } while (smallest != index);
    
    h->pos--;
    return h->array[0];
}



#endif //__HEAP_H__