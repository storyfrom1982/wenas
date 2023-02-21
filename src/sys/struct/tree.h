#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>

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

void tree_inseart(__tree root, linekey_ptr key, __ptr mapping)
{
    uint64_t i;
    __tree parent, tree = root;
    __tree_node *node, *prev = __tree2node(tree);
    
    __tree2node(tree)->route ++;
    char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
    {
        parent = tree;
        i = (*p) >> 4;
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
        i = *p & 0x0F;
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
    }
    
    node->mapping = mapping;
    __tree2node(parent)->leaves ++;
}

__ptr tree_find(__tree root, linekey_ptr key)
{
    char *p = &key->byte[1], *end = p + key->byte[0];
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
        return __tree2node(root)->mapping;
    }

    return NULL;
}


void tree_delete(__tree root, linekey_ptr key, void(*free_ptr)(__ptr))
{
    __tree parent, tree = root;
    char *p = &key->byte[1], *end = p + key->byte[0];

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

    __tree2node(root)->route--;

    if (free_ptr){
        free_ptr(__tree2node(tree)->mapping);
    }
    __tree2node(tree)->mapping = NULL;
    __tree2node(__tree2node(tree)->parent)->leaves--;

    p--;
    end = &key->byte[0];

    while (p != end)
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
    }
}


void tree_clear(__tree root, void(*free_ptr)(__ptr))
{
    uint64_t i = 0;
    __tree tree = root, temp;

    while (tree && __tree2node(root)->branch > 0)
    {
        while (i < TREE_DIMENSION)
        {
            if (tree[i] == NULL){
                ++i;
                continue;
            }
            
            tree = (__tree)tree[i];
            i = 0;

            if (__tree2node(tree)->branch == 0){
                break;
            }
        }

        i = __tree2node(tree)->index + 1;
        temp = tree;
        tree = __tree2node(tree)->parent;
        if (__tree2node(temp)->branch == 0){
            __tree2node(tree)->branch--;
            if (__tree2node(temp)->mapping){
                __tree2node(root)->route--;
                if (free_ptr){
                    free_ptr(__tree2node(temp)->mapping);
                }
            }
            free(temp);
            tree[i-1] = NULL;
        }
    }
}


__ptr tree_min(__tree root)
{
    if (__tree2node(root)->route > 0){

        __tree tree = root;

        int i = 0;
        while (1)
        {
            while (tree[i] == NULL){
                i++;
            }

            tree = (__tree)tree[i];

            if (__tree2node(tree)->mapping != NULL){
                return __tree2node(tree)->mapping;
            }
            
            i = 0;
        }
    }

    return NULL;
}


__ptr tree_max(__tree root)
{
    if (__tree2node(root)->route > 0){

        __tree tree = root;

        int i = TREE_DIMENSION -1;
        while (1)
        {
            while (tree[i] == NULL){
                i--;
            }

            tree = (__tree)tree[i];

            if (__tree2node(tree)->route == 1 && __tree2node(tree)->mapping != NULL){
                return __tree2node(tree)->mapping;
            }
            
            i = TREE_DIMENSION -1;
        }
    }

    return NULL;
}


__node_list* tree_sort_up(__tree root, linekey_ptr key, uint64_t count)
{
    int first = 0, i = 0;
    __tree parent, tree = root;
    __node_list head = {0}, *next = &head;
    
    if (key != NULL)
    {
        char *p = &key->byte[1], *end = p + key->byte[0];
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


__node_list* tree_sort_down(__tree root, linekey_ptr key, uint64_t count)
{
    int i = TREE_DIMENSION -1;
    __tree parent, tree = root;
    __node_list head = {0}, *next = &head;
    
    if (key != NULL)
    {
        char *p = &key->byte[1], *end = p + key->byte[0];
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

            if (__tree2node(tree)->route == 1 && __tree2node(tree)->mapping != NULL){
                // 必须先加入叶子结点。
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

        i = __tree2node(tree)->index - 1;
        tree = __tree2node(tree)->parent;

        if (tree){
            // 已经加入了所有子结点，再加入父节点。
            if (__tree2node(tree)->mapping != NULL){
                next->next = (__node_list*)malloc(sizeof(__node_list));
                next = next->next;
                next->node = __tree2node(tree);
                next->next = NULL;
                count --;
            }
        }
    }

    return head.next;
}


#endif