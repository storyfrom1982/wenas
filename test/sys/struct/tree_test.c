#include "sys/struct/tree.h"
#include "env/malloc.h"

static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}

static void free_mapping(void *mapping)
{
    __logd("free tree node mapping: %s\n", (char*)mapping);
    free(mapping);
}

int main(int argc, char *argv[])
{
    // const char *root = "a";
    const char *keys[] ={
                            "aaa",      "aaab",     "az",       "aaabbb",   "aaabbc",   "aaabcc",   "aaabcd",   "aaabcde", 
                            "bbb",      "bbbb",     "by",       "bbbbbb",   "bbbbbc",   "bbbbcc",   "bbbbcd",   "bbbbcde", 
                            "ccc",      "cdef",     "cdefa",    "cdefab",   "cdefabc",  "cddefc",   "cabcde",   "cbabdef", 
                            "zzz",      "zzzx",     "zaaaaaaa", "zzzxxx",   "zzzxxy",   "zzzxyy",   "zzzxyz",   "zzzxyza"
                        };

    __tree tree = tree_create();
    // __tree_node(tree)->mapping = root;
    // __tree_node(tree)->count = 0;

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] = %s\n", keys[i]);
        tree_inseart(tree, keys[i], strlen(keys[i]), strdup(keys[i]));
    }

    {
        const char *key = "bbb";
        __node_list *tmp, *sort = tree_up(tree, key, strlen(key), 32);
        while (sort)
        {
            tmp = sort;
            __logd("sort up >>>>----------------------------> %s\n", sort->node->mapping);
            sort = sort->next;
            free(tmp);
        }
    }


    {
        const char *key = "bbb";
        __node_list *tmp, *sort = tree_down(tree, key, strlen(key), 32);
        while (sort)
        {
            tmp = sort;
            __logd("sort down >>>>----------------------------> %s\n", sort->node->mapping);
            sort = sort->next;
            free(tmp);
        }
    }
    

    __ptr min = tree_min(tree);
    __logd("min >>>>------------> %s\n", min);

    __ptr max = tree_max(tree);
    __logd("max >>>>------------> %s\n", max);
    
    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        char *mapping = tree_find(tree, keys[i], strlen(keys[i]));
        __logd("find key =%s mapping = %s\n", keys[i], mapping);
    }

    __logd("tree node count -- %lu\n", __tree2node(tree)->route);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*) / 2; ++i){
        __logd("delete key ================================= %s\n", keys[i]);
        __ptr val = tree_delete(tree, keys[i], strlen(keys[i]));
        __logd("delete val ================================= %s\n", (char*)val);
        free(val);
    }

    __logd("tree node count ---- %lu\n", __tree2node(tree)->route);

    for (int i = 0; i < sizeof(keys) / sizeof(const char*); ++i){
        // __logd("keys[i] ================================= %s\n", keys[i]);
        char *mapping = tree_find(tree, keys[i], strlen(keys[i]));
        __logd("----->>> key =%s mapping = %s\n", keys[i], mapping);
    }

    __logd("tree node count %lu\n", __tree2node(tree)->route);
    tree_clear(tree, free_mapping);
    tree_release(&tree);

#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif
}