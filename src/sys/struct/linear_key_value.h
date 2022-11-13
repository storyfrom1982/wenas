#ifndef __LINEAR_KEY_VALUE__
#define __LINEAR_KEY_VALUE__

#include "linear_data_block.h"

typedef struct linear_key_value {
    uint32_t size, pos;
    Lineardb *head, *end, *key, *value;
}Linearkv;


static inline void linearkv_initialization(Linearkv *lkv, char *buf, uint32_t capacity)
{
    lkv->pos = 0;
    lkv->size = capacity;
    lkv->key = lkv->head = (Lineardb*)buf;
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
    lkv->key = (Lineardb *)(lkv->head->byte + lkv->pos);
    lineardb_bind_bytes(lkv->key, key, strlen(key) + 1);
    lkv->pos += __block_size(lkv->key);
    lkv->value = (Lineardb *)(lkv->head->byte + lkv->pos);
    lineardb_bind_bytes(lkv->value, value, strlen(value) + 1);
    lkv->pos += __block_size(lkv->value);
}

static inline void linearkv_append_int32(Linearkv *lkv, char *key, int32_t n)
{
    lkv->key = (Lineardb *)(lkv->head->byte + lkv->pos);
    lineardb_bind_bytes(lkv->key, key, strlen(key) + 1);
    lkv->pos += __block_size(lkv->key);
    lkv->value = (Lineardb *)(lkv->head->byte + lkv->pos);
    *lkv->value = __number32_to_block(n);
    lkv->pos += __block_size(lkv->value);
}

static inline void linearkv_append_uint32(Linearkv *lkv, char *key, uint32_t n)
{
    lkv->key = (Lineardb *)(lkv->head->byte + lkv->pos);
    lineardb_bind_bytes(lkv->key, key, strlen(key) + 1);
    lkv->pos += __block_size(lkv->key);
    lkv->value = (Lineardb *)(lkv->head->byte + lkv->pos);
    *lkv->value = __number32_to_block(n);
    lkv->pos += __block_size(lkv->value);
}

static inline void linearkv_append(Linearkv *lkv, Lineardb *key, Lineardb *value)
{
    uint32_t key_size = __block_size(key), value_size = __block_size(value);
    memcpy(lkv->head->byte + lkv->pos, key->byte, key_size);
    lkv->pos += key_size;
    memcpy(lkv->head->byte + lkv->pos, value->byte, value_size);
    lkv->pos += value_size;
}

static inline Lineardb* linearkv_find(Linearkv *lkv, Lineardb *key)
{
    uint32_t find_pos = 0, cmp_size = __block_size(key), key_size;
    while (find_pos < lkv->pos) {
        lkv->key = (Lineardb*)(lkv->head->byte + find_pos);
        key_size = __block_size(lkv->key);
        find_pos += key_size;
        lkv->value = (Lineardb*)(lkv->key->byte + key_size);
        if (key_size == cmp_size 
            && memcmp(__block_byte(lkv->key), __block_byte(key), cmp_size - __block_head_size(key)) == 0){
            return lkv->value;
        }
        find_pos += __block_size(lkv->value);
    }
    return NULL;
}

static inline Lineardb* linearkv_find_string(Linearkv *lkv, char *key)
{
    uint32_t find_pos = 0, cmp_size = strlen(key) + 1, key_size;
    while (find_pos < lkv->pos) {
        lkv->key = (Lineardb*)(lkv->head->byte + find_pos);
        key_size = __block_size(lkv->key);
        find_pos += key_size;
        lkv->value = (Lineardb*)(lkv->key->byte + key_size);
        if (key_size - __block_head_size(lkv->key) == cmp_size 
            && memcmp(__block_byte(lkv->key), key, cmp_size) == 0){
            return lkv->value;
        }
        find_pos += __block_size(lkv->value);;
    }
    return NULL;
}


#endif //__LINEAR_KEY_VALUE__