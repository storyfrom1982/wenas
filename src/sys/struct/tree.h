#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>

#define NODE_DIMENSION          8
#define TREE_DIMENSION          16
#define TREE_NODE_DIMENSION     (TREE_DIMENSION + NODE_DIMENSION)

#define __tree2node(node)        ((__tree_node*)&(node)[TREE_DIMENSION])
#define __node2tree(tree)        ((__tree)((char*)tree-(TREE_DIMENSION * sizeof(__ptr))))


//写入顺序遍历连表不放在node中存储
//明确定义 路由数，分支数
//子孙总数是否等于路由数
//是否可以用分支数和索引完成排序
//是否可以不维护 order 连表，而是在排序的时候动态分配
//是否可以将Value部分压缩到4个指针，将节点大小定位在160bit

typedef struct __tree_node {
    __ptr val;
    __ptr *parent;
    struct __tree_node *prev;
    struct __tree_node *next;
    struct __tree_node *order;
    struct {
        uint8_t index;
        uint8_t route;
        uint8_t leaf;
        uint8_t x;
        uint32_t y;
    };
    uint64_t count;
}__tree_node;

typedef __ptr* __tree;


__tree tree_create()
{
    return (__tree)calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
}


void tree_destroy(__tree *pp_root)
{
    if (pp_root && *pp_root){
        __tree root = *pp_root;
        *pp_root = (__tree)NULL;
        free(root);
    }
}

void tree_inseart(__tree root, linekey_t *key, __ptr val)
{
    uint64_t i;
    __tree parent, tree = root;
    __tree_node *node, *prev = __tree2node(tree);
    
    __tree2node(tree)->count ++;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
    {
        parent = tree;
        i = (*p) >> 4;
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
            __tree2node(tree)->route ++;
        }
        tree = (__tree)tree[i];
        node = __tree2node(tree);
        node->count ++;
        node->index = i;
        node->parent = parent;

        parent = tree;
        i = *p & 0x0F;
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
            __tree2node(tree)->route ++;
        }
        tree = (__tree)tree[i];
        node = __tree2node(tree);
        node->count ++;
        node->index = i;
        node->parent = parent;

        p++;
    }
    
    node->val = val;
    __tree2node(parent)->leaf ++;
    
    node->next = prev->next;
    if (node->next){
        node->next->prev = node;
    }
    prev->next = node;
    node->prev = prev;
}

__ptr tree_find(__tree root, linekey_t *key)
{
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    while (p != end)
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
    }

    if (root != NULL){
        return __tree2node(root)->val;
    }

    return NULL;
}


void tree_delete(__tree root, linekey_t *key, void(*free_ptr)(__ptr))
{
    __tree parent, tree = root;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
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
    }

    if (tree == NULL){
        return;
    }

    __tree2node(root)->count--;
    __tree2node(tree)->prev->next = __tree2node(tree)->next;
    if (__tree2node(tree)->next){
        __tree2node(tree)->next->prev = __tree2node(tree)->prev;
    }

    if (free_ptr){
        free_ptr(__tree2node(tree)->val);
    }
    __tree2node(tree)->val = NULL;
    __tree2node(__tree2node(tree)->parent)->leaf--;

    p--;
    end = &key->byte[0];

    while (p != end)
    {
        parent = __tree2node(tree)->parent;
        if (tree != NULL){
            if ((--(__tree2node(tree)->count)) == 0){
                free(tree);
                parent[((*p) & 0x0F)] = NULL;
                __tree2node(parent)->route --;
            }
            tree = parent;
        }
        
        parent = __tree2node(tree)->parent;
        if (tree != NULL){
            if ((--(__tree2node(tree)->count)) == 0){
                free(tree);
                parent[((*p) >> 4)] = NULL;
                __tree2node(parent)->route --;
            }
            tree = parent;
        }

        p--;
    }
}


void tree_clear(__tree root, void(*free_ptr)(__ptr))
{
    uint64_t i = 0;
    __tree tree = root, temp;

    while (tree && __tree2node(root)->route > 0)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                ++i;
                continue;
            }
            
            tree = (__tree)tree[i];
            i = 0;

            if (__tree2node(tree)->route == 0){
                break;
            }
        }

        i = __tree2node(tree)->index + 1;
        temp = tree;
        tree = __tree2node(tree)->parent;
        if (__tree2node(temp)->route == 0){
            __tree2node(tree)->route--;
            if (__tree2node(temp)->val){
                __tree2node(root)->count--;
                if (free_ptr){
                    free_ptr(__tree2node(temp)->val);
                }
            }
            free(temp);
            // tree[i-1] = NULL;
        }
    }
}


__ptr tree_min(__tree root)
{
    if (__tree2node(root)->count > 0){

        __tree tree = root;

        int i = 0;
        while (1)
        {
            while (tree[i] == NULL){
                i++;
            }

            tree = (__tree)tree[i];

            if (__tree2node(tree)->val != NULL){
                return __tree2node(tree)->val;
            }
            
            i = 0;
        }
    }

    return NULL;
}


__ptr tree_max(__tree root)
{
    if (__tree2node(root)->count > 0){

        __tree tree = root;

        int i = TREE_DIMENSION -1;
        while (1)
        {
            while (tree[i] == NULL){
                i--;
            }

            tree = (__tree)tree[i];

            if (__tree2node(tree)->count == 1 && __tree2node(tree)->val != NULL){
                return __tree2node(tree)->val;
            }
            
            i = TREE_DIMENSION -1;
        }
    }

    return NULL;
}


__tree_node* tree_sort_up(__tree root, linekey_t *key, uint64_t count)
{
    int first = 0, i = 0;
    __tree parent, tree = root;
    __tree_node head = {0}, *next = &head;
    
    if (key != NULL)
    {
        unsigned char *p = &key->byte[1], *end = p + key->byte[0];
        while (p != end)
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

            if (__tree2node(tree)->val != NULL){
                next->order = __tree2node(tree);
                next = next->order;
                next->order = NULL;
                count --;
                if (__tree2node(tree)->count == 1){
                    break;
                }
            }
        }

        i = __tree2node(tree)->index + 1;
        tree = __tree2node(tree)->parent;
    }

    return head.order;
}


__tree_node* tree_sort_down(__tree root, linekey_t *key, uint64_t count)
{
    int i = TREE_DIMENSION -1;
    __tree parent, tree = root;
    __tree_node head = {0}, *next = &head;
    
    if (key != NULL)
    {
        unsigned char *p = &key->byte[1], *end = p + key->byte[0];
        while (p != end)
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
        }

        tree = parent;
        i--; // 所有子节点都大于父节点，所以从比当前结点小的兄弟节点开始遍历。

        while (tree[i] == NULL)
        {
            i--;
            continue;
        }
    }

    while (tree && count)
    {
        while (i >= 0)
        {
            if (tree[i] == NULL){
                --i;
                continue;
            }
            
            tree = (__tree)tree[i];
            i = TREE_DIMENSION -1;

            if (__tree2node(tree)->count == 1 && __tree2node(tree)->val != NULL){
                // 必须先加入叶子结点。
                next->order = __tree2node(tree);
                next = next->order;
                next->order = NULL;
                count --;
                if (__tree2node(tree)->count == 1){
                    break;
                }
            }
        }

        i = __tree2node(tree)->index - 1;
        tree = __tree2node(tree)->parent;

        if (tree){
            // 已经加入了所有子结点，再加入父节点。
            if (__tree2node(tree)->val != NULL){
                next->order = __tree2node(tree);
                next = next->order;
                next->order = NULL;
                count --;
            }
        }
    }

    return head.order;
}


#endif