#include "sys/struct/tree.h"
#include "ex/malloc.h"

static void malloc_debug_cb(const char *debug)
{
    __ex_loge("%s\n", debug);
}

static void free_mapping(void *mapping)
{
    // __ex_logd("free tree node mapping: %d\n", *(int*)mapping);
    free(mapping);
}

int main(int argc, char *argv[])
{
    __ex_backtrace_setup();
    // const char *root = "a";
    const char *keys[] ={
                            "aaa",      "aaab",     "az",       "aaabbb",   "aaabbc",   "aaabcc",   "aaabcd",   "aaabcde", 
                            "bbb",      "bbba",     "by",       "bbbb",     "bbbc",     "bbbd",     "bbbe",     "bbbz", 
                            "ccc",      "cdef",     "cdefa",    "cdefab",   "cdefabc",  "cddefc",   "cabcde",   "cbabdef", 
                            "zzz",      "zzzx",     "zaaaaaaa", "zzzxxx",   "zzzxxy",   "zzzxyy",   "zzzxyz",   "zzzxyza"
                        };

    __tree tree = tree_create();
    // __tree_node(tree)->mapping = root;
    // __tree_node(tree)->count = 0;


#if 0
    const int ikeys[] = {
                        0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 
                        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
                        32753, 32754, 32755, 32756, 32757, 32758, 32759, 32760, 32761, 32762, 32763, 32764, 32765, 32766, 32767, 32768, 
                        65521, 65522, 65523, 65524, 65525, 65526, 65527, 65528, 65529, 65530, 65531, 65532, 65533, 65534, 65535, 65536,
    };

    // int key = 1;
    // tree_inseart(tree, &key, sizeof(int), &key);
    // key = 32768;
    // tree_inseart(tree, &key, sizeof(int), &key);    
    // key = 65536;
    // tree_inseart(tree, &key, sizeof(int), &key);

    for (int i = 0; i < sizeof(ikeys) / sizeof(int); ++i){
        __ex_logd("keys[i] = %d\n", ikeys[i]);
        tree_inseart(tree, &ikeys[i], sizeof(int), &ikeys[i]);
    }

    for (int i = 0; i < sizeof(ikeys) / sizeof(int); ++i){
        int *mapping = tree_find(tree, &ikeys[i], sizeof(int));
        __ex_logd("find key =%d mapping = %d\n", ikeys[i], *mapping);
    }

    {
        int key = 2;
        __node_list *tmp, *sort = tree_up(tree, &key, sizeof(key), 64);
        while (sort)
        {
            tmp = sort;
            __ex_logd("sort up >>>>----------------------------> %d\n", *(int*)sort->node->mapping);
            sort = sort->next;
            free(tmp);
        }
    }     

    // tree_clear(tree, free_mapping);

    // {
    //     int key = 31;
    //     __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 1\n");
    //     __node_list *tmp, *sort = tree_down(tree, &key, sizeof(int), 64);
    //     __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 2\n");
    //     while (sort)
    //     {
    //         tmp = sort;
    //         __ex_logd("sort down >>>>----------------------------> %d\n", *(int*)sort->node->mapping);
    //         sort = sort->next;
    //         free(tmp);
    //     }
    // }

    return 0;
#endif

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __ex_logd("keys[i] = %s\n", keys[i]);
        tree_inseart(tree, keys[i], strlen(keys[i]), strdup(keys[i]));
    }

    // {
    //     const char *key = "zzzxyza";
    //     __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 1\n");
    //     __node_list *tmp, *sort = tree_down(tree, key, strlen(key), 32);
    //     __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 2\n");
    //     while (sort)
    //     {
    //         tmp = sort;
    //         __ex_logd("sort down >>>>----------------------------> %s\n", sort->node->mapping);
    //         sort = sort->next;
    //         free(tmp);
    //     }
    // }    

    {
        const char *key = "bbb";
        __node_list *tmp, *sort = tree_up(tree, key, strlen(key), 32);
        while (sort)
        {
            tmp = sort;
            __ex_logd("sort up >>>>----------------------------> %s\n", sort->node->mapping);
            sort = sort->next;
            free(tmp);
        }
    }

    __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    

    __ptr min = tree_min(tree);
    __ex_logd("min >>>>------------> %s\n", min);

    __ptr max = tree_max(tree);
    __ex_logd("max >>>>------------> %s\n", max);
    
    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        char *mapping = tree_find(tree, keys[i], strlen(keys[i]));
        __ex_logd("find key =%s mapping = %s\n", keys[i], mapping);
    }

    __ex_logd("tree node count -- %lu\n", __tree2node(tree)->route);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*) / 2; ++i){
        __ex_logd("delete key ================================= %s\n", keys[i]);
        __ptr val = tree_delete(tree, keys[i], strlen(keys[i]));
        __ex_logd("delete val ================================= %s\n", (char*)val);
        free(val);
    }

    __ex_logd("tree node count ---- %lu\n", __tree2node(tree)->route);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __ex_logd("keys[i] ================================= %s\n", keys[i]);
        char *mapping = tree_find(tree, keys[i], strlen(keys[i]));
        __ex_logd("----->>> key =%s mapping = %s\n", keys[i], mapping);
    }

    __ex_logd("tree node count %lu\n", __tree2node(tree)->route);
    tree_clear(tree, free_mapping);
    tree_release(&tree);

#if defined(ENV_MALLOC_BACKTRACE)
    __ex_memory_leak_trace(malloc_debug_cb);
#endif
}