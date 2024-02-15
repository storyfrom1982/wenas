#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include "xmalloc.h"

#define NODE_DIMENSION          4
#define TREE_DIMENSION          16
#define TREE_MAX_INDEX          (TREE_DIMENSION - 1)
#define TREE_NODE_DIMENSION     (TREE_DIMENSION + NODE_DIMENSION)

#define __tree2node(node)        ((xnode_ptr)&(node)[TREE_DIMENSION])
#define __node2tree(tree)        ((xtree)(((char*)tree) - (TREE_DIMENSION * sizeof(void**))))


typedef struct xnode {
    void* mapping;
    void **parent;
    uint64_t route;
    uint16_t branch;
    uint16_t leaves;
    uint8_t index, x, y, z;
}*xnode_ptr;

typedef void** xtree;


typedef struct xnode_list {
    xnode_ptr node;
    struct xnode_list *next;
}*xnode_list_ptr;


static inline xtree xtree_create()
{
    return (xtree)calloc(TREE_NODE_DIMENSION, sizeof(void*));
}


static inline void xtree_free(xtree *pptr)
{
    if (pptr && *pptr){
        xtree node = *pptr;
        *pptr = (xtree)NULL;
        free(node);
    }
}


static inline void xtree_save(xtree root, void *key, uint8_t keylen, void *mapping)
{
    uint8_t i;
    xtree parent, tree = root;
    xnode_ptr node;
    
    __tree2node(tree)->route ++;
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;

    // __xlogd("---------------------------------- mapping = %d index = %u\n", *(int*)mapping, i);

    while (len < keylen)
    {
        parent = tree;
        i = ((*p) >> 4);
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(void*));
            __tree2node(tree)->branch ++;
        }
        tree = (xtree)tree[i];
        node = __tree2node(tree);
        node->route ++;
        node->index = i;
        node->parent = parent;
        // __xlogd("mapping = %d index = %u\n", *(int*)mapping, i);

        parent = tree;
        i = (*p & 0x0F);
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(void*));
            __tree2node(tree)->branch ++;
        }
        tree = (xtree)tree[i];
        node = __tree2node(tree);
        node->route ++;
        node->index = i;
        node->parent = parent;
        // __xlogd("mapping = %d index = %u\n", *(int*)mapping, i);

        p++;
        len++;
    }
    
    node->mapping = mapping;
    __tree2node(parent)->leaves ++;
    // return &node->mapping;
}


static inline void* xtree_find(xtree root, void *key, uint8_t keylen)
{
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;
    while (len < keylen)
    {
        if (root != NULL){
            root = (xtree)root[((*p) >> 4)];
        }else {
            break;
        }
        
        if (root != NULL){
            root = (xtree)root[((*p) & 0x0F)];
        }else {
            break;
        }
    
        p++;
        len++;
    }

    if (root != NULL){
        return __tree2node(root)->mapping;
    }

    return NULL;
}


static inline void* xtree_take(xtree root, void *key, uint8_t keylen)
{
    void* mapping = NULL;
    xtree parent, tree = root;
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;

    // 不能直接删除路径，要先找到节点，才能执行删除操作
    while (len < keylen)
    {
        if (tree != NULL){
            tree = (xtree)tree[((*p) >> 4)];
        }else {
            break;
        }
        if (tree != NULL){
            tree = (xtree)tree[((*p) & 0x0F)];
        }else {
            break;
        }

        p++;
        len++;
    }

    if (tree == NULL){
        return mapping;
    }

    // 总数减一
    __tree2node(root)->route--;

    mapping = __tree2node(tree)->mapping;
    __tree2node(tree)->mapping = NULL;
    // 父节点的叶子数减一
    __tree2node(__tree2node(tree)->parent)->leaves--;

    p--;

    // 删除路径
    while (len > 0)
    {
        parent = __tree2node(tree)->parent;
        if (tree != NULL){
            if ((--(__tree2node(tree)->route)) == 0){
                // 这个分支只有这一个属于删除节点的路径，所以节点被删除后，这个分支也要同时删除
                free(tree);
                parent[((*p) & 0x0F)] = NULL;
                // 更新分支数
                __tree2node(parent)->branch --;
            }
            tree = parent;
        }
        
        parent = __tree2node(tree)->parent;
        if (tree != NULL){
            if ((--(__tree2node(tree)->route)) == 0){
                free(tree);
                parent[((*p) >> 4)] = NULL;
                __tree2node(parent)->branch --;
            }
            tree = parent;
        }

        p--;
        len--;
    }

    return mapping;
}


