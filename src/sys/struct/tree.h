#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>

#define NODE_DIMENSION          8
#define TREE_DIMENSION          16
#define TREE_NODE_DIMENSION     (TREE_DIMENSION + NODE_DIMENSION)

#define __treenode(node)        ((__tree_node*)&(node)[TREE_DIMENSION])

typedef struct __tree_node {
    __ptr val;
    __ptr *parent;
    struct __tree_node *prev;
    struct __tree_node *next;
    struct __tree_node *order;
    uint64_t index, count;
}__tree_node;

typedef __ptr* __tree;


__tree tree_create()
{
    return (__tree)calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
}

void tree_destroy(__tree *pp_tree)
{
    if (pp_tree && *pp_tree){
        __tree tree = *pp_tree;
        *pp_tree = (__tree)NULL;
        free(tree);
    }
}

void tree_inseart(__tree tree, linekey_t *key, __ptr val)
{
    uint64_t i;
    __tree parent;
    __tree_node *node, *prev = __treenode(tree);
    
    __treenode(tree)->count ++;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
    {
        parent = tree;
        i = (*p) >> 4;
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
        }
        tree = (__tree)tree[i];
        node = __treenode(tree);
        node->count ++;
        node->index = i;
        node->parent = parent;

        parent = tree;
        i = *p & 0x0F;
        if (tree[i] == NULL){
            tree[i] = calloc(TREE_NODE_DIMENSION, sizeof(__ptr));
        }
        tree = (__tree)tree[i];
        node = __treenode(tree);
        node->count ++;
        node->index = i;
        node->parent = parent;

        p++;
    }

    node->val = val;
    
    node->next = prev->next;
    if (node->next){
        node->next->prev = node;
    }
    prev->next = node;
    node->prev = prev;
}

__ptr tree_find(__tree tree, linekey_t *key)
{
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

    if (tree != NULL){
        return __treenode(tree)->val;
    }

    return NULL;
}


void tree_delete(__tree tree, linekey_t *key)
{
    __tree parent, node = tree;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
    {
        if (node != NULL){
            node = (__tree)node[((*p) >> 4)];
        }else {
            break;
        }
        if (node != NULL){
            node = (__tree)node[((*p) & 0x0F)];
        }else {
            break;
        }

        p++;
    }

    if (node == NULL){
        return;
    }

    __treenode(tree)->count--;
    __treenode(node)->prev->next = __treenode(node)->next;
    if (__treenode(node)->next){
        __treenode(node)->next->prev = __treenode(node)->prev;
    }

    p--;
    end = &key->byte[0];
    while (p != end)
    {
        parent = __treenode(node)->parent;
        if (node != NULL){
            if ((--(__treenode(node)->count)) == 0){
                free(node);
                parent[((*p) & 0x0F)] = NULL;
            }
            node = parent;
        }
        
        parent = __treenode(node)->parent;
        if (node != NULL){
            if ((--(__treenode(node)->count)) == 0){
                free(node);
                parent[((*p) >> 4)] = NULL;
            }
            node = parent;
        }

        p--;
    }
}


__ptr tree_min(__tree tree)
{
    if (__treenode(tree)->count > 0){

        __tree node = tree;

        int i = 0;
        while (1)
        {
            while (node[i] == NULL){
                i++;
            }

            node = (__tree)node[i];

            if (__treenode(node)->val != NULL){
                return __treenode(node)->val;
            }
            
            i = 0;
        }
    }

    return NULL;
}


__ptr tree_max(__tree tree)
{
    if (__treenode(tree)->count > 0){

        __tree node = tree;

        int i = TREE_DIMENSION -1;
        while (1)
        {
            while (node[i] == NULL){
                i--;
            }

            node = (__tree)node[i];

            if (__treenode(node)->count == 1 && __treenode(node)->val != NULL){
                return __treenode(node)->val;
            }
            
            i = TREE_DIMENSION -1;
        }
    }

    return NULL;
}

__tree_node* tree_sort_up(__tree tree, linekey_t *key, uint64_t count)
{
    int i = 0;
    __tree node = tree;
    __tree_node head = {0}, *next = &head;
    
    if (key != NULL)
    {
        count ++;
        unsigned char *p = &key->byte[1], *end = p + key->byte[0];
        while (p != end)
        {
            if (node != NULL){
                i = ((*p) >> 4);
                node = (__tree)node[i];
            }else {
                break;
            }
            
            if (node != NULL){
                i = ((*p) & 0x0F);
                node = (__tree)node[i];
            }else {
                break;
            }

            p++;
        }

        node = __treenode(node)->parent;
    }

    while (node && count)
    {
        while (i < TREE_DIMENSION)
        {
            if (node[i] == NULL){
                ++i;
                continue;
            }
            
            node = (__tree)node[i];
            i = 0;

            if (__treenode(node)->val != NULL){
                next->order = __treenode(node);
                next = next->order;
                count --;
                if (__treenode(node)->count == 1){
                    break;
                }
            }
        }

        i = __treenode(node)->index + 1;
        node = __treenode(node)->parent;
    }

    if (key != NULL && head.order != NULL){
        return head.order->order;
    }

    return head.order;
}


__tree_node* tree_sort_down(__tree tree, linekey_t *key, uint64_t count)
{
    int i = TREE_DIMENSION -1;
    __tree node = tree;
    __tree_node head = {0}, *next = &head;
    
    if (key != NULL)
    {
        count ++;
        unsigned char *p = &key->byte[1], *end = p + key->byte[0];
        while (p != end)
        {
            if (node != NULL){
                i = ((*p) >> 4);
                node = (__tree)node[i];
            }else {
                break;
            }
            
            if (node != NULL){
                i = ((*p) & 0x0F);
                node = (__tree)node[i];
            }else {
                break;
            }

            p++;
        }

        node = __treenode(node)->parent;
    }

    while (node && count)
    {
        while (i >= 0)
        {
            if (node[i] == NULL){
                --i;
                continue;
            }
            
            node = (__tree)node[i];
            i = TREE_DIMENSION -1;

            if (__treenode(node)->val != NULL){
                next->order = __treenode(node);
                next = next->order;
                count --;
                if (__treenode(node)->count == 1){
                    break;
                }
            }
        }

        i = __treenode(node)->index - 1;
        node = __treenode(node)->parent;
    }

    if (key != NULL && head.order != NULL){
        return head.order->order;
    }

    return head.order;
}


#endif