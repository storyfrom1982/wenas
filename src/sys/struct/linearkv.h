#ifndef __LINEAR_KV__
#define __LINEAR_KV__

#include "lineardb.h"

typedef struct linear_key {
    uint8_t byte[8];
}lkey_t;

typedef struct linear_key_value {
    uint32_t len, pos;
    lkey_t *key;
    lineardb_t *value;
    uint8_t head[1];
}linearkv_t, *linearkv_parser_t;


static inline linearkv_t* lkv_build(uint32_t size)
{
    linearkv_t *lkv = (linearkv_t *)malloc(sizeof(linearkv_t) + size);
    lkv->len = size;
    lkv->pos = 0;
    lkv->key = NULL;
    lkv->value = NULL;
    return lkv;
}

static inline void lkv_destroy(linearkv_t **pp_lkv)
{
    if (pp_lkv && *pp_lkv){
        linearkv_t *lkv = *pp_lkv;
        *pp_lkv = NULL;
        free(lkv);
    }
}

static inline void lkv_clear(linearkv_t *lkv)
{
    if (lkv){
        lkv->pos = 0;
    }
}

static inline void lkv_add_str(linearkv_t *lkv, const char *key, char *value)
{
    lkv->key = (lkey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (lineardb_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_load_string(lkv->value, value);
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_obj(linearkv_t *lkv, const char *key, lineardb_t *ldb)
{
    lkv->key = (lkey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (lineardb_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    lineardb_load_binary(lkv->value, __dataof_block(ldb), __sizeof_data(ldb));
    lkv->value->byte[0] |= BLOCK_TYPE_BLOCK;
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_number(linearkv_t *lkv, const char *key, lineardb_t ldb)
{
    lkv->key = (lkey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->value = (lineardb_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->value = ldb;
    lkv->pos += __sizeof_block(lkv->value);
}

static inline void lkv_add_n8(linearkv_t *lkv, const char *key, int8_t n8)
{
    lkv_add_number(lkv, key, __n2b8(n8));
}

static inline void lkv_add_n16(linearkv_t *lkv, const char *key, int16_t n16)
{
    lkv_add_number(lkv, key, __n2b16(n16));
}

static inline void lkv_add_n32(linearkv_t *lkv, const char *key, int32_t n32)
{
    lkv_add_number(lkv, key, __n2b32(n32));
}

static inline void lkv_add_n64(linearkv_t *lkv, const char *key, int64_t n64)
{
    lkv_add_number(lkv, key, __n2b64(n64));
}

static inline void lkv_add_u8(linearkv_t *lkv, const char *key, uint8_t u8)
{
    lkv_add_number(lkv, key, __u2b8(u8));
}

static inline void lkv_add_u16(linearkv_t *lkv, const char *key, uint16_t u16)
{
    lkv_add_number(lkv, key, __u2b16(u16));
}

static inline void lkv_add_u32(linearkv_t *lkv, const char *key, uint32_t u32)
{
    lkv_add_number(lkv, key, __u2b32(u32));
}

static inline void lkv_add_u64(linearkv_t *lkv, const char *key, uint64_t u64)
{
    lkv_add_number(lkv, key, __u2b64(u64));
}

static inline void lkv_add_f32(linearkv_t *lkv, const char *key, float f32)
{
    lkv_add_number(lkv, key, __f2b32(f32));
}

static inline void lkv_add_f64(linearkv_t *lkv, const char *key, double f64)
{
    lkv_add_number(lkv, key, __f2b64(f64));
}

static inline void lkv_add_ptr(linearkv_t *lkv, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    lkv_add_number(lkv, key, __n2b64(n));
}

static inline void lkv_add_bool(linearkv_t *lkv, const char *key, uint8_t b)
{
    lkv_add_number(lkv, key, __boolean2block(b));
}

static inline lineardb_t* lkv_find(linearkv_parser_t parser, const char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < parser->pos) {
        parser->key = (lkey_t *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->value = (lineardb_t *)(parser->key->byte + (parser->key->byte[0] + 2));
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->value;
        }
        find_pos += __sizeof_block(parser->value);
    }
    return NULL;
}

static inline lineardb_t* lkv_after(linearkv_parser_t parser, const char *key)
{
    if (parser->value){

        lineardb_t *start_pos = parser->value;
        uint32_t find_pos = ((start_pos->byte) - (parser->head) + __sizeof_block(parser->value));

        while (find_pos < parser->pos) {
            parser->key = (lkey_t *)(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->value = (lineardb_t *)(parser->key->byte + (parser->key->byte[0] + 2));
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
            parser->value = (lineardb_t *)(parser->key->byte + (parser->key->byte[0] + 2));
            find_pos += __sizeof_block(parser->value);
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->value;
            }
        } while (parser->value != start_pos);
    }

    return NULL;
}

static inline uint8_t lkv_find_bool(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __block2boolean(ldb);
    }
    return 0;
}

static inline int8_t lkv_find_n8(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n8(ldb);
    }
    return 0;
}

static inline int16_t lkv_find_n16(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n16(ldb);
    }
    return 0;
}

static inline int32_t lkv_find_n32(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n32(ldb);
    }
    return 0;
}

static inline int64_t lkv_find_n64(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2n64(ldb);
    }
    return 0;
}

static inline uint8_t lkv_find_u8(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2u8(ldb);
    }
    return 0;
}

static inline uint16_t lkv_find_u16(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2u16(ldb);
    }
    return 0;
}

static inline uint32_t lkv_find_u32(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2u32(ldb);
    }
    return 0;
}

static inline uint64_t lkv_find_u64(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2u64(ldb);
    }
    return 0;
}

static inline float lkv_find_f32(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2f32(ldb);
    }
    return 0.0f;
}

static inline double lkv_find_f64(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return __b2f64(ldb);
    }
    return 0.0f;
}

static inline void* lkv_find_ptr(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return (void *)(__b2n64(ldb));
    }
    return NULL;
}

static inline const char* lkv_find_str(linearkv_parser_t parser, const char *key)
{
    lineardb_t *ldb = lkv_after(parser, key);
    if (ldb == NULL){
        ldb = lkv_find(parser, key);
    }
    if (ldb){
        return (const char *)__dataof_block(ldb);
    }
    return NULL;
}

#endif //__LINEAR_KV__