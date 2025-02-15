#ifndef __XTABLE_H__
#define __XTABLE_H__


#include "xmalloc.h"
#include "xlib/avlmini.h"


typedef struct search_node {
    struct avl_node node;
    struct search_node *next, *prev;
    uint64_t key;
    uint8_t *uuid;
    uint32_t uuid_len;
    uint32_t list_len;
    void *value;
}search_node_t;

typedef struct search_table {
    uint64_t count;
    uint32_t size; //must be the power of 2
    uint32_t mask;
    search_node_t head;
    search_node_t **table;
    struct avl_tree tree;
}search_table_t;

void search_table_init(search_table_t *st, uint32_t size /*must be the power of 2*/);
void search_table_clear(search_table_t *st);
void search_table_add(search_table_t *st, const char *key, void *value);
void* search_table_del(search_table_t *st, const char *key);
void* search_table_find(search_table_t *st, const char *key);


typedef struct index_node {
    struct avl_node node;
    struct index_node *next, *prev;
    uint64_t index;
    uint32_t list_len;
}index_node_t;

typedef struct index_table {
    uint64_t count;
    uint32_t size; //must be the power of 2
    uint32_t mask;
    index_node_t head;
    index_node_t **table;
    struct avl_tree tree;
}index_table_t;

void index_table_init(index_table_t *it, uint32_t size /*must be the power of 2*/);
void index_table_clear(index_table_t *it);
void index_table_add(index_table_t *it, index_node_t *node);
void* index_table_del(index_table_t *it, index_node_t *node);
void* index_table_find(index_table_t *it, uint64_t index);



#endif