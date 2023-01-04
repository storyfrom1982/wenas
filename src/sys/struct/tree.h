#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>


#define TREE_WIDTH              22
#define TREE_VALUE              16

#define __tree_val(node)   ((__tree_val*)&node[TREE_VALUE])

typedef struct __tree_val {
    __ptr val;
    __uint64 count;
    __ptr *parent;
    struct __tree_val *prev;
    struct __tree_val *next;
    struct __tree_val *child;
}__tree_val;

typedef struct __tree_node {
    __ptr index[TREE_WIDTH];
}__tree_node;


void tree_inseart(__tree_node *tree, linekey_t *key, __ptr val)
{
    __ptr *index = tree->index, *parent;
    __tree_val *prev = __tree_val(index);
    
    __tree_val(index)->count ++;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    __uint8 i = 0;

    while (p != end)
    {
        parent = index;
        i = (*p) >> 4;
        if (index[i] == NULL){
            index[i] = calloc(TREE_WIDTH, sizeof(__ptr));
        }
        index = (__ptr*)index[i];
        __tree_val(index)->count ++;
        __tree_val(index)->parent = parent;

        parent = index;
        i = *p & 0x0F;
        if (index[i] == NULL){
            index[i] = calloc(TREE_WIDTH, sizeof(__ptr));
        }
        index = (__ptr*)index[i];
        __tree_val(index)->count ++;
        __tree_val(index)->parent = parent;

        p++;
    }

    __tree_val(index)->val = val;
    
    __tree_val(index)->next = prev->next;
    if (__tree_val(index)->next){
        __tree_val(index)->next->prev = __tree_val(index);
    }
    prev->next = __tree_val(index);
    __tree_val(index)->prev = prev;
}

__ptr tree_find(__tree_node *tree, linekey_t *key)
{
    __ptr *index = tree->index;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    __uint8 i = 0;
    while (p != end)
    {
        i = (*p) >> 4;
        if (index != NULL){
            index = (__ptr*)index[i];
        }else {
            break;
        }
        
        i = *p & 0x0F;
        if (index != NULL){
            index = (__ptr*)index[i];
        }else {
            break;
        }
    
        p++;
    }

    if (index != NULL){
        return __tree_val(index)->val;
    }

    return NULL;
}


void tree_delete(__tree_node *tree, linekey_t *key)
{
    // __logd(">>>>------------------------------------------------> enter %s\n", &key->byte[1]);
    __ptr *parent, *index = tree->index;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];

    while (p != end)
    {
        if (index != NULL){
            index = (__ptr*)index[((*p) >> 4)];
        }else {
            return;
        }
        if (index != NULL){
            index = (__ptr*)index[((*p) & 0x0F)];
        }else {
            return;
        }

        p++;
    }

    __tree_val(index)->prev->next = __tree_val(index)->next;
    if (__tree_val(index)->next){
        __tree_val(index)->next->prev = __tree_val(index)->prev;
    }

    p--;
    end = &key->byte[0];
    while (p != end)
    {
        parent = __tree_val(index)->parent;
        if (index != NULL){
            // __logd(">>>>---------------------> %lu\n", __tree_val(index)->count);
            if ((--(__tree_val(index)->count)) == 0){
                // __logd(">>>>---------------------> free\n");
                free(index);
                parent[((*p) & 0x0F)] = NULL;
            }
            index = parent;
        }
        
        parent = __tree_val(index)->parent;
        if (index != NULL){
            // __logd(">>>>--------------------->>> %lu\n", __tree_val(index)->count);
            if ((--(__tree_val(index)->count)) == 0){
                // __logd(">>>>---------------------> free 1\n");
                free(index);
                parent[((*p) >> 4)] = NULL;
            }
            index = parent;
        }

        p--;
    }

    if (index != NULL){
        if ((__tree_val(index)->count--) == 0){
            // free(index);
        }
    }

    // __logd(">>>>------------------------------------------------> exit %s\n", &key->byte[1]);
}



#endif