#ifndef __LINEAR_KV__
#define __LINEAR_KV__

#include "lineardb.h"

#define LKV_BUILDER_MAX_SIZE    10240

typedef struct linear_key {
    uint8_t byte[8];
}lkey_t;

typedef struct linear_key_value {
    uint32_t len, pos;
    lkey_t *key;
    Lineardb *value;
    uint8_t head[LKV_BUILDER_MAX_SIZE];
}lkv_builder_t, *lkv_parser_t;


static inline void lkv_clear(lkv_builder_t *builder)
{
    builder->len = LKV_BUILDER_MAX_SIZE;
    builder->pos = 0;
    builder->key = NULL;
    builder->value = NULL;
}

static inline void lkv_add_str(lkv_builder_t *lkv, char *key, char *value)
{
    lkv->key = (lkey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_load_string(lkv->value, value);
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_number(lkv_builder_t *lkv, char *key, Lineardb ldb)
{
    lkv->key = (lkey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (Lineardb *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->value = ldb;
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_n8(lkv_builder_t *lkv, char *key, int8_t n)
{
    lkv_add_number(lkv, key, __n2b8(n));
}

static inline void lkv_add_n16(lkv_builder_t *lkv, char *key, int16_t n)
{
    lkv_add_number(lkv, key, __n2b16(n));
}

static inline void lkv_add_n32(lkv_builder_t *lkv, char *key, int32_t n)
{
    lkv_add_number(lkv, key, __n2b32(n));
}

static inline void lkv_add_n64(lkv_builder_t *lkv, char *key, int64_t n)
{
    lkv_add_number(lkv, key, __n2b64(n));
}

static inline void lkv_add_f32(lkv_builder_t *lkv, char *key, float f)
{
    lkv_add_number(lkv, key, f2b32(f));
}

static inline void lkv_add_f64(lkv_builder_t *lkv, char *key, double f)
{
    lkv_add_number(lkv, key, f2b64(f));
}

static inline void lkv_add_ptr(lkv_builder_t *lkv, char *key, void *p)
{
    lkv_add_number(lkv, key, n2b64((int64_t)(p)));
}

static inline Lineardb* lkv_find(lkv_parser_t parser, char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < parser->pos) {
        parser->key = (lkey_t *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->value;
        }
        find_pos += __sizeof_block(parser->value);
    }
    return NULL;
}

static inline Lineardb* lkv_after(lkv_parser_t parser, char *key)
{
    if (parser->value){

        Lineardb *start_pos = parser->value;
        uint32_t find_pos = ((start_pos->byte) - (parser->head) + __sizeof_block(parser->value));

        while (find_pos < parser->pos) {
            parser->key = (lkey_t *)(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->value;
            }
            find_pos += __sizeof_block(parser->value);
        }

        find_pos = 0;

        do {
            parser->key = (lkey_t *)(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->value = (Lineardb *)(parser->key->byte + (parser->key->byte[0] + 2));
            find_pos += __sizeof_block(parser->value);
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->value;
            }
        } while (parser->value != start_pos);
    }

    return NULL;
}

static inline int8_t lkv_find_n8(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n8(ldb);
    }
    return 0;
}

static inline int16_t lkv_find_n16(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n16(ldb);
    }
    return 0;
}

static inline int32_t lkv_find_n32(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n32(ldb);
    }
    return 0;
}

static inline int64_t lkv_find_n64(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n64(ldb);
    }
    return 0;
}

static inline float lkv_find_f32(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return b2f32(ldb);
    }
    return 0.0f;
}

static inline double lkv_find_f64(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return b2f64(ldb);
    }
    return 0.0f;
}

static inline void* lkv_find_ptr(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return (void *)(__b2n64(ldb));
    }
    return NULL;
}

static inline const char* lkv_find_str(lkv_parser_t parser, char *key)
{
    Lineardb *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return (const char *)__dataof_block(ldb);
    }
    return NULL;
}

#endif //__LINEAR_KV__