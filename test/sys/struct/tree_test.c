#include "sys/struct/tree.h"


void tree_test()
{

    // const char *root = "a";
    const char *keys[] = {"aecd", "adcd", "abcd", "accd", "aacd", "bbcd", "zzzzzxyz", "cacd", "ccdc", "aaaabcd"};

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

    __logd("tree node count -- %lu\n", __treenode(tree)->count);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*) / 2; ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        tree_delete(tree, key);
        free(key);
    }

    __logd("tree node count ---- %lu\n", __treenode(tree)->count);

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

    __tree_node *next = __treenode(tree)->next;
    while (next)
    {
        __logd("tree node val %s\n", (char*)next->val);
        next = next->next;
    }
    

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        // __logd("key = %s node count $$$-------> %lu\n", keys[i], __tree_node(tree->index)->count);
        tree_delete(tree, key);
        __logd("key = %s node count -------> %lu\n", keys[i], __treenode(tree)->count);
        free(key);
    }

    __logd("tree node count %lu\n", __treenode(tree)->count);
    tree_destroy(&tree);
}