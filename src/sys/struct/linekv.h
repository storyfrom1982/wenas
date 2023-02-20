#ifndef __LINEKV_H__
#define __LINEKV_H__

#include "linedb.h"

typedef struct linedb* lineval_ptr;

typedef struct linekey {
    char byte[2];
}*linekey_ptr;

typedef struct linekv {
    uint64_t pos, len;
    linekey_ptr key;
    lineval_ptr val;
    unsigned char *head;
}*linekv_ptr;


static inline linekv_ptr linekv_create(uint64_t size)
{
    linekv_ptr lkv = (linekv_ptr)malloc(sizeof(struct linekv));
    lkv->head = (unsigned char*) malloc(size);
    lkv->key = (linekey_ptr )(lkv->head);
    lkv->len = size;
    lkv->pos = 0;
    lkv->val = NULL;
    return lkv;
}

static inline void linekv_destroy(linekv_ptr *pp_lkv)
{
    if (pp_lkv && *pp_lkv){
        linekv_ptr lkv = *pp_lkv;
        *pp_lkv = NULL;
        if (lkv->head){
            free(lkv->head);
        }
        free(lkv);
    }
}

static inline void linekv_bind_object(linekv_ptr lkv, lineval_ptr val)
{
    lkv->len = lkv->pos = __sizeof_data(val);
    lkv->head = __dataof_linedb(val);
    lkv->key = (linekey_ptr )(lkv->head);
    lkv->val = NULL;
}

static inline void linekv_load_object(linekv_ptr lkv, lineval_ptr val)
{
    uint64_t size = __sizeof_data(val);
    if (size > lkv->len){
        linekv_destroy(&lkv);
        lkv = linekv_create(size);
    }
    lkv->pos = size;
    memcpy(lkv->head, __dataof_linedb(val), size);
    lkv->key = (linekey_ptr )(lkv->head);
    lkv->val = NULL;
}

static inline void linekv_clear(linekv_ptr lkv)
{
    if (lkv){
        lkv->pos = 0;
        lkv->key = (linekey_ptr )(lkv->head);
        lkv->val = NULL;
    }
}

