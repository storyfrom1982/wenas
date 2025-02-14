
#ifndef __UUID_LIST_H__
#define __UUID_LIST_H__


#include "xalloc.h"
#include "xlib/avlmini.h"


#define UUID_BIN_BUF_LEN       32  
#define UUID_HEX_BUF_LEN       65


void* uuid_generate(void *uuid_bin_buf, const char *user_name);


static inline unsigned char* str2uuid(const char* hexstr, uint8_t *uuid) {
    for (size_t i = 0; i < UUID_BIN_BUF_LEN; ++i) {
        char a = hexstr[i * 2];
        char b = hexstr[i * 2 + 1];
        a = (a >= '0' && a <= '9') ? a - '0' : a - 'A' + 10;
        b = (b >= '0' && b <= '9') ? b - '0' : b - 'A' + 10;
        uuid[i] = ((a << 4) | b);
    }
    return uuid;
}

static inline char* uuid2str(const unsigned char *uuid, char *hexstr) {
    const static char hex_chars[] = "0123456789ABCDEF";
    for (size_t i = 0; i < UUID_BIN_BUF_LEN; ++i) {
        hexstr[i * 2] = hex_chars[(uuid[i] >> 4) & 0x0F];
        hexstr[i * 2 + 1] = hex_chars[uuid[i] & 0x0F];
    }
    hexstr[UUID_HEX_BUF_LEN-1] = '\0';
    return hexstr;
}

typedef struct uuid_node {
    struct avl_node node;
    struct uuid_node *next, *prev;
    uint64_t uuid[4];
    uint64_t hash_key;
    uint32_t list_len;
}uuid_node_t;


typedef struct uuid_list {
    uint64_t count;
    struct avl_tree hash_tree;
    struct avl_tree uuid_tree;
}uuid_list_t;


void uuid_list_init(uuid_list_t *ul);
void uuid_list_clear(uuid_list_t *ul);
void uuid_list_add(uuid_list_t *ul, uuid_node_t *node);
uuid_node_t* uuid_list_del(uuid_list_t *ul, uuid_node_t *node);
uuid_node_t* uuid_list_find(uuid_list_t *ul, uint64_t hash_key, uint64_t *uuid);
uuid_node_t* uuid_list_find_by_hash(uuid_list_t *utree, uint64_t hash_key);
uuid_node_t* uuid_list_first(uuid_list_t *ul);
uuid_node_t* uuid_list_last(uuid_list_t *ul);
uuid_node_t* uuid_list_next(uuid_list_t *ul, uuid_node_t *node);
uuid_node_t* uuid_list_prev(uuid_list_t *ul, uuid_node_t *node);


#endif