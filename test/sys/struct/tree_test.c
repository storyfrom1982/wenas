#include "sys/struct/tree.h"
#include "ex/malloc.h"

static void malloc_debug_cb(const char *debug)
{
    __ex_loge("%s\n", debug);
}

static void free_mapping(void *mapping)
{
    __ex_logd("free tree node mapping: %s\n", (char*)mapping);
    free(mapping);
}

int main(int argc, char *argv[])
{
    __ex_backtrace_setup();
    // const char *root = "a";
    const char *keys[] ={
                            "aaa",      "aaab",     "az",       "aaabbb",   "aaabbc",   "aaabcc",   "aaabcd",   "aaabcde", 
                            "bbb",      "bbba",     "by",       "bbbb",     "bbbc",     "bbbd",     "bbbe",     "bbbf", 
                            "ccc",      "cdef",     "cdefa",    "cdefab",   "cdefabc",  "cddefc",   "cabcde",   "cbabdef", 
                            "zzz",      "zzzx",     "zaaaaaaa", "zzzxxx",   "zzzxxy",   "zzzxyy",   "zzzxyz",   "zzzxyza"
                        };

    __tree tree = tree_create();
    // __tree_node(tree)->mapping = root;
    // __tree_node(tree)->count = 0;

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __ex_logd("keys[i] = %s\n", keys[i]);
        tree_inseart(tree, keys[i], strlen(keys[i]), strdup(keys[i]));
    }

    {
        const char *key = "zzz";
        __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 1\n");
        __node_list *tmp, *sort = tree_down(tree, key, strlen(key), 32);
        __ex_logd("sort ----------- >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 2\n");
        while (sort)
        {
            tmp = sort;
            __ex_logd("sort down >>>>----------------------------> %s\n", sort->node->mapping);
            sort = sort->next;
            free(tmp);
        }
    }    

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