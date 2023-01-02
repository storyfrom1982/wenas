#include "sys/struct/tree.h"


void tree_test()
{

    const char *keys[] = {"abcd", "abcde", "abcf", "cdab", "xyz", "qpw", "ffff", "ccaaxxkkee", "llmmnnhhjjggyy", "rwsmnvxzqp"};

    __tree_node *tree = (__tree_node *)calloc(1, sizeof(__tree_node));

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        __logd("keys[i] = %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        tree_inseart(tree, key, (__ptr)keys[i]);
    }
    
    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        linekey_t *key = (linekey_t *)malloc(strlen(keys[i]) + 2);
        key->byte[0] = strlen(keys[i]);
        memcpy(&key->byte[1], keys[i], strlen(keys[i]));
        key->byte[key->byte[0]+1] = '\0';
        char *val = tree_find(tree, key);
        __logd("val = %s\n", val);
    }
}