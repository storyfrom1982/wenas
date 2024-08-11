#ifndef __XTABLE_H__
#define __XTABLE_H__


#include "xmalloc.h"
#include "xlib/avlmini.h"


typedef struct xhtnode {
    struct avl_node node;
    uint64_t key;
    uint8_t *uuid;
    uint32_t uuid_len;
    uint32_t list_len;
    struct xhtnode *next, *prev;
    void *value;
}xhtnode_t;

xhtnode_t* xhtnode_create(uint64_t hash_key, uint8_t *uuid, uint8_t uuid_len);



typedef struct xhash_table {
    uint64_t count;
    uint32_t size; //must be the power of 2
    uint32_t mask;
    xhtnode_t head;
    xhtnode_t **table;
    struct avl_tree tree;
}xhash_table_t;

void xhash_table_init(xhash_table_t *ht, uint32_t size /*must be the power of 2*/);
void xhash_table_clear(xhash_table_t *ht);
void xhash_table_add(xhash_table_t *ht, const char *key, void *value);
void* xhash_table_del(xhash_table_t *ht, const char *key);
void* xhash_table_find(xhash_table_t *ht, const char *key);



typedef struct xhash_tree {
    uint64_t count;
    struct avl_tree hash_tree;
    struct avl_tree uuid_tree;
}xhash_tree_t;

void xhash_tree_init(xhash_tree_t *ht);
void xhash_tree_clear(xhash_tree_t *ht);
void xhash_tree_add(xhash_tree_t *ht, xhtnode_t *node);
xhtnode_t* xhash_tree_del(xhash_tree_t *ht, xhtnode_t *node);
xhtnode_t* xhash_tree_find(xhash_tree_t *ht, uint64_t key, const uint8_t *uuid, uint32_t len);
xhtnode_t* xhash_tree_find_by_key(xhash_tree_t *ht, uint64_t key);

xhtnode_t *xhash_tree_first(xhash_tree_t *ht);
xhtnode_t *xhash_tree_last(xhash_tree_t *ht);
xhtnode_t *xhash_tree_next(xhash_tree_t *ht, xhtnode_t *node);
xhtnode_t *xhash_tree_prev(xhash_tree_t *ht, xhtnode_t *node);

#endif