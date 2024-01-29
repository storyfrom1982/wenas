#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <ex/ex.h>

#define NODE_DIMENSION          4
#define TREE_DIMENSION          16
#define TREE_MAX_INDEX          (TREE_DIMENSION -1)
#define TREE_NODE_DIMENSION     (TREE_DIMENSION + NODE_DIMENSION)

#define __tree2node(node)        ((__tree_node*)&(node)[TREE_DIMENSION])
#define __node2tree(tree)        ((__tree)((char*)tree-(TREE_DIMENSION * sizeof(__ptr))))


typedef struct __tree_node {
    __ptr mapping;
    __ptr *parent;
    uint64_t route;
    uint16_t branch;
    uint16_t leaves;
    uint8_t index, x, y, z;
}__tree_node;

typedef __ptr* __tree;


typedef struct __node_list {
    __tree_node *node;
    struct __node_list *next;
}__node_list;


static inline __tree tree_create()
{
    return (__tree)calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
}


static inline void tree_release(__tree *pptr)
{
    if (pptr && *pptr){
        __tree node = *pptr;
        *pptr = (__tree)NULL;
        free(node);
    }
}

//TODO 返回 mapping 的地址，允许用户自定义操作。
//static inline __ptr* xtree_inseart(__tree root, void *key, uint8_t keylen, __ptr mapping)
static inline void tree_inseart(__tree root, void *key, uint8_t keylen, __ptr mapping)
{
    uint8_t i;
    __tree parent, tree = root;
    __tree_node *node;
    
    __tree2node(tree)->route ++;
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;

    // __ex_logd("---------------------------------- mapping = %d index = %u\n", *(int*)mapping, i);

    while (len < keylen)
    {
        parent = tree;
        i = ((*p) >> 4);
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
            __tree2node(tree)->branch ++;
        }
        tree = (__tree)tree[i];
        node = __tree2node(tree);
        node->route ++;
        node->index = i;
        node->parent = parent;
        // __ex_logd("mapping = %d index = %u\n", *(int*)mapping, i);

        parent = tree;
        i = (*p & 0x0F);
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
            __tree2node(tree)->branch ++;
        }
        tree = (__tree)tree[i];
        node = __tree2node(tree);
        node->route ++;
        node->index = i;
        node->parent = parent;
        // __ex_logd("mapping = %d index = %u\n", *(int*)mapping, i);

        p++;
        len++;
    }
    
    node->mapping = mapping;
    __tree2node(parent)->leaves ++;
    // return &node->mapping;
}

