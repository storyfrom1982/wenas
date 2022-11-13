#ifndef __LINEAR_KV__
#define __LINEAR_KV__

#include "lineardb.h"

typedef struct linear_key {
    uint8_t byte[8];
}LinearKey;

typedef struct linear_key_value {
    uint32_t size, pos;
    Lineardb *value;
    LinearKey *head, *key;
}Linearkv;


static inline void linearkv_initialization(Linearkv *lkv, char *buf, uint32_t size)
{
    lkv->pos = 0;
    lkv->size = size;
    lkv->head = (LinearKey*)buf;
    lkv->value = NULL;
}

static inline Linearkv* linearkv_create(uint32_t capacity)
{
    Linearkv *lkv = (Linearkv *)malloc(sizeof(Linearkv));
    char *buf = (char *)malloc(capacity);
    linearkv_initialization(lkv, buf, capacity);
    return lkv;
}

static inline void linearkv_release(Linearkv **pp_lkv)
{
    if (pp_lkv && *pp_lkv){
        Linearkv *lkv = *pp_lkv;
        *pp_lkv = NULL;
        free(lkv->head);
        free(lkv);
    }
}

static inline void linearkv_append_string(Linearkv *lkv, char *key, char *value)
{
    lkv->key = (LinearKey *)(lkv->head->byte + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    // fprintf(stdout, "strlen(key)=%lu\n", lkv->key->byte[0]);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_bind_string(lkv->value, value);
    lkv->pos += __sizeof_block(lkv->value);
}

static inline Lineardb* linearkv_find(Linearkv *lkv, char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < lkv->pos) {
        lkv->key = (LinearKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        // fprintf(stdout, "strlen(key)=%lu byte[0]=%lu\n", strlen(key), lkv->key->byte[0]);
        // fprintf(stdout, "memcmp()=%d\n", strncmp(&lkv->key->byte[1], key, lkv->key->byte[0]));
        // fprintf(stdout, "key=%s\n", &lkv->key->byte[1]);
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
        find_pos += __sizeof_block(lkv->value);
    }
    return NULL;
}

static inline Lineardb* linearkv_find_from(Linearkv *lkv, Lineardb *from, char *key)
{
    uint32_t find_pos = ((from->byte) - (lkv->head->byte) + __sizeof_block(from));
    // fprintf(stdout, "find pos = %u head pos = %u\n", find_pos, lkv->pos);
    while (find_pos < lkv->pos) {
        lkv->key = (LinearKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
        find_pos += __sizeof_block(lkv->value);
    }

    // fprintf(stdout, "find from head =======================>>>>>>>>>>>>>>>>\n");
    // int i = 0;

    find_pos = 0;
    do {
        lkv->key = (LinearKey *)(lkv->head->byte + find_pos);
        find_pos += (lkv->key->byte[0] + 2);
        lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
        find_pos += __sizeof_block(lkv->value);
        // fprintf(stdout, "find count=%d\n", i++);
        if (strlen(key) == lkv->key->byte[0]
            && memcmp(&lkv->key->byte[1], key, lkv->key->byte[0]) == 0){
            return lkv->value;
        }
    } while (lkv->value != from);
    

    return NULL;
}


#endif //__LINEAR_KV__