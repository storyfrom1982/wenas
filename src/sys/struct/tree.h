#ifndef __SYS_TREE_H__
#define __SYS_TREE_H__

#include <env/env.h>
#include <sys/struct/linekv.h>


#define TREE_NODE_RED       1
#define TREE_NODE_BLACK     0


typedef struct __tree_node {
    __ptr index[17];
}__tree_node;


void tree_inseart(__tree_node *tree, linekey_t *key, __ptr val)
{
    __uint64 len = key->byte[0];
    __ptr *index = tree->index;
    char *p = &key->byte[1];
    __uint8 i = 0;
    while (len > 0)
    {
        // __logd("inseart len %u key = %c\n", len, *p);
        i = *p & 0x0F;
        // __logd("inseart index = %u\n", i);
        if (index[i] == NULL){
            index[i] = calloc(17, sizeof(__ptr));
        }
        index = (__ptr*)index[i];

        i = (*p) >> 4;
        // __logd("inseart index = %u\n", i);
        if (index[i] == NULL){
            index[i] = calloc(17, sizeof(__ptr));
        }
        index = (__ptr*)index[i];

        p++;
        len--;

        if (len == 0){
            index[16] = val;
            // __logd("inseart val = %s addr = %p\n", (char*)index[16], index[16]);
        }
    }
}

__ptr tree_find(__tree_node *tree, linekey_t *key)
{
    __uint64 len = key->byte[0];
    __ptr *index = tree->index;
    char *p = &key->byte[1];
    __uint8 i = 0;
    while (len > 0)
    {
        // __logd("find len %u key = %c\n", len, *p);
        i = *p & 0x0F;
        if (index != NULL){
            // __logd("find index = %u\n", i);
            index = (__ptr*)index[i];
        }
        
        i = (*p) >> 4;
        if (index != NULL){
            // __logd("find index = %u\n", i);
            index = (__ptr*)index[i];
        }
    
        p++;
        len--;

        if (len == 0){
            // __logd("find val = %s addr %p\n", (char*)index[16], index[16]);
            return index[16];
        }
    }

    return NULL;
}



#endif