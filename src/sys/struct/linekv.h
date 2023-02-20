#ifndef __LINEKV_H__
#define __LINEKV_H__

#include "linedb.h"


#define LINEKEY_HEAD_SIZE       1
#define LINEKEY_TAIL_SIZE       1
#define LINEKEY_MAX_LENGTH      UINT8_MAX

typedef struct linekey {
    char byte[2];
}*linekey_ptr;

#define __sizeof_linekey(k) \
        (k->byte[0] + LINEKEY_HEAD_SIZE + LINEKEY_TAIL_SIZE)

typedef struct linedb* lineval_ptr;

typedef struct linekv {
    uint32_t reader;
    uint64_t pos, len;
    linekey_ptr key;
    lineval_ptr val;
    uint8_t *head;
}*linekv_ptr;


#define LINEKV_INIT_SIZE    0x100000 //1M


static inline linekey_ptr linekey_bind_string(linekey_ptr key, const char *str)
{
    uint8_t len = strlen(str);
    if (len > LINEKEY_MAX_LENGTH - (LINEKEY_HEAD_SIZE + LINEKEY_TAIL_SIZE)){
        len = LINEKEY_MAX_LENGTH - (LINEKEY_HEAD_SIZE + LINEKEY_TAIL_SIZE);
    }
    memcpy(key->byte + LINEKEY_HEAD_SIZE, str, len);
    *(key->byte + (len + LINEKEY_TAIL_SIZE)) = '\0';
    key->byte[0] = len;
    return key;
}

static inline linekey_ptr linekey_from_string(const char *str)
{
    linekey_ptr key = (linekey_ptr)malloc(LINEKEY_MAX_LENGTH);
    linekey_bind_string(key, str);
    return key;
}

static inline const char* linekey_to_string(linekey_ptr key)
{
    if (key){
        return (const char*)(key->byte + LINEKEY_HEAD_SIZE);
    }
    return "";
}


static inline linekv_ptr linekv_writer_create()
{
    linekv_ptr lkv = (linekv_ptr)malloc(sizeof(struct linekv));
    lkv->head = (uint8_t*) malloc(LINEKV_INIT_SIZE);
    lkv->reader = 0;
    lkv->key = (linekey_ptr )(lkv->head);
    lkv->len = LINEKV_INIT_SIZE;
    lkv->pos = 0;
    lkv->val = NULL;
    return lkv;
}

static inline void linekv_free(linekv_ptr *pptr)
{
    if (pptr && *pptr && !((*pptr)->reader)){
        linekv_ptr lkv = *pptr;
        *pptr = NULL;
        free(lkv->head);
        free(lkv);
    }
}

