#ifndef __XTABLE_H__
#define __XTABLE_H__

#include "xmalloc.h"
#include "xlib/avlmini.h"


#define ORIGIN_KEY_MAX      32


typedef struct xnode {
    struct avl_node node;
    uint64_t hash;
    uint8_t *key;
    uint32_t key_len;
    uint32_t list_len;
    struct xnode *next, *prev;
    void *value;
}xnode_t;


xnode_t* xnode_create(uint64_t hash, uint8_t *key, uint8_t key_len);

typedef struct xhash_table {
    uint64_t count;
    uint32_t size; //must be the power of 2
    uint32_t mask;
    xnode_t head;
    xnode_t **table;
    struct avl_tree tree;
}xhash_table_t;


void xhash_table_init(xhash_table_t *table, uint32_t size /*must be the power of 2*/);
void xhash_table_clear(xhash_table_t *table);
void xhash_table_add(xhash_table_t *table, const char *key, void *value);
void* xhash_table_del(xhash_table_t *table, const char *key);
void* xhash_table_find(xhash_table_t *table, const char *key);



typedef struct xtree_table {
    uint64_t count;
    struct avl_tree hash_tree;
    struct avl_tree origin_tree;
}xtree_table_t;

void xtree_table_init(xtree_table_t *table);
void xtree_table_clear(xtree_table_t *table);
void xtree_table_add(xtree_table_t *table, xnode_t *node);
xnode_t* xtree_table_remove(xtree_table_t *table, xnode_t *node);
xnode_t* xtree_table_find(xtree_table_t *table, uint64_t hash, uint8_t *key, uint32_t len);
xnode_t* xtree_table_find_same_hash(xtree_table_t *table, uint64_t hash);

xnode_t *xtree_table_first(xtree_table_t *table);
xnode_t *xtree_table_last(xtree_table_t *table);
xnode_t *xtree_table_next(xtree_table_t *table, xnode_t *node);
xnode_t *xtree_table_prev(xtree_table_t *table, xnode_t *node);

#endif