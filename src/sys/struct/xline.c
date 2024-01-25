#include <stdint.h>

// #define XLINE_NUMBER_SIZE_MASK     0x1f //00011111 开启 XLINE_TYPE_4BIT 字节（ 保留低位 5bit，64bit 是 16 个字节，16 需要占用 5bit ）
#define XLINE_NUMBER_SIZE_MASK     0x0f //00011111 保留低位 4bit，获取 xline 为 number 类型时，数据的长度
#define XLINE_CONTENT_TYPE_MASK     0xc0 //11000000 保留高位 2bit，获取 xline 的数据类型

enum {
    // XLINE_TYPE_4BIT = 0x01,
    XLINE_TYPE_8BIT = 0x01, //8比特数
    XLINE_TYPE_16BIT = 0x02, //16比特数
    XLINE_TYPE_32BIT = 0x04, //32比特数
    XLINE_TYPE_64BIT = 0x08, //64比特数
    XLINE_TYPE_OBJECT = 0x10 //可变长度数据
    // 开启 4bit 字节，XLINE_TYPE_OBJECT 将递增到 0x20
    // XLINE_TYPE_OBJECT = 0x20
};

enum {
    XLINE_NUMBER_TYPE_BOOL = 0x00, //布尔数值类型
    XLINE_NUMBER_TYPE_NATURAL = 0x40, //自然数值类型
    XLINE_NUMBER_TYPE_INTEGER = 0x80, //整数值类型
    XLINE_NUMBER_TYPE_REAL = 0xc0 //实数值类型
};

enum {
    XLINE_OBJECT_TYPE_BIN = 0x00, //以字节为单位长度的二进制数据类型
    XLINE_OBJECT_TYPE_TEXT = 0x40, //以字典形式存储的数据类型
    XLINE_OBJECT_TYPE_MAP = 0x80, //以列表形式存储的数据类型
    XLINE_OBJECT_TYPE_LIST = 0xc0 //以文本形式存储的数据类型
};


#ifndef NULL
#   define NULL ((void*)0)
#endif

//xline 静态分配的最大长度（ 64bit 数的长度为 8 字节，加 1 字节头部标志位 ）
#define XLINE_STATIC_SIZE      9


typedef struct xline {
    uint8_t byte[XLINE_STATIC_SIZE];
}*xline_ptr;


union real32 {
    float f;
    char byte[4];
};

union real64 {
    double f;
    char byte[8];
};


#define __number_to_byte_8bit(n, flag) \
        (struct xline){ \
            XLINE_TYPE_8BIT | (flag), \
            (((char*)&(n))[0]) \
        }

#define __byte_to_number_8bit(b, type) \
        ((type)(b)->byte[1])



#define __number_to_byte_16bit(n, flag) \
        (struct xline){ \
            XLINE_TYPE_16BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]) \
        }

#define __byte_to_number_16bit(b, type) \
        ((type)(b)->byte[2] << 8 | (type)(b)->byte[1])  



#define __number_to_byte_32bit(n, flag) \
        (struct xline){ \
            XLINE_TYPE_32BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]) \
        }

#define __byte_to_number_32bit(b, type) \
        ((type)(b)->byte[4] << 24 | (type)(b)->byte[3] << 16 | (type)(b)->byte[2] << 8 | (type)(b)->byte[1])