static inline void xtree_clear(xtree root, void(*free_mapping)(void*))
{
    uint8_t i = 0;
    xtree tree = root, temp;

    while (tree && __tree2node(root)->branch > 0)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                ++i;
                continue;
            }
            
            //找到一个分支结点
            tree = (xtree)tree[i];
            //判断是否为叶子结点
            if (__tree2node(tree)->branch == 0){
                //是叶子结点就跳出循环
                break;
            }
            //不是叶子结点就开始遍历这个分支
            i = 0;
        }

        //保存下一个兄弟结点的索引
        i = __tree2node(tree)->index + 1;
        temp = tree;
        //指向上级父亲结点
        tree = __tree2node(tree)->parent;
        //判断是否为叶子结点
        if (__tree2node(temp)->branch == 0){
            //移除叶子结点
            __tree2node(tree)->branch--;
            if (__tree2node(temp)->mapping){
                __tree2node(root)->route--;
                if (free_mapping){
                    free_mapping(__tree2node(temp)->mapping);
                }
            }
            // __logi("xtree_clear %u ", i-1);
            free(temp);
            tree[i-1] = NULL;
        }
        //已经回到了上级父结点
        //开始遍历下一个子结点
    }
}


static inline void* tree_min(xtree root)
{
    if (__tree2node(root)->route > 0){

        xtree tree = root;
        // 从主干最左边的分支开始找，找到第一个有映射值的节点
        uint8_t i = 0;

        while (tree)
        {
            // 下标不会大于 TREE_DIMENSION，因为 root 的 route 大于 0，所以 tree 必然有一个分支
            while (tree[i] == NULL){
                // 跳过空节点
                i++;
            }

            tree = (xtree)tree[i];

            if (tree && __tree2node(tree)->mapping != NULL){
                // 最左边第一个有值的节点
                return __tree2node(tree)->mapping;
            }

            i = 0;
        }
    }

    return NULL;
}


static inline void* tree_max(xtree root)
{
    if (__tree2node(root)->route > 0){

        xtree tree = root;
        // 从最右边开始找，找到第一个叶子节点
        uint8_t i = TREE_DIMENSION - 1;

        while (tree)
        {
            while (tree[i] == NULL){
                // 跳过空节点
                i--;
            }

            tree = (xtree)tree[i];

            if (tree && __tree2node(tree)->branch == 0){
                // 找到叶子节点
                return __tree2node(tree)->mapping;
            }
            // 指向当前枝干最右边的节点
            i = TREE_DIMENSION - 1;
        }
    }

    return NULL;
}


static inline xnode_list_ptr tree_rise(xtree root, void *key, uint8_t keylen, uint64_t count)
{
    uint8_t first = 0, i = 0;
    xtree parent, tree = root;
    struct xnode_list head = {0}, *next = &head;
    
    if (key != NULL)
    {
        uint8_t len = 0;
        uint8_t *p = (uint8_t *)key;
        while (len < keylen)
        {
            parent = tree;
            if (tree != NULL){
                i = ((*p) >> 4);
                tree = (xtree)tree[i];
            }else {
                break;
            }
            
            parent = tree;
            if (tree != NULL){
                i = ((*p) & 0x0F);
                tree = (xtree)tree[i];
            }else {
                break;
            }

            p++;
            len++;
        }

        if (tree == NULL){
            return NULL;
        }

        // 将当前节点加入排序队列
        next->next = (xnode_list_ptr)malloc(sizeof(struct xnode_list));
        next = next->next;
        next->node = __tree2node(tree);
        next->next = NULL;
        count --;        

        // 因为所有子节点都大于父节点，所以从当前节点的第一个子节点开始遍历。
        i = 0;
    }

    while (tree && count)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                if (i == TREE_MAX_INDEX){
                    // 已经遍历了所有子节点
                    break;
                }
                ++i;
                continue;
            }
            
            tree = (xtree)tree[i];
            i = 0;

            // 因为是从左向右，从小到大遍历一棵树，所以，只要节点有映射，就可以加入排序队列
            if (__tree2node(tree)->mapping != NULL){
                next->next = (xnode_list_ptr)malloc(sizeof(struct xnode_list));
                next = next->next;
                next->node = __tree2node(tree);
                next->next = NULL;
                count --;
                // 除了自己，没有节点从这里经过，所以这个节点已经是叶子节点
                if (__tree2node(tree)->route == 1){
                    // 叶子节点没有分支，所有无需遍历
                    break;
                }
            }
        }

        // 下标指向伯节点
        i = __tree2node(tree)->index + 1;
        // 指针指向父节点
        tree = __tree2node(tree)->parent;

        // 循环检查，确保找到一个不越界的，可用的伯节点
        while (tree && i > TREE_MAX_INDEX){
            // 当前节点已经是最小的子节点，所以指针要上移，伯节点，伯叔节点开始遍历
            // 循环检查，确保找到一个不越界的，可用的伯节点
            i = __tree2node(tree)->index + 1;
            tree = __tree2node(tree)->parent;
        }        
    }

    return head.next;
}