static inline void linekv_reader_load(linekv_ptr lkv, lineval_ptr val)
{
    lkv->reader = 1;
    lkv->len = lkv->pos = __sizeof_data(val);
    lkv->head = __dataof_linedb(val);
    lkv->key = (linekey_ptr)(lkv->head);
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

static inline void linekv_add_value(linekv_ptr lkv, const char *key, const void *val, uint64_t size, uint8_t flag)
{
    while ((lkv->len - lkv->pos) < (UINT8_MAX + 9 + size)){
        lkv->len += LINEKV_INIT_SIZE;
        void *p = malloc(lkv->len);
        memcpy(p, lkv->head, lkv->pos);
        free(lkv->head);
        lkv->head = (uint8_t*)p;
    }
    lkv->key = (linekey_ptr)(lkv->head + lkv->pos);
    linekey_bind_string(lkv->key, key);
    lkv->pos += __sizeof_linekey(lkv->key);
    lkv->val = (lineval_ptr)(lkv->key->byte + __sizeof_linekey(lkv->key));
    linedb_filled_data(lkv->val, val, size, flag);
    lkv->pos += __sizeof_linedb(lkv->val);
}

static inline void linekv_add_binary(linekv_ptr lkv, const char *key, const void *val, uint64_t size)
{
    linekv_add_value(lkv, key, val, size, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_BINARY);
}

static inline void linekv_add_string(linekv_ptr lkv, const char *key, const char *str)
{
    linekv_add_value(lkv, key, str, strlen(str) + 1, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_STRING);
}

static inline void linekv_add_object(linekv_ptr lkv, const char *key, linekv_ptr obj)
{
    linekv_add_value(lkv, key, obj->head, obj->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_CUSTOM);
}

static inline void linekv_add_array(linekv_ptr lkv, const char *key, linearray_ptr val)
{
    linekv_add_value(lkv, key, __dataof_linedb(val->head), __sizeof_data(val->head), LINEDB_TYPE_OBJECT | LINEDB_OBJECT_ARRAY);
}

static inline void linekv_add_number(linekv_ptr lkv, const char *key, struct linedb val)
{
    while ((lkv->len - lkv->pos) < (UINT8_MAX + __sizeof_linedb(&val))){
        lkv->len += LINEKV_INIT_SIZE;
        void *p = malloc(lkv->len);
        memcpy(p, lkv->head, lkv->pos);
        free(lkv->head);
        lkv->head = (uint8_t*)p;
    }
    lkv->key = (linekey_ptr)(lkv->head + lkv->pos);
    linekey_bind_string(lkv->key, key);
    lkv->pos += __sizeof_linekey(lkv->key);
    lkv->val = (lineval_ptr)(lkv->key->byte + __sizeof_linekey(lkv->key));
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

static inline lineval_ptr linekv_head(linekv_ptr reader)
{
    reader->key = (linekey_ptr)(reader->head);
    reader->val = (lineval_ptr)(reader->key->byte + __sizeof_linekey(reader->key));
    return reader->val;
}

static inline lineval_ptr linekv_next(linekv_ptr reader)
{
    if (reader->val){
        uint64_t find_pos = ((reader->val->byte) - (reader->head) + __sizeof_linedb(reader->val));
        if (find_pos < reader->pos){
            reader->key = (linekey_ptr)(reader->head + find_pos);
            reader->val = (lineval_ptr)(reader->key->byte + __sizeof_linekey(reader->key));
            return reader->val;
        }
    }
    return NULL;
}

static inline lineval_ptr linekv_find(linekv_ptr reader, const char *key)
{
    uint64_t find_pos = 0;
    while (find_pos < reader->pos) {
        reader->key = (linekey_ptr)(reader->head + find_pos);
        find_pos += __sizeof_linekey(reader->key);
        reader->val = (lineval_ptr)(reader->key->byte + __sizeof_linekey(reader->key));
        if (strlen(key) == reader->key->byte[0]
            && memcmp(&reader->key->byte[1], key, reader->key->byte[0]) == 0){
            return reader->val;
        }
        find_pos += __sizeof_linedb(reader->val);
    }
    return NULL;
}

static inline lineval_ptr linekv_after(linekv_ptr reader, const char *key)
{
    if (reader->val){

        lineval_ptr start_val = reader->val;
        uint64_t find_pos = ((start_val->byte) - (reader->head) + __sizeof_linedb(reader->val));

        while (find_pos < reader->pos) {
            reader->key = (linekey_ptr)(reader->head + find_pos);
            find_pos += __sizeof_linekey(reader->key);
            reader->val = (lineval_ptr)(reader->key->byte + __sizeof_linekey(reader->key));
            if (strlen(key) == reader->key->byte[0]
                && memcmp(&reader->key->byte[1], key, reader->key->byte[0]) == 0){
                return reader->val;
            }
            find_pos += __sizeof_linedb(reader->val);
        }

        find_pos = 0;

        do {
            reader->key = (linekey_ptr)(reader->head + find_pos);
            find_pos += __sizeof_linekey(reader->key);
            reader->val = (lineval_ptr)(reader->key->byte + __sizeof_linekey(reader->key));
            find_pos += __sizeof_linedb(reader->val);
            if (strlen(key) == reader->key->byte[0]
                && memcmp(&reader->key->byte[1], key, reader->key->byte[0]) == 0){
                return reader->val;
            }
        } while (reader->val != start_val);
    }

    return NULL;
}

static inline bool linekv_find_bool(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __byte_to_bool(val);
    }
    return 0;
}

static inline int8_t linekv_find_int8(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2n8(val);
    }
    return 0;
}

static inline int16_t linekv_find_int16(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2n16(val);
    }
    return 0;
}

static inline int32_t linekv_find_int32(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2n32(val);
    }
    return 0;
}

static inline int64_t linekv_find_int64(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2n64(val);
    }
    return 0;
}

static inline uint8_t linekv_find_uint8(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2u8(val);
    }
    return 0;
}

static inline uint16_t linekv_find_uint16(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2u16(val);
    }
    return 0;
}

static inline uint32_t linekv_find_uint32(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2u32(val);
    }
    return 0;
}

static inline uint64_t linekv_find_uint64(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
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

static inline double linekv_find_float64(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return __b2f64(val);
    }
    return 0.0f;
}

static inline void* linekv_find_ptr(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return (void *)(__b2n64(val));
    }
    return NULL;
}

static inline const char* linekv_find_string(linekv_ptr reader, const char *key)
{
    lineval_ptr val = linekv_after(reader, key);
    if (val == NULL){
        val = linekv_find(reader, key);
    }
    if (val){
        return (const char *)__dataof_linedb(val);
    }
    return NULL;
}

#endif //__LINEKV_H__