#define __number_to_byte_64bit(n, flag) \
        (struct xline){ \
            XLINE_TYPE_64BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __byte_to_number_64bit(b, type) \
        ( (type)(b)->byte[8] << 56 | (type)(b)->byte[7] << 48 | (type)(b)->byte[6] << 40 | (type)(b)->byte[5] << 32 \
        | (type)(b)->byte[4] << 24 | (type)(b)->byte[3] << 16 | (type)(b)->byte[2] << 8 | (type)(b)->byte[1] )



// #define __byte_to_float_32bit(b) \
//         (((union real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f)

// #define __byte_to_float_64bit(b) \
//         (((union real64) \
//             { \
//                 .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
//                 .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
//             } \
//         ).f)


static inline float __byte_to_float_32bit(xline_ptr p)
{
    union real32 r;
    r.byte[0] = p->byte[1];
    r.byte[1] = p->byte[2];
    r.byte[2] = p->byte[3];
    r.byte[3] = p->byte[4];
    return r.f;
}

static inline float __byte_to_float_64bit(xline_ptr p)
{
    union real64 r;
    r.byte[0] = p->byte[1];
    r.byte[1] = p->byte[2];
    r.byte[2] = p->byte[3];
    r.byte[3] = p->byte[4];
    r.byte[4] = p->byte[5];
    r.byte[5] = p->byte[6];
    r.byte[6] = p->byte[7];
    r.byte[7] = p->byte[8];
    return r.f;
}


#define __n2b8(n)        __number_to_byte_8bit(n, XLINE_NUMBER_TYPE_INTEGER)
#define __n2b16(n)       __number_to_byte_16bit(n, XLINE_NUMBER_TYPE_INTEGER)
#define __n2b32(n)       __number_to_byte_32bit(n, XLINE_NUMBER_TYPE_INTEGER)
#define __n2b64(n)       __number_to_byte_64bit(n, XLINE_NUMBER_TYPE_INTEGER)

#define __b2n8(b)        __byte_to_number_8bit(b, int8_t)
#define __b2n16(b)       __byte_to_number_16bit(b, int16_t)
#define __b2n32(b)       __byte_to_number_32bit(b, int32_t)
#define __b2n64(b)       __byte_to_number_64bit(b, int64_t)

#define __u2b8(n)        __number_to_byte_8bit(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u2b16(n)       __number_to_byte_16bit(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u2b32(n)       __number_to_byte_32bit(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u2b64(n)       __number_to_byte_64bit(n, XLINE_NUMBER_TYPE_NATURAL)

#define __b2u8(b)        __byte_to_number_8bit(b, uint8_t)
#define __b2u16(b)       __byte_to_number_16bit(b, uint16_t)
#define __b2u32(b)       __byte_to_number_32bit(b, uint32_t)
#define __b2u64(b)       __byte_to_number_64bit(b, uint64_t)

#define __f2b32(f)       __number_to_byte_32bit(f, XLINE_NUMBER_TYPE_REAL)
#define __f2b64(f)       __number_to_byte_64bit(f, XLINE_NUMBER_TYPE_REAL)

#define __b2f32(b)       __byte_to_float_32bit(b)
#define __b2f64(b)       __byte_to_float_64bit(b)

#define __bool_to_byte(b)       __number_to_byte_8bit(b, XLINE_NUMBER_TYPE_BOOL)
#define __byte_to_bool(b)       __byte_to_number_8bit(b, uint8_t)

#define __xline_number_8bit(b)      ((b)->byte[0] & XLINE_TYPE_8BIT)
#define __xline_number_16bit(b)     ((b)->byte[0] & XLINE_TYPE_16BIT)
#define __xline_number_32bit(b)     ((b)->byte[0] & XLINE_TYPE_32BIT)
#define __xline_number_64bit(b)     ((b)->byte[0] & XLINE_TYPE_64BIT)

#define __xline_typeif_object(b)        ((b)->byte[0] & XLINE_TYPE_OBJECT)
#define __xline_typeif_number(b)        (!((b)->byte[0] & XLINE_TYPE_OBJECT))
#define __xline_typeof_content(b)       ((b)->byte[0] & XLINE_CONTENT_TYPE_MASK)

#define __xline_obj_typeif_bin(b)       (__xline_typeof_content(b) == XLINE_OBJECT_TYPE_BIN)
#define __xline_obj_typeif_map(b)       (__xline_typeof_content(b) == XLINE_OBJECT_TYPE_MAP)
#define __xline_obj_typeif_text(b)      (__xline_typeof_content(b) == XLINE_OBJECT_TYPE_TEXT)
#define __xline_obj_typeif_list(b)      (__xline_typeof_content(b) == XLINE_OBJECT_TYPE_LIST)

#define __xline_num_typeif_boolean(b)   (__xline_typeof_content(b) == XLINE_NUMBER_TYPE_BOOL)
#define __xline_num_typeif_natural(b)   (__xline_typeof_content(b) == XLINE_NUMBER_TYPE_NATURAL)
#define __xline_num_typeif_integer(b)   (__xline_typeof_content(b) == XLINE_NUMBER_TYPE_INTEGER)
#define __xline_num_typeif_real(b)      (__xline_typeof_content(b) == XLINE_NUMBER_TYPE_REAL)


#define __xline_sizeof_head(b) \
        (uint64_t)( __xline_typeif_object(b) \
        ? 1 + (((b)->byte[0]) & XLINE_NUMBER_SIZE_MASK) \
        : 1 )

#define __xline_sizeof_data(b) \
        (uint64_t)( __xline_typeif_object(b) \
        ? __xline_number_8bit(b) ? __b2u8(b) \
        : __xline_number_16bit(b) ? __b2u16(b) \
        : __xline_number_32bit(b) ? __b2u32(b) \
        : __b2u64(b) : (((b)->byte[0]) & XLINE_NUMBER_SIZE_MASK) )

#define __xline_sizeof(b)        (( __xline_sizeof_head(b) + __xline_sizeof_data(b)))

#define __xline_to_data(b)        (&((b)->byte[0]) + __xline_sizeof_head(b))

#define __xline_sizeif_object(size) \
        ( (size) < 0x100 ? (2 + (size)) \
        : (size) < 0x10000 ? (3 + (size)) \
        : (size) < 0x100000000 ? (5 + (size)) \
        : (9 + (size)) )

#define __xline_free(line) \
        if (line->byte[0] & XLINE_TYPE_OBJECT) { \
            free(line); \
            line = NULL; \
        }


inline uint64_t xline_fill(xline_ptr x, const void *data, uint64_t size, uint8_t flag)
{
    if (size < 0x100){
        *x = __number_to_byte_8bit(size, flag);
        memcpy(&x->byte[2], data, size);
    }else if (size < 0x10000){
        *x = __number_to_byte_16bit(size, flag);
        memcpy(&x->byte[3], data, size);
    }else if (size < 0x100000000){
        *x = __number_to_byte_32bit(size, flag);
        memcpy(&x->byte[5], data, size);
    }else {
        *x = __number_to_byte_64bit(size, flag);
        memcpy(&x->byte[9], data, size);
    }
    return size + __xline_sizeof_head(x);
}

inline xline_ptr xline_from_text(const char *text)
{
    uint64_t len = strlen(text) + 1;
    xline_ptr ptr = (xline_ptr)malloc(__xline_sizeif_object((len)));
    xline_fill(ptr, text, len, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_TEXT);
    *(ptr->byte + (__xline_sizeof_head(ptr) + len - 1)) = '\0';
    return ptr;
}

inline xline_ptr xline_from_object(void *obj, uint64_t size, uint8_t flag)
{
    xline_ptr ptr = (xline_ptr)malloc(__xline_sizeif_object(size));
    xline_fill(ptr, obj, size, flag);
    return ptr;
}


#define XKEY_HEAD_SIZE          1
#define XKEY_DATA_MAX_SIZE      255
#define XKEY_MAX_SIZE           (XKEY_HEAD_SIZE + XKEY_DATA_MAX_SIZE)

typedef struct xkey {
    char byte[1];
}*xkey_ptr;

#define __sizeof_xkey(xk)   (XKEY_HEAD_SIZE + (xk)->byte[0])


typedef struct xline_object {
    uint64_t stride;
    uint64_t pos, len;
    xkey_ptr key;
    xline_ptr val;
    struct xline_object *prev, *next;
    uint8_t *addr, *head;
}*xline_object_ptr;


inline uint64_t xkey_fill(xkey_ptr xkey, const char *str)
{
    uint8_t len = strlen(str);
    if (len > UINT8_MAX - XKEY_HEAD_SIZE){
        len = UINT8_MAX - XKEY_HEAD_SIZE;
    }
    memcpy(xkey->byte + XKEY_HEAD_SIZE, str, len);
    xkey->byte[0] = len;
    return __sizeof_xkey(xkey);
}

inline void xline_clear_object(xline_object_ptr xobj)
{
    if (xobj && xobj->addr){
        free(xobj->addr);
    }
    *xobj = (struct xline_object){0};
}

inline void xline_make_object(xline_object_ptr xobj, uint64_t stride)
{
    xobj->pos = 0;
    xobj->len = xobj->stride = stride;
    xobj->addr = (uint8_t*) malloc(stride);
    xobj->head = xobj->addr + XLINE_STATIC_SIZE;
    xobj->key = xobj->val = NULL;
}

inline void xline_object_append(xline_object_ptr xobj, const char *key, const void *val, uint64_t size, uint8_t flag)
{
    while ((xobj->len - xobj->pos) < (XKEY_MAX_SIZE + XLINE_STATIC_SIZE + size)){
        xobj->len += xobj->stride;
        xobj->addr = (uint8_t *)malloc(xobj->len);
        memcpy(xobj->addr + XLINE_STATIC_SIZE, xobj->head, xobj->pos);
        free((xobj->head - XLINE_STATIC_SIZE));
        xobj->head = xobj->addr + XLINE_STATIC_SIZE;
    }
    xobj->key = (xkey_ptr)(xobj->head + xobj->pos);
    xobj->pos += xkey_fill(xobj->key, key);
    xobj->val = (xline_ptr)(xobj->head + xobj->pos);
    xobj->pos += xline_fill(xobj->val, val, size, flag);
    *((xline_ptr)xobj->addr) = __number_to_byte_64bit(xobj->pos, XLINE_OBJECT_TYPE_MAP);
}

inline void xline_object_add_text(xline_object_ptr xobj, const char *key, const char *text)
{
    xline_object_append(xobj, key, text, strlen(text), XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_TEXT);
}

inline void xline_object_add_map(xline_object_ptr xobj, const char *key, xline_object_ptr xmap)
{
    xline_object_append(xobj, key, xmap->head, xmap->pos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_MAP);
}

inline void xline_object_add_list(xline_object_ptr xobj, const char *key, xline_object_ptr xlist)
{
    xline_object_append(xobj, key, xlist->head, xlist->pos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_LIST);
}

inline void xline_object_add_binary(xline_object_ptr xobj, const char *key, const void *val, uint64_t size)
{
    xline_object_append(xobj, key, val, size, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_BIN);
}

inline void xline_object_append_number(xline_object_ptr xobj, const char *key, struct xline val)
{
    while ((xobj->len - xobj->pos) < (XKEY_MAX_SIZE + XLINE_STATIC_SIZE)){
        xobj->len += xobj->stride;
        xobj->addr = (uint8_t *)malloc(xobj->len);
        memcpy(xobj->addr + XLINE_STATIC_SIZE, xobj->head, xobj->pos);
        free((xobj->head - XLINE_STATIC_SIZE));
        xobj->head = xobj->addr + XLINE_STATIC_SIZE;
    }    
    xobj->key = (xkey_ptr)(xobj->head + xobj->pos);
    xobj->pos += xkey_fill(xobj->key, key);
    xobj->val = (xline_ptr)(xobj->head + xobj->pos);
    *xobj->val = val;
    xobj->pos += __xline_sizeof(xobj->val);
    *((xline_ptr)xobj->addr) = __number_to_byte_64bit(xobj->pos, XLINE_OBJECT_TYPE_MAP);
}

inline void xline_object_add_int8(xline_object_ptr xobj, const char *key, int8_t n8)
{
    xline_object_append_number(xobj, key, __n2b8(n8));
}

inline void xline_object_add_int16(xline_object_ptr xobj, const char *key, int16_t n16)
{
    xline_object_append_number(xobj, key, __n2b16(n16));
}

inline void xline_object_add_int32(xline_object_ptr xobj, const char *key, int32_t n32)
{
    xline_object_append_number(xobj, key, __n2b32(n32));
}

inline void xline_object_add_int64(xline_object_ptr xobj, const char *key, int64_t n64)
{
    xline_object_append_number(xobj, key, __n2b64(n64));
}

inline void xline_object_add_uint8(xline_object_ptr xobj, const char *key, uint8_t u8)
{
    xline_object_append_number(xobj, key, __u2b8(u8));
}

inline void xline_object_add_uint16(xline_object_ptr xobj, const char *key, uint16_t u16)
{
    xline_object_append_number(xobj, key, __u2b16(u16));
}

inline void xline_object_add_uint32(xline_object_ptr xobj, const char *key, uint32_t u32)
{
    xline_object_append_number(xobj, key, __u2b32(u32));
}

inline void xline_object_add_uint64(xline_object_ptr xobj, const char *key, uint64_t u64)
{
    xline_object_append_number(xobj, key, __u2b64(u64));
}

inline void xline_object_add_real32(xline_object_ptr xobj, const char *key, float f32)
{
    xline_object_append_number(xobj, key, __f2b32(f32));
}

inline void xline_object_add_real64(xline_object_ptr xobj, const char *key, double f64)
{
    xline_object_append_number(xobj, key, __f2b64(f64));
}

inline void xline_object_add_ptr(xline_object_ptr xobj, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    xline_object_append_number(xobj, key, __n2b64(n));
}

inline void xline_object_add_bool(xline_object_ptr xobj, const char *key, uint8_t b)
{
    xline_object_append_number(xobj, key, __bool_to_byte(b));
}

inline xline_ptr xline_object_parse(xline_object_ptr xobj, xline_ptr xmap)
{
    xobj->addr = NULL;
    xobj->len = __xline_sizeof_data(xmap);
    xobj->head = (uint8_t*)__xline_to_data(xmap);
    xobj->key = (xkey_ptr)(xobj->head);
    xobj->val = (xline_ptr)(xobj->key->byte + __sizeof_xkey(xobj->key));    
    return xobj->val;
}

inline xline_ptr xline_object_next(xline_object_ptr xobj)
{
    if (xobj->val){
        xobj->pos = ((xobj->val->byte) - (xobj->head) + __xline_sizeof(xobj->val));
        if (xobj->pos < xobj->len){
            xobj->key = (xkey_ptr)(xobj->head + xobj->pos);
            xobj->val = (xline_ptr)(xobj->key->byte + __sizeof_xkey(xobj->key));
            return xobj->val;
        }
    }
    return NULL;
}

inline xline_ptr xline_object_find(xline_object_ptr xobj, const char *key)
{
    xobj->pos = 0;
    while (xobj->pos < xobj->len) {
        xobj->key = (xkey_ptr)(xobj->head + xobj->pos);
        xobj->pos += __sizeof_xkey(xobj->key);
        xobj->val = (xline_ptr)(xobj->key->byte + __sizeof_xkey(xobj->key));
        if (strlen(key) == xobj->key->byte[0]
            && memcmp(key, &xobj->key->byte[1], xobj->key->byte[0]) == 0){
            return xobj->val;
        }
        xobj->pos += __xline_sizeof(xobj->val);
    }
    return NULL;
}

inline xline_ptr xline_object_after(xline_object_ptr xobj, const char *key)
{
    if (xobj->val){

        xline_ptr start_val = xobj->val;
        xobj->pos = ((start_val->byte) - (xobj->head) + __xline_sizeof(xobj->val));

        while (xobj->pos < xobj->len) {
            xobj->key = (xline_ptr)(xobj->head + xobj->pos);
            xobj->pos += __sizeof_xkey(xobj->key);
            xobj->val = (xline_ptr)(xobj->key->byte + __xline_sizeof(xobj->key));
            if (strlen(key) == xobj->key->byte[0]
                && memcmp(key, &xobj->key->byte[1], xobj->key->byte[0]) == 0){
                return xobj->val;
            }
            xobj->pos += __xline_sizeof(xobj->val);
        }

        xobj->pos = 0;

        do {
            xobj->key = (xkey_ptr)(xobj->head + xobj->pos);
            xobj->pos += __sizeof_xkey(xobj->key);
            xobj->val = (xline_ptr)(xobj->key->byte + __sizeof_xkey(xobj->key));
            xobj->pos += __xline_sizeof(xobj->val);
            if (strlen(key) == xobj->key->byte[0]
                && memcmp(key, &xobj->key->byte[1], xobj->key->byte[0]) == 0){
                return xobj->val;
            }
        } while (xobj->val != start_val);
    }

    return xline_object_find(xobj, key);
}

inline uint8_t xline_object_find_bool(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __byte_to_bool(val);
    }
    return 0;
}