static inline xnode_list_ptr tree_drop(xtree root, void *key, uint8_t keylen, uint64_t count)
{
    uint8_t i = TREE_DIMENSION -1;
    xtree parent, tree = root;
    struct xnode_list head = {0}, *next = &head;
    
    if (key != NULL)
    {
        uint8_t len = 0;
        uint8_t *p = (uint8_t *)key;

        // 找到开始的节点
        while (len < keylen)
        {
            parent = tree;
            if (tree != NULL){
                i = ((*p) >> 4);
                tree = (xtree)tree[i];
            }else {
                break;
            }

            parent = tree;
            if (tree != NULL){
                i = ((*p) & 0x0F);
                tree = (xtree)tree[i];
            }else {
                break;
            }

            p++;
            len++;
        }

        if (tree == NULL){
            return NULL;
        }

        // 将当前节点加入排序列表
        next->next = (xnode_list_ptr)malloc(sizeof(struct xnode_list));
        next = next->next;
        next->node = __tree2node(tree);
        next->next = NULL;
        count--;

        // 因为所有子节点都大于父节点，所以从弟节点开始遍历
        // 下标移动到弟节点
        i = __tree2node(tree)->index -1;
        // 指针指向父节点
        tree = __tree2node(tree)->parent;

        // 循环检查，确保找到一个不越界的，可用的叔节点
        while (tree && i > TREE_MAX_INDEX){
            // 当前节点已经是最小的子节点，所以指针要上移，叔节点，从叔节点开始遍历
            i = __tree2node(tree)->index -1;
            tree = __tree2node(tree)->parent;
        }
    }

    while (tree && count)
    {
        while (i >= 0)
        {
            // 检查当前节点是否为空节点
            if (tree[i] == NULL){
                if (i == 0){
                    // 已经遍历了所有子节点
                    break;
                }
                --i;
                // 跳过空节点
                continue;
            }

            // 移动到非空子节点
            tree = (xtree)tree[i];
            // 从最右边开始遍历当前节点的子节点
            i = TREE_DIMENSION -1;

            // 子节点的分支数为 0
            // 叶子节点的映射一定不为空
            if (__tree2node(tree)->branch == 0/* && __tree2node(tree)->mapping != NULL*/){
                // 在一个分支上找到了叶子结点
                // 这里只加入叶子节点，不处理分支节点
                // 因为是从右向左遍历，所以最先找到的第一个叶子节点，一定是最大的那个叶子节点
                next->next = (xnode_list_ptr)malloc(sizeof(struct xnode_list));
                next = next->next;
                next->node = __tree2node(tree);
                next->next = NULL;
                count--;
                break;
            }
        }

        // 下标指向弟节点
        i = __tree2node(tree)->index - 1;
        // 指针指向父节点
        tree = __tree2node(tree)->parent;

        if (tree){
            // 标记一个分支已经被遍历过了
            __tree2node(tree)->x++;
            if (__tree2node(tree)->x == __tree2node(tree)->branch){
                // 当前节点的所有子节点都已经被遍历
                // 可以把当前节点加入排序列表中了
                // 将标记清零
                __tree2node(tree)->x = 0;
                if (__tree2node(tree)->mapping != NULL){
                    next->next = (xnode_list_ptr)malloc(sizeof(struct xnode_list));
                    next = next->next;
                    next->node = __tree2node(tree);
                    next->next = NULL;
                    count --;
                }

            }
        }

        // 循环检查，确保找到一个不越界的，可用的叔节点
        while (tree && i > TREE_MAX_INDEX){
            // 当前节点已经是最小的子节点，所以指针要上移，叔节点，从叔节点开始遍历
            // 循环检查，确保找到一个不越界的，可用的叔节点
            i = __tree2node(tree)->index -1;
            tree = __tree2node(tree)->parent;
        }
    }

    return head.next;
}

// 无法排序int类型数字 受字节序，大小端的影响
// [0x7B724B80] [731.174]   59 tree.h                [D] ---------------------------------- mapping = 1 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 1 index = 1
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 1 index = 0
// [0x7B724B80] [731.174]   59 tree.h                [D] ---------------------------------- mapping = 32768 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 32768 index = 8
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 32768 index = 0
// [0x7B724B80] [731.174]   59 tree.h                [D] ---------------------------------- mapping = 65536 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 65536 index = 1
// [0x7B724B80] [731.174]   74 tree.h                [D] mapping = 65536 index = 0
// [0x7B724B80] [731.174]   87 tree.h                [D] mapping = 65536 index = 0


#endif
