#include "sys/struct/tree.h"


static void free_val(void *val)
{
    __logd("free tree node val ============================================== %s\n", (char*)val);
}

void tree_test()
{

    // const char *root = "a";
    const char *keys[] ={
                            "aaa",      "aaab",     "az",       "aaabbb",   "aaabbc",   "aaabcc",   "aaabcd",   "aaabcde", 
                            "bbb",      "bbbb",     "by",       "bbbbbb",   "bbbbbc",   "bbbbcc",   "bbbbcd",   "bbbbcde", 
                            "ccc",      "cdef",     "cdefa",    "cdefab",   "cdefabc",  "cddefc",   "cabcde",   "cbabdef", 
                            "zzz",      "zzzx",     "zaaaaaaa", "zzzxxx",   "zzzxxy",   "zzzxyy",   "zzzxyz",   "zzzxyza"
                        };

    __tree tree = tree_create();
    // __tree_node(tree)->val = root;
    // __tree_node(tree)->count = 0;

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] = %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        tree_inseart(tree, key, (__ptr)keys[i]);
        free(key);
    }

    {
        const char *tmp_key = "bbb";
        linekey_t *key = (linekey_t *)malloc(strlen(tmp_key) + 2);
        key->byte[0] = strlen(tmp_key);
        memcpy(&key->byte[1], tmp_key, strlen(tmp_key));
        __tree_node *sort = tree_sort_up(tree, key, 32);
        free(key);
        while (sort)
        {
            __logd("sort up >>>>----------------------------> %s\n", sort->val);
            sort = sort->order;
        }
    }


    {
        const char *tmp_key = "bbb";
        linekey_t *key = (linekey_t *)malloc(strlen(tmp_key) + 2);
        key->byte[0] = strlen(tmp_key);
        memcpy(&key->byte[1], tmp_key, strlen(tmp_key));
        __tree_node *sort = tree_sort_down(tree, key, 32);
        free(key);
        while (sort)
        {
            __logd("sort down >>>>----------------------------> %s\n", sort->val);
            sort = sort->order;
        }
    }
    

    __ptr min = tree_min(tree);
    __logd("min >>>>------------> %s\n", min);

    __ptr max = tree_max(tree);
    __logd("max >>>>------------> %s\n", max);
    
    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        char *val = tree_find(tree, key);
        __logd("key =%s val = %s\n", (char*)&key->byte[1], val);
        free(key);
    }

    __logd("tree node count -- %lu\n", __tree2node(tree)->count);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*) / 2; ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        tree_delete(tree, key, free_val);
        free(key);
    }

    __logd("tree node count ---- %lu\n", __tree2node(tree)->count);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        char *val = tree_find(tree, key);
        __logd("----->>> key =%s val = %s\n", (char*)&key->byte[1], val);
        free(key);
    }

    __tree_node *next = __tree2node(tree)->next;
    while (next)
    {
        __logd("tree node val %s\n", (char*)next->val);
        next = next->next;
    }
    

    // for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
    //     // __logd("keys[i] ================================= %s\n", keys[i]);
    //     linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
    //     key->byte[0] = strlen(keys[i]);
    //     memcpy(&key->byte[1], keys[i], strlen(keys[i]));
    //     key->byte[key->byte[0]+1] = '\0';
    //     // __logd("key = %s node count $$$-------> %lu\n", keys[i], __tree_node(tree->index)->count);
    //     tree_delete(tree, key, free_val);
    //     __logd("key = %s node count -------> %lu\n", keys[i], __tree2node(tree)->count);
    //     free(key);
    // }

    __logd("tree node count %lu\n", __tree2node(tree)->count);
    tree_clear(tree, free_val);
    tree_destroy(&tree);
}