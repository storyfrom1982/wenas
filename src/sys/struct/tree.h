#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <ex/ex.h>

#define NODE_DIMENSION          4
#define TREE_DIMENSION          16
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

        if (tree){
            // 如果索引 key 存在，需要跳过索引节点。
            i = 0;
            parent = tree;
        }

        // 因为所有子节点都大于父节点，所以从当前节点的第一个子节点开始遍历。
        tree = parent;

        while (tree[i] == NULL)
        {
            i++;
            continue;
        }
    }

    while (tree && count)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                ++i;
                continue;
            }
            
            tree = (__tree)tree[i];
            i = 0;

            if (__tree2node(tree)->mapping != NULL){
                next->next = (__node_list*)malloc(sizeof(__node_list));
                next = next->next;
                next->node = __tree2node(tree);
                next->next = NULL;
                count --;
                if (__tree2node(tree)->route == 1){
                    break;
                }
            }
        }

        i = __tree2node(tree)->index + 1;
        tree = __tree2node(tree)->parent;
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

        tree = parent;
        // 所有子节点都大于父节点，所以从比当前节点小的兄弟节点开始遍历
        if (i == 0){
            // 当前节点已经是兄弟节点当中最小的节点，所以要向上移动到叔叔节点
            i = TREE_DIMENSION -1;
            tree = __tree2node(tree)->parent;
        }else {
            // 移动到小于当前节点的兄弟节点
            i--;
        }

        while (tree[i] == NULL)
        {
            if (i == 0){
                i = __tree2node(tree)->index - 1;
                tree = __tree2node(tree)->parent;
            }else {
                i--;
            }
            continue;
        }
    }

    while (tree && count)
    {
        while (i >= 0)
        {
            if (tree[i] == NULL){
                if (i == 0){
                    // __ex_logd("find all >>>>>---------------------->>>\n");
                    // 已经遍历了所有子节点
                    break;
                }
                --i;
                // __ex_logd("find next >>>>>---------------------->>>\n");
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
                __ex_logd("leaf mapping ----------- %s\n", (char*)next->node->mapping);
                break;
            }
        }

        // 将下标指向比当前节点小的兄弟节点
        i = __tree2node(tree)->index - 1;
        // 移动回当前节点的父节点，继续向下遍历
        tree = __tree2node(tree)->parent;

        if (tree){
            // 标记一个分支已经被遍历过了
            __tree2node(tree)->x++;
            if (__tree2node(tree)->x == __tree2node(tree)->branch){
                // 当前节点的所有子节点都已经被遍历
                // 多以可以把当前节点加入列表中了
                __ex_logd("branch++ ------------x = %u branch = %u\n", __tree2node(tree)->x, __tree2node(tree)->branch);
                // 将标记清零
                __tree2node(tree)->x = 0;
                if (__tree2node(tree)->mapping != NULL){
                    next->next = (__node_list*)malloc(sizeof(__node_list));
                    next = next->next;
                    next->node = __tree2node(tree);
                    next->next = NULL;
                    count --;
                    __ex_logd("branch mapping ------------ %s\n", (char*)next->node->mapping);
                }                
            }else {
                __ex_logd("branch++ ------------x = %u branch = %u\n", __tree2node(tree)->x, __tree2node(tree)->branch);
            }
            
            // @note 之前在这里直接当前节点加入列表是有问题的
            // 因为当时并不能确定当前的节点的子节点都已经被遍历了
            // 父节点没有被重复加入，是因为所有叶子节点的父节点都没有映射值
        }
    }

    return head.next;
}


#endif
