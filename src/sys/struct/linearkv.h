#ifndef __LINEAR_KV__
#define __LINEAR_KV__

#include "lineardb.h"

typedef linedb_t lineval_t;

typedef struct linear_key {
    uint8_t byte[8];
}linekey_t;

typedef struct linear_key_value_pair {
    uint32_t pos, len;
    linekey_t *key;
    lineval_t *val;
    uint8_t *head;
}linekv_t, *linekv_parser_t;

enum {
    LINEDB_OBJECT_ARRAY = LINEDB_OBJECT_CUSTOM,
    LINEDB_OBJECT_LINEKV = LINEDB_OBJECT_RESERVED
};

#define __objectis_array(b)         (__typeof_linedb(b) == LINEDB_OBJECT_ARRAY)
#define __objectis_linekv(b)        (__typeof_linedb(b) == LINEDB_OBJECT_LINEKV)


static inline linekv_t* linekv_build(uint32_t size)
{
    linekv_t *lkv = (linekv_t *)malloc(sizeof(linekv_t));
    lkv->head = (uint8_t*) malloc(size);
    lkv->key = (linekey_t *)(lkv->head);
    lkv->len = size;
    lkv->pos = 0;
    lkv->val = NULL;
    return lkv;
}

static inline void linekv_destroy(linekv_t **pp_lkv)
{
    if (pp_lkv && *pp_lkv){
        linekv_t *lkv = *pp_lkv;
        *pp_lkv = NULL;
        if (lkv->head){
            free(lkv->head);
        }
        free(lkv);
    }
}

static inline void linekv_bind_object(linekv_t *lkv, lineval_t *val)
{
    lkv->len = lkv->pos = __sizeof_data(val);
    lkv->head = __dataof_linedb(val);
    lkv->key = (linekey_t *)(lkv->head);
    lkv->val = NULL;
}

static inline void linekv_load_object(linekv_t *lkv, lineval_t *val)
{
    uint32_t size = __sizeof_data(val);
    if (size > lkv->len){
        linekv_destroy(&lkv);
        lkv = linekv_build(size);
    }
    lkv->pos = size;
    memcpy(lkv->head, __dataof_linedb(val), size);
    lkv->key = (linekey_t *)(lkv->head);
    lkv->val = NULL;
}

static inline void linekv_clear(linekv_t *lkv)
{
    if (lkv){
        lkv->pos = 0;
        lkv->key = (linekey_t *)(lkv->head);
        lkv->val = NULL;
    }
}

