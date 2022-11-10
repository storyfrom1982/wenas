#ifndef __LINEAR_KEY_VALUE__
#define __LINEAR_KEY_VALUE__

#include "linear_data_block.h"

typedef struct linear_key_value {
    uint32_t capacity, pos;
    Lineardb *db, *key, *value;
}Linearkv;


static inline void linearkv_initialization(Linearkv *lkv, char *buf, uint32_t capacity)
{
    lkv->pos = 0;
    lkv->capacity = capacity;
    lkv->key = lkv->db = (Lineardb*)buf;
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
        free(lkv->db);
        free(lkv);
    }
}

static inline void linearkv_append(Linearkv *lkv, Lineardb *key, Lineardb *value)
{
    uint32_t key_size = __block_size(key), value_size = __block_size(value);
    // fprintf(stdout, "key size %u\n", key_size);
    // Lineardb v = *value;
    // fprintf(stdout, "key %s -> value %d\n", __block_byte(key), __block_to_number32(value).i);
    memcpy(lkv->db->byte + lkv->pos, key->byte, key_size);
    lkv->pos += key_size;
    memcpy(lkv->db->byte + lkv->pos, value->byte, value_size);
    lkv->pos += value_size;
    // fprintf(stdout, "lkv pos %u\n", lkv->pos);
}

static inline Lineardb* linearkv_find(Linearkv *lkv, Lineardb *key)
{
    uint32_t find_pos = 0, key_size, value_size, src_key_size = __block_size(key);
    // fprintf(stdout, "find_pos pos =========== %u\n", find_pos);
    while (find_pos < lkv->pos) {
        lkv->key = (Lineardb*)(lkv->db->byte + find_pos);
        key_size = __block_size(lkv->key);
        // fprintf(stdout, "key size %u\n", src_key_size);
        lkv->value = (Lineardb*)(lkv->key->byte + key_size);
        value_size = __block_size(lkv->value);
        // fprintf(stdout, "key %s -> src %s\n", __block_byte(lkv->key), __block_byte(key));
        // fprintf(stdout, "head size %u\n", __block_head_size(key));
        if (key_size == src_key_size && strncmp(__block_byte(lkv->key), __block_byte(key), key_size - __block_head_size(key)) == 0){
            return lkv->value;
        }
        find_pos += (key_size + value_size);
        // fprintf(stdout, "find_pos pos %u %u %u\n", __block_size(lkv->value), key_size, find_pos);
    }
    return NULL;
}

#endif //__LINEAR_KEY_VALUE__