inline int8_t xline_object_find_int8(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2n8(val);
    }
    return 0;
}

inline int16_t xline_object_find_int16(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2n16(val);
    }
    return 0;
}

inline int32_t xline_object_find_int32(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2n32(val);
    }
    return 0;
}

inline int64_t xline_object_find_int64(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2n64(val);
    }
    return 0;
}

inline uint8_t xline_object_find_uint8(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2u8(val);
    }
    return 0;
}

inline uint16_t xline_object_find_uint16(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2u16(val);
    }
    return 0;
}

inline uint32_t xline_object_find_uint32(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2u32(val);
    }
    return 0;
}

inline uint64_t xline_object_find_uint64(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2u64(val);
    }
    return 0;
}

inline float xline_object_find_real32(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2f32(val);
    }
    return 0.0f;
}

inline double xline_object_find_real64(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return __b2f64(val);
    }
    return 0.0f;
}

inline void* xline_object_find_ptr(xline_object_ptr xobj, const char *key)
{
    xline_ptr val = xline_object_after(xobj, key);
    if (val){
        return (void *)(__b2n64(val));
    }
    return NULL;
}

inline void xline_object_list_append(xline_object_ptr xobj, xline_ptr x)
{
    uint64_t len = __xline_sizeof(x);
    while ((xobj->len - xobj->pos) < len){
        xobj->len += xobj->stride;
        xobj->addr = (uint8_t *)malloc(xobj->len);
        memcpy(xobj->addr + XLINE_STATIC_SIZE, xobj->head, xobj->pos);
        free((xobj->head - XLINE_STATIC_SIZE));
        xobj->head = xobj->addr + XLINE_STATIC_SIZE;
    }
    memcpy(xobj->head + xobj->pos, x->byte, len);
    xobj->pos += len;
    *((xline_ptr)xobj->addr) = __number_to_byte_64bit(xobj->pos, XLINE_OBJECT_TYPE_LIST);
}

inline xline_ptr xline_object_list_parse(xline_object_ptr xobj, xline_ptr xlist)
{
    xobj->addr = NULL;
    xobj->key = NULL;
    xobj->len = __xline_sizeof_data(xlist);
    xobj->head = (uint8_t*)__xline_to_data(xlist);
    xobj->val = (xline_ptr)xobj->head;
    xline_ptr x = (xline_ptr)(xobj->head);
    xobj->pos = __xline_sizeof(x);
    return x;
}

inline xline_ptr xline_object_list_next(xline_object_ptr xobj)
{
    if (xobj->len - xobj->pos > 0){
        xline_ptr x = (xline_ptr)(xobj->head + xobj->pos);
        xobj->pos += __xline_sizeof(x);
        return x;
    }
    return NULL;
}