static inline void linekv_add_binary(linekv_t *lkv, const char *key, const void *value, uint32_t size)
{
    lkv->key = (linekey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_load_binary(lkv->val, value, size);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_str(linekv_t *lkv, const char *key, const char *value)
{
    lkv->key = (linekey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_load_string(lkv->val, value);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_obj(linekv_t *lkv, const char *key, linekv_t *obj)
{
    lkv->key = (linekey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_load_binary(lkv->val, obj->head, obj->pos);
    lkv->val->byte[0] |= LINEDB_OBJECT_LINEKV;
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_number(linekv_t *lkv, const char *key, lineval_t val)
{
    lkv->key = (linekey_t *)(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_t *)(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->val = val;
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_int8(linekv_t *lkv, const char *key, int8_t n8)
{
    linekv_add_number(lkv, key, __n2b8(n8));
}

static inline void linekv_add_int16(linekv_t *lkv, const char *key, int16_t n16)
{
    linekv_add_number(lkv, key, __n2b16(n16));
}

static inline void linekv_add_int32(linekv_t *lkv, const char *key, int32_t n32)
{
    linekv_add_number(lkv, key, __n2b32(n32));
}

static inline void linekv_add_int64(linekv_t *lkv, const char *key, int64_t n64)
{
    linekv_add_number(lkv, key, __n2b64(n64));
}

static inline void linekv_add_uint8(linekv_t *lkv, const char *key, uint8_t u8)
{
    linekv_add_number(lkv, key, __u2b8(u8));
}

static inline void linekv_add_uint16(linekv_t *lkv, const char *key, uint16_t u16)
{
    linekv_add_number(lkv, key, __u2b16(u16));
}

static inline void linekv_add_uint32(linekv_t *lkv, const char *key, uint32_t u32)
{
    linekv_add_number(lkv, key, __u2b32(u32));
}

static inline void linekv_add_uint64(linekv_t *lkv, const char *key, uint64_t u64)
{
    linekv_add_number(lkv, key, __u2b64(u64));
}

static inline void linekv_add_float32(linekv_t *lkv, const char *key, float f32)
{
    linekv_add_number(lkv, key, __f2b32(f32));
}

static inline void linekv_add_float64(linekv_t *lkv, const char *key, double f64)
{
    linekv_add_number(lkv, key, __f2b64(f64));
}

static inline void linekv_add_ptr(linekv_t *lkv, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    linekv_add_number(lkv, key, __n2b64(n));
}

static inline void linekv_add_bool(linekv_t *lkv, const char *key, uint8_t b)
{
    linekv_add_number(lkv, key, __boolean2block(b));
}

static inline lineval_t* linekv_head(linekv_parser_t parser)
{
    parser->key = (linekey_t *)(parser->head);
    parser->val = (lineval_t *)(parser->key->byte + (parser->key->byte[0] + 2));
    return parser->val;
}

static inline lineval_t* linekv_next(linekv_parser_t parser)
{
    if (parser->val){
        lineval_t *start_val = parser->val;
        uint32_t find_pos = ((start_val->byte) - (parser->head) + __sizeof_linedb(parser->val));
        if (find_pos < parser->pos){
            parser->key = (linekey_t *)(parser->head + find_pos);
            parser->val = (lineval_t *)(parser->key->byte + (parser->key->byte[0] + 2));
            return parser->val;
        }
    }
    return NULL;
}

static inline const char* linekv_current_key(linekv_parser_t parser)
{
    if (parser->key){
        return (const char *)&(parser->key->byte[1]);
    }
    return "";
}

static inline lineval_t* linekv_find(linekv_parser_t parser, const char *key)
{
    uint32_t find_pos = 0;
    while (find_pos < parser->pos) {
        parser->key = (linekey_t *)(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->val = (lineval_t *)(parser->key->byte + (parser->key->byte[0] + 2));
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->val;
        }
        find_pos += __sizeof_linedb(parser->val);
    }
    return NULL;
}

static inline lineval_t* linekv_after(linekv_parser_t parser, const char *key)
{
    if (parser->val){

        lineval_t *start_val = parser->val;
        uint32_t find_pos = ((start_val->byte) - (parser->head) + __sizeof_linedb(parser->val));

        while (find_pos < parser->pos) {
            parser->key = (linekey_t *)(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->val = (lineval_t *)(parser->key->byte + (parser->key->byte[0] + 2));
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->val;
            }
            find_pos += __sizeof_linedb(parser->val);
        }

        find_pos = 0;

        do {
            parser->key = (linekey_t *)(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->val = (lineval_t *)(parser->key->byte + (parser->key->byte[0] + 2));
            find_pos += __sizeof_linedb(parser->val);
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->val;
            }
        } while (parser->val != start_val);
    }

    return NULL;
}

static inline uint8_t linekv_find_bool(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __block2boolean(val);
    }
    return 0;
}

static inline int8_t linekv_find_int8(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n8(val);
    }
    return 0;
}

static inline int16_t linekv_find_int16(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n16(val);
    }
    return 0;
}

static inline int32_t linekv_find_int32(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n32(val);
    }
    return 0;
}

static inline int64_t linekv_find_int64(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n64(val);
    }
    return 0;
}

static inline uint8_t linekv_find_uint8(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u8(val);
    }
    return 0;
}

static inline uint16_t linekv_find_uint16(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u16(val);
    }
    return 0;
}

static inline uint32_t linekv_find_uint32(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u32(val);
    }
    return 0;
}

static inline uint64_t linekv_find_uint64(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u64(val);
    }
    return 0;
}

static inline float linekv_find_float32(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2f32(val);
    }
    return 0.0f;
}

static inline double linekv_find_float64(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2f64(val);
    }
    return 0.0f;
}

static inline void* linekv_find_ptr(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return (void *)(__b2n64(val));
    }
    return NULL;
}

static inline const char* linekv_find_str(linekv_parser_t parser, const char *key)
{
    lineval_t *val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return (const char *)__dataof_linedb(val);
    }
    return NULL;
}

#endif //__LINEAR_KV__