static inline void linekv_add_binary(linekv_ptr lkv, const char *key, const void *value, uint64_t size)
{
    lkv->key = (linekey_ptr )(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_ptr )(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_filled_data(lkv->val, value, size, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_BINARY);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_str(linekv_ptr lkv, const char *key, const char *value)
{
    lkv->key = (linekey_ptr )(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_ptr )(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_filled_data(lkv->val, value, strlen(value) + 1, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_STRING);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_obj(linekv_ptr lkv, const char *key, linekv_ptr obj)
{
    lkv->key = (linekey_ptr )(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key); //TODO check len > 255
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_ptr )(lkv->key->byte + (lkv->key->byte[0] + 2));
    linedb_filled_data(lkv->val, obj->head, obj->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_CUSTOM);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_number(linekv_ptr lkv, const char *key, struct linedb val)
{
    lkv->key = (linekey_ptr )(lkv->head + lkv->pos);
    lkv->key->byte[0] = strlen(key);
    memcpy(&(lkv->key->byte[1]), key, lkv->key->byte[0]);
    lkv->key->byte[lkv->key->byte[0] + 1] = '\0';
    lkv->pos += (lkv->key->byte[0] + 2);
    lkv->val = (lineval_ptr )(lkv->key->byte + (lkv->key->byte[0] + 2));
    *lkv->val = val;
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_int8(linekv_ptr lkv, const char *key, int8_t n8)
{
    linekv_add_number(lkv, key, __n2b8(n8));
}

static inline void linekv_add_int16(linekv_ptr lkv, const char *key, int16_t n16)
{
    linekv_add_number(lkv, key, __n2b16(n16));
}

static inline void linekv_add_int32(linekv_ptr lkv, const char *key, int32_t n32)
{
    linekv_add_number(lkv, key, __n2b32(n32));
}

static inline void linekv_add_int64(linekv_ptr lkv, const char *key, int64_t n64)
{
    linekv_add_number(lkv, key, __n2b64(n64));
}

static inline void linekv_add_uint8(linekv_ptr lkv, const char *key, uint8_t u8)
{
    linekv_add_number(lkv, key, __u2b8(u8));
}

static inline void linekv_add_uint16(linekv_ptr lkv, const char *key, uint16_t u16)
{
    linekv_add_number(lkv, key, __u2b16(u16));
}

static inline void linekv_add_uint32(linekv_ptr lkv, const char *key, uint32_t u32)
{
    linekv_add_number(lkv, key, __u2b32(u32));
}

static inline void linekv_add_uint64(linekv_ptr lkv, const char *key, uint64_t u64)
{
    linekv_add_number(lkv, key, __u2b64(u64));
}

static inline void linekv_add_float32(linekv_ptr lkv, const char *key, float f32)
{
    linekv_add_number(lkv, key, __f2b32(f32));
}

static inline void linekv_add_float64(linekv_ptr lkv, const char *key, double f64)
{
    linekv_add_number(lkv, key, __f2b64(f64));
}

static inline void linekv_add_ptr(linekv_ptr lkv, const char *key, __ptr p)
{
    int64_t n = (int64_t)(p);
    linekv_add_number(lkv, key, __n2b64(n));
}

static inline void linekv_add_bool(linekv_ptr lkv, const char *key, bool b)
{
    linekv_add_number(lkv, key, __bool_to_byte(b));
}

static inline lineval_ptr linekv_head(linekv_ptr parser)
{
    parser->key = (linekey_ptr )(parser->head);
    parser->val = (lineval_ptr )(parser->key->byte + (parser->key->byte[0] + 2));
    return parser->val;
}

static inline lineval_ptr linekv_next(linekv_ptr parser)
{
    if (parser->val){
        lineval_ptr start_val = parser->val;
        uint64_t find_pos = ((start_val->byte) - (parser->head) + __sizeof_linedb(parser->val));
        if (find_pos < parser->pos){
            parser->key = (linekey_ptr )(parser->head + find_pos);
            parser->val = (lineval_ptr )(parser->key->byte + (parser->key->byte[0] + 2));
            return parser->val;
        }
    }
    return NULL;
}

static inline const char* linekv_current_key(linekv_ptr parser)
{
    if (parser->key){
        return (const char *)&(parser->key->byte[1]);
    }
    return "";
}

static inline lineval_ptr linekv_find(linekv_ptr parser, const char *key)
{
    uint64_t find_pos = 0;
    while (find_pos < parser->pos) {
        parser->key = (linekey_ptr )(parser->head + find_pos);
        find_pos += (parser->key->byte[0] + 2);
        parser->val = (lineval_ptr )(parser->key->byte + (parser->key->byte[0] + 2));
        if (strlen(key) == parser->key->byte[0]
            && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
            return parser->val;
        }
        find_pos += __sizeof_linedb(parser->val);
    }
    return NULL;
}

static inline lineval_ptr linekv_after(linekv_ptr parser, const char *key)
{
    if (parser->val){

        lineval_ptr start_val = parser->val;
        uint64_t find_pos = ((start_val->byte) - (parser->head) + __sizeof_linedb(parser->val));

        while (find_pos < parser->pos) {
            parser->key = (linekey_ptr )(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->val = (lineval_ptr )(parser->key->byte + (parser->key->byte[0] + 2));
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->val;
            }
            find_pos += __sizeof_linedb(parser->val);
        }

        find_pos = 0;

        do {
            parser->key = (linekey_ptr )(parser->head + find_pos);
            find_pos += (parser->key->byte[0] + 2);
            parser->val = (lineval_ptr )(parser->key->byte + (parser->key->byte[0] + 2));
            find_pos += __sizeof_linedb(parser->val);
            if (strlen(key) == parser->key->byte[0]
                && memcmp(&parser->key->byte[1], key, parser->key->byte[0]) == 0){
                return parser->val;
            }
        } while (parser->val != start_val);
    }

    return NULL;
}

static inline bool linekv_find_bool(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __byte_to_bool(val);
    }
    return 0;
}

static inline int8_t linekv_find_int8(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n8(val);
    }
    return 0;
}

static inline int16_t linekv_find_int16(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n16(val);
    }
    return 0;
}

static inline int32_t linekv_find_int32(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n32(val);
    }
    return 0;
}

static inline int64_t linekv_find_int64(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2n64(val);
    }
    return 0;
}

static inline uint8_t linekv_find_uint8(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u8(val);
    }
    return 0;
}

static inline uint16_t linekv_find_uint16(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u16(val);
    }
    return 0;
}

static inline uint32_t linekv_find_uint32(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u32(val);
    }
    return 0;
}

static inline uint64_t linekv_find_uint64(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2u64(val);
    }
    return 0;
}

static inline float linekv_find_float32(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2f32(val);
    }
    return 0.0f;
}

static inline double linekv_find_float64(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return __b2f64(val);
    }
    return 0.0f;
}

static inline void* linekv_find_ptr(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return (void *)(__b2n64(val));
    }
    return NULL;
}

static inline const char* linekv_find_str(linekv_ptr parser, const char *key)
{
    lineval_ptr val = linekv_after(parser, key);
    if (val == NULL){
        val = linekv_find(parser, key);
    }
    if (val){
        return (const char *)__dataof_linedb(val);
    }
    return NULL;
}

#endif //__LINEKV_H__