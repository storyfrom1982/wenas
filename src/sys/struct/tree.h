#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>


#define TREE_WIDTH              21
#define TREE_COUNT_INDEX        16
#define TREE_VALUE_INDEX        17

typedef struct __tree_val {
    __ptr val;
    struct __tree_val *prev;
    struct __tree_val *next;
    struct __tree_val *child;
}__tree_val;

typedef struct __tree_node {
    __ptr index[TREE_WIDTH];
}__tree_node;


void tree_inseart(__tree_node *tree, linekey_t *key, __ptr val)
{
    __ptr *index = tree->index;
    __tree_val *prev, val_node = {.val = val, .next = NULL};
    
    ((__uint64*)index)[TREE_COUNT_INDEX] ++;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    __uint8 i = 0;

    while (p != end)
    {
        if (((__tree_val*)&index[TREE_VALUE_INDEX])->val != NULL){
            prev = ((__tree_val*)&index[TREE_VALUE_INDEX]);
            // __logd("prev key = %s\n", (char *)prev->val);
        }

        i = *p & 0x0F;
        if (index[i] == NULL){
            index[i] = calloc(TREE_WIDTH, sizeof(__ptr));
        }
        index = (__ptr*)index[i];
        ((__uint64*)index)[TREE_COUNT_INDEX] ++;

        i = (*p) >> 4;
        if (index[i] == NULL){
            index[i] = calloc(TREE_WIDTH, sizeof(__ptr));
        }
        index = (__ptr*)index[i];
        ((__uint64*)index)[TREE_COUNT_INDEX] ++;

        p++;
    }

    *(__tree_val*)&index[TREE_VALUE_INDEX] = val_node;
    
    // while (prev && *p < prev->key)
    // {
    //     prev = prev->next;
    // }
    
    ((__tree_val*)&index[TREE_VALUE_INDEX])->next = prev->next;
    if (((__tree_val*)&index[TREE_VALUE_INDEX])->next){
        ((__tree_val*)&index[TREE_VALUE_INDEX])->next->prev = ((__tree_val*)&index[TREE_VALUE_INDEX]);
    }
    prev->next = ((__tree_val*)&index[TREE_VALUE_INDEX]);
    ((__tree_val*)&index[TREE_VALUE_INDEX])->prev = prev;
}

__ptr tree_find(__tree_node *tree, linekey_t *key)
{
    __ptr *index = tree->index;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    __uint8 i = 0;
    while (p != end)
    {
        i = *p & 0x0F;
        if (index != NULL){
            index = (__ptr*)index[i];
        }else {
            break;
        }
        
        i = (*p) >> 4;
        if (index != NULL){
            index = (__ptr*)index[i];
        }else {
            break;
        }
    
        p++;
    }

    if (index != NULL){
        return ((__tree_val*)&index[TREE_VALUE_INDEX])->val;
    }

    return NULL;
}


void tree_delete(__tree_node *tree, linekey_t *key)
{
    __ptr *index = tree->index;
    __ptr *tmp = index;
    unsigned char *p = &key->byte[1], *end = p + key->byte[0];
    __uint8 i = 0;
    while (p != end)
    {
        i = *p & 0x0F;
        if (index != NULL){
            (((__uint64*)index)[TREE_COUNT_INDEX])--;
            index = (__ptr*)index[i];
            if ((((__uint64*)tmp)[TREE_COUNT_INDEX]) == 0){
                free(tmp);
            }
            tmp = index;
        }else {
            return;
        }
        
        i = (*p) >> 4;
        if (index != NULL){
            (((__uint64*)index)[TREE_COUNT_INDEX])--;
            index = (__ptr*)index[i];
            if ((((__uint64*)tmp)[TREE_COUNT_INDEX]) == 0){
                free(tmp);
            }
            tmp = index;
        }else {
            return;
        }

        p++;
    }

    if (--(((__uint64*)tmp)[TREE_COUNT_INDEX]) == 0){
        ((__tree_val*)&tmp[TREE_VALUE_INDEX])->prev->next = ((__tree_val*)&tmp[TREE_VALUE_INDEX])->next;
        if (((__tree_val*)&tmp[TREE_VALUE_INDEX])->next){
            ((__tree_val*)&tmp[TREE_VALUE_INDEX])->next->prev = ((__tree_val*)&tmp[TREE_VALUE_INDEX])->prev;
        }
        free(tmp);
    }
}



#endif