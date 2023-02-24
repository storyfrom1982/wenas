#ifndef __LINEKV_H__
#define __LINEKV_H__

#include "linedb.h"


#define LINEKEY_HEAD_SIZE       1
#define LINEKEY_TAIL_SIZE       1
#define LINEKEY_MAX_LENGTH      255

typedef struct linekey {
    char byte[2];
}*linekey_ptr;

#define __sizeof_linekey(k) \
        (k->byte[0] + LINEKEY_HEAD_SIZE)

typedef struct linedb* lineval_ptr;



enum {
    LINEARRAY_FLAG_NONE = 0x00,
    LINEARRAY_FLAG_MEMFREE = 0x01,
    LINEARRAY_FLAG_EXPANDED = 0x02
};

#define LINEARRAY_INIT_SIZE     0x10000 //64K

typedef struct linearray {
    uint32_t flag;
    uint32_t stride;
    uint64_t pos, len;
    uint8_t *head;
}*linearray_ptr;


enum {
    LINEKV_FLAG_NONE = 0x00,
    LINEKV_FLAG_MEMFREE = 0x01,
    LINEKV_FLAG_EXPANDED = 0x02
};

// #define LINEKV_INIT_SIZE        0x100000 //1M

typedef struct linekv {
    uint32_t flag;
    uint32_t stride;
    uint64_t pos, len;
    linekey_ptr key;
    lineval_ptr val;
    uint8_t *head;
    struct linekv *prev, *next;
}*linekv_ptr;



static inline linekey_ptr linekey_bind_string(linekey_ptr key, const char *str)
{
    uint8_t len = strlen(str) + LINEKEY_TAIL_SIZE;
    if (len > LINEKEY_MAX_LENGTH - LINEKEY_HEAD_SIZE){
        len = LINEKEY_MAX_LENGTH - LINEKEY_HEAD_SIZE;
    }
    memcpy(key->byte + LINEKEY_HEAD_SIZE, str, len);
    *(key->byte + (len + LINEKEY_HEAD_SIZE)) = '\0';
    key->byte[0] = len;
    return key;
}

static inline linekey_ptr linekey_from_string(const char *str)
{
    linekey_ptr key = (linekey_ptr)malloc(LINEKEY_MAX_LENGTH);
    linekey_bind_string(key, str);
    return key;
}

static inline linekey_ptr linekey_from_data(void *data, uint64_t size)
{
    linekey_ptr key = (linekey_ptr)malloc(size + 1);
    key->byte[0] = size;
    memcpy(key->byte + LINEKEY_HEAD_SIZE, data, size);
    return key;
}

static inline const char* linekey_to_string(linekey_ptr key)
{
    if (key){
        return (const char*)(key->byte + LINEKEY_HEAD_SIZE);
    }
    return "";
}


static inline linearray_ptr linearray_create(uint32_t stride)
{
    linearray_ptr la = (linearray_ptr)malloc(sizeof(struct linearray));
    la->flag = LINEARRAY_FLAG_MEMFREE;
    la->head = (uint8_t *) malloc(stride);
    la->len = stride;
    la->pos = 0;
    return la;
}


static inline void linearray_append(linearray_ptr la, linedb_ptr ldb)
{
    uint64_t ldb_size = __sizeof_linedb(ldb);
    while ((la->len - la->pos) < ldb_size){
        la->len += la->stride;
        void *p = (uint8_t *)malloc(la->len);
        memcpy(p, la->head, la->pos);
        free(la->head);
        la->head = (uint8_t *)p;
    }
    memcpy(la->head + la->pos, ldb->byte, ldb_size);
    la->pos += ldb_size;
}


static inline void linearray_load(linearray_ptr la, void *data, uint64_t size)
{
    la->flag = LINEARRAY_FLAG_NONE;
    la->pos = 0;
    la->len = size;
    la->head = (uint8_t*)data;
}


static inline linedb_ptr linearray_next(linearray_ptr la)
{
    if (la->len - la->pos > 0){
        linedb_ptr ldb = (linedb_ptr)(la->head + la->pos);
        la->pos += __sizeof_linedb(ldb);
        return ldb;
    }
    return NULL;
}


static inline void linearray_release(linearray_ptr *pptr)
{
    if (pptr && *pptr && ((*pptr)->flag & LINEARRAY_FLAG_MEMFREE)){
        linearray_ptr la = *pptr;
        *pptr = NULL;
        free(la->head);
        free(la);
    }
}


static inline linekv_ptr linekv_create(uint32_t stride)
{
    linekv_ptr lkv = (linekv_ptr)malloc(sizeof(struct linekv) + stride);
    lkv->flag = LINEKV_FLAG_MEMFREE;
    lkv->head = (uint8_t*) malloc(stride);
    lkv->stride = stride;
    lkv->key = (linekey_ptr)(lkv->head);
    lkv->len = stride;
    lkv->pos = 0;
    lkv->val = NULL;
    return lkv;
}

static inline void linekv_release(linekv_ptr *pptr)
{
    if (pptr && *pptr && ((*pptr)->flag & LINEKV_FLAG_MEMFREE)){
        linekv_ptr lkv = *pptr;
        *pptr = NULL;
        free(lkv->head);
        free(lkv);
    }
}

static inline void linekv_load(linekv_ptr lkv, void *data, uint64_t size)
{
    lkv->flag = LINEKV_FLAG_NONE;
    lkv->len = lkv->pos = size;
    lkv->head = (uint8_t*)data;
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
    while ((lkv->len - lkv->pos) < (LINEKEY_MAX_LENGTH + LINEDB_HEAD_MAX_SIZE + size)){
        lkv->len += lkv->stride;
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

static inline void linekv_add_string(linekv_ptr lkv, const char *key, const char *str)
{
    linekv_add_value(lkv, key, str, strlen(str) + 1, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_STRING);
}

static inline void linekv_add_object(linekv_ptr lkv, const char *key, linekv_ptr obj)
{
    linekv_add_value(lkv, key, obj->head, obj->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_CUSTOM);
}

static inline void linekv_add_array(linekv_ptr lkv, const char *key, linearray_ptr array)
{
    linekv_add_value(lkv, key, array->head, array->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_ARRAY);
}

static inline void linekv_add_binary(linekv_ptr lkv, const char *key, const void *val, uint64_t size)
{
    linekv_add_value(lkv, key, val, size, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_BINARY);
}

static inline void linekv_add_number(linekv_ptr lkv, const char *key, struct linedb val)
{
    while ((lkv->len - lkv->pos) < (LINEKEY_MAX_LENGTH + __sizeof_linedb(&val))){
        lkv->len += lkv->stride;
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

static inline lineval_ptr linekv_first(linekv_ptr reader)
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
        if (strlen(key) == reader->key->byte[0] - LINEKEY_TAIL_SIZE
            && memcmp(&reader->key->byte[1], key, reader->key->byte[0] - LINEKEY_TAIL_SIZE) == 0){
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
            if (strlen(key) == reader->key->byte[0] - LINEKEY_TAIL_SIZE
                && memcmp(&reader->key->byte[1], key, reader->key->byte[0] - LINEKEY_TAIL_SIZE) == 0){
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
            if (strlen(key) == reader->key->byte[0] - LINEKEY_TAIL_SIZE
                && memcmp(&reader->key->byte[1], key, reader->key->byte[0] - LINEKEY_TAIL_SIZE) == 0){
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