// static inline __ptr* xtree_find(__tree root, void *key, uint8_t keylen)
static inline __ptr tree_find(__tree root, void *key, uint8_t keylen)
{
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;
    while (len < keylen)
    {
        if (root != NULL){
            root = (__tree)root[((*p) >> 4)];
        }else {
            break;
        }
        
        if (root != NULL){
            root = (__tree)root[((*p) & 0x0F)];
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


static inline __ptr tree_delete(__tree root, void *key, uint8_t keylen)
{
    __ptr mapping = NULL;
    __tree parent, tree = root;
    uint8_t len = 0;
    uint8_t *p = (uint8_t *)key;

    while (len < keylen)
    {
        if (tree != NULL){
            tree = (__tree)tree[((*p) >> 4)];
        }else {
            break;
        }
        if (tree != NULL){
            tree = (__tree)tree[((*p) & 0x0F)];
        }else {
            break;
        }

        p++;
        len++;
    }

    if (tree == NULL){
        return mapping;
    }

    __tree2node(root)->route--;

    mapping = __tree2node(tree)->mapping;
    __tree2node(tree)->mapping = NULL;
    __tree2node(__tree2node(tree)->parent)->leaves--;

    p--;

    while (len > 0)
    {
        parent = __tree2node(tree)->parent;
        if (tree != NULL){
            if ((--(__tree2node(tree)->route)) == 0){
                free(tree);
                parent[((*p) & 0x0F)] = NULL;
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


static inline void tree_clear(__tree root, void(*free_ptr)(__ptr))
{
    uint8_t i = 0;
    __tree tree = root, temp;

    while (tree && __tree2node(root)->branch > 0)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                ++i;
                continue;
            }
            
            //找到一个分支结点
            tree = (__tree)tree[i];
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
                if (free_ptr){
                    free_ptr(__tree2node(temp)->mapping);
                }
            }
            // __logi("tree_clear %u ", i-1);
            free(temp);
            tree[i-1] = NULL;
        }
        //已经回到了上级父结点
        //开始遍历下一个子结点
    }
}


static inline __ptr tree_min(__tree root)
{
    if (__tree2node(root)->route > 0){

        __tree tree = root;
        uint8_t i = 0;

        while (tree)
        {
            // i 不会大于 TREE_DIMENSION，因为 root 的 route 大于 0，所以 tree 必然有一个分支
            while (tree[i] == NULL){
                i++;
            }

            tree = (__tree)tree[i];

            if (tree && __tree2node(tree)->route == 1 && __tree2node(tree)->mapping != NULL){
                return __tree2node(tree)->mapping;
            }

            i = 0;
        }
    }

    return NULL;
}


static inline __ptr tree_max(__tree root)
{
    if (__tree2node(root)->route > 0){

        __tree tree = root;
        uint8_t i = TREE_DIMENSION - 1;

        while (tree)
        {
            while (tree[i] == NULL){
                i--;
            }

            tree = (__tree)tree[i];

            if (tree && __tree2node(tree)->route == 1 && __tree2node(tree)->mapping != NULL){
                return __tree2node(tree)->mapping;
            }
            
            i = TREE_DIMENSION - 1;
        }
    }

    return NULL;
}


static inline __node_list* tree_up(__tree root, void *key, uint8_t keylen, uint64_t count)
{
    uint8_t first = 0, i = 0;
    __tree parent, tree = root;
    __node_list head = {0}, *next = &head;
    
    if (key != NULL)
    {
        uint8_t len = 0;
        uint8_t *p = (uint8_t *)key;
        while (len < keylen)
        {
            parent = tree;
            if (tree != NULL){
                i = ((*p) >> 4);
                tree = (__tree)tree[i];
            }else {
                break;
            }
            
            parent = tree;
            if (tree != NULL){
                i = ((*p) & 0x0F);
                tree = (__tree)tree[i];
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
        next->next = (__node_list*)malloc(sizeof(__node_list));
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
            
            tree = (__tree)tree[i];
            i = 0;

            // 因为是从左向右，从小到大遍历一棵树，所以，只要节点有映射，就可以加入排序队列
            if (__tree2node(tree)->mapping != NULL){
                next->next = (__node_list*)malloc(sizeof(__node_list));
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


static inline __node_list* tree_down(__tree root, void *key, uint8_t keylen, uint64_t count)
{
    uint8_t i = TREE_DIMENSION -1;
    __tree parent, tree = root;
    __node_list head = {0}, *next = &head;
    
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
                tree = (__tree)tree[i];
            }else {
                break;
            }

            parent = tree;
            if (tree != NULL){
                i = ((*p) & 0x0F);
                tree = (__tree)tree[i];
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
        next->next = (__node_list*)malloc(sizeof(__node_list));
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
            tree = (__tree)tree[i];
            // 从最右边开始遍历当前节点的子节点
            i = TREE_DIMENSION -1;

            // 子节点的分支数为 0
            // 叶子节点的映射一定不为空
            if (__tree2node(tree)->branch == 0/* && __tree2node(tree)->mapping != NULL*/){
                // 在一个分支上找到了叶子结点
                // 这里只加入叶子节点，不处理分支节点
                // 因为是从右向左遍历，所以最先找到的第一个叶子节点，一定是最大的那个叶子节点
                next->next = (__node_list*)malloc(sizeof(__node_list));
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
                    next->next = (__node_list*)malloc(sizeof(__node_list));
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
