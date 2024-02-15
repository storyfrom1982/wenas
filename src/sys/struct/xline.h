#ifndef __XLINE_H__
#define __XLINE_H__

#include <ex/malloc.h>

// #define XLINE_NUMBER_SIZE_MASK     0x1f //00011111 开启 XLINE_TYPE_4BIT 字节（ 保留低位 5bit，64bit 是 16 个字节，16 需要占用 5bit ）
#define XLINE_NUMBER_SIZE_MASK     0x0f //00011111 保留低位 4bit，获取 xline 为 number 类型时，数据的长度
#define XLINE_OBJECT_TYPE_MASK     0xc0 //11000000 保留高位 2bit，获取 xline 的数据类型

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

#define __xline_typeof(b)               ((b)->byte[0] & XLINE_TYPE_OBJECT)
#define __xline_typeif_object(b)        (__xline_typeof(b) != 0)
#define __xline_typeif_number(b)        (__xline_typeof(b) == 0)

#define __xline_subclass_typeof(b)      ((b)->byte[0] & XLINE_OBJECT_TYPE_MASK)
#define __xline_obj_typeif_bin(b)       (__xline_subclass_typeof(b) == XLINE_OBJECT_TYPE_BIN)
#define __xline_obj_typeif_map(b)       (__xline_subclass_typeof(b) == XLINE_OBJECT_TYPE_MAP)
#define __xline_obj_typeif_text(b)      (__xline_subclass_typeof(b) == XLINE_OBJECT_TYPE_TEXT)
#define __xline_obj_typeif_list(b)      (__xline_subclass_typeof(b) == XLINE_OBJECT_TYPE_LIST)

#define __xline_num_typeif_boolean(b)   (__xline_subclass_typeof(b) == XLINE_NUMBER_TYPE_BOOL)
#define __xline_num_typeif_natural(b)   (__xline_subclass_typeof(b) == XLINE_NUMBER_TYPE_NATURAL)
#define __xline_num_typeif_integer(b)   (__xline_subclass_typeof(b) == XLINE_NUMBER_TYPE_INTEGER)
#define __xline_num_typeif_real(b)      (__xline_subclass_typeof(b) == XLINE_NUMBER_TYPE_REAL)


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


static inline uint64_t xline_fill(xline_ptr x, const void *data, uint64_t size, uint8_t flag)
{
    if (size < 0x100){
        *x = __number_to_byte_8bit(size, flag);
        mcopy(&x->byte[2], data, size);
    }else if (size < 0x10000){
        *x = __number_to_byte_16bit(size, flag);
        mcopy(&x->byte[3], data, size);
    }else if (size < 0x100000000){
        *x = __number_to_byte_32bit(size, flag);
        mcopy(&x->byte[5], data, size);
    }else {
        *x = __number_to_byte_64bit(size, flag);
        mcopy(&x->byte[9], data, size);
    }
    return size + __xline_sizeof_head(x);
}

static inline xline_ptr xline_from_text(const char *text)
{
    uint64_t len = slength(text) + 1;
    xline_ptr ptr = (xline_ptr)malloc(__xline_sizeif_object((len)));
    xline_fill(ptr, text, len, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_TEXT);
    *(ptr->byte + (__xline_sizeof_head(ptr) + len - 1)) = '\0';
    return ptr;
}

static inline xline_ptr xline_from_object(void *obj, uint64_t size, uint8_t flag)
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

#define __xkey_sizeof(xk)       (XKEY_HEAD_SIZE + (xk)->byte[0])


typedef struct xmaker {
    uint64_t wpos, rpos, len;
    xkey_ptr key;
    xline_ptr val;
    xline_ptr xline;
    uint8_t *addr, *head;
}*xmaker_ptr;


static inline uint64_t xkey_fill(xkey_ptr xkey, const char *str, size_t len)
{
    // key 的字符串的最大长度不能超过 255
    // 因为 xkey 用一个字节存储字符串长度，长度一旦大于 255 就会溢出 uint8 类型
    if (len > UINT8_MAX){
        len = UINT8_MAX;
    }
    mcopy(xkey->byte + XKEY_HEAD_SIZE, str, len);
    xkey->byte[0] = len;
    return __xkey_sizeof(xkey);
}

static inline void xline_maker_clear(xmaker_ptr maker)
{
    if (maker && maker->addr){
        __xlogd("xline_maker_clear ============================== xmaker.addr: 0x%X\n", maker->addr);
        free(maker->addr);
    }
    *maker = (struct xmaker){0};
}

static inline void xline_maker_setup(xmaker_ptr maker, uint8_t *ptr, uint64_t len)
{
    maker->wpos = maker->rpos = 0;
    if (ptr){
        maker->len = len;
        maker->head = ptr + XLINE_STATIC_SIZE;
        maker->xline = (xline_ptr)ptr;
    }else {
        maker->len = len + XLINE_STATIC_SIZE;
        maker->addr = (uint8_t*) malloc(maker->len);
        maker->head = maker->addr + XLINE_STATIC_SIZE;
        maker->xline = (xline_ptr)maker->addr;
    }
    maker->key = NULL;
    maker->val = NULL;
}

static inline void xline_maker_reset(xmaker_ptr maker)
{
    maker->wpos = maker->rpos = 0;
    maker->key = NULL;
    maker->val = NULL;
}

static inline void xline_maker_update(xmaker_ptr parent, xmaker_ptr child)
{
    parent->wpos += __xline_sizeof(child->xline);
    *(parent->xline) = __number_to_byte_64bit(parent->wpos, __xline_typeof(child->xline) | __xline_subclass_typeof(child->xline));
}

static inline void xline_append_object(xmaker_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    while ((maker->len - (XLINE_STATIC_SIZE + maker->wpos)) < (XKEY_HEAD_SIZE + keylen + XLINE_STATIC_SIZE + size)){
        if (maker->addr == NULL){
            __xloge("maker->addr == NULL\n");
            exit(0);
        }
        maker->len += maker->len;
        maker->addr = (uint8_t *)malloc(maker->len);
        mcopy(maker->addr + XLINE_STATIC_SIZE, maker->head, maker->wpos);
        free((maker->head - XLINE_STATIC_SIZE));
        maker->head = maker->addr + XLINE_STATIC_SIZE;
        maker->xline = (xline_ptr)maker->addr;
    }
    maker->key = (xkey_ptr)(maker->head + maker->wpos);
    maker->wpos += xkey_fill(maker->key, key, slength(key));
    maker->val = (xline_ptr)(maker->head + maker->wpos);
    maker->wpos += xline_fill(maker->val, val, size, flag);
    *(maker->xline) = __number_to_byte_64bit(maker->wpos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_MAP);
}

static inline void xline_add_text(xmaker_ptr maker, const char *key, const char *text, uint64_t size)
{
    xline_append_object(maker, key, slength(key), text, size, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_TEXT);
}

static inline void xline_add_map(xmaker_ptr maker, const char *key, xmaker_ptr xmap)
{
    xline_append_object(maker, key, slength(key), xmap->head, xmap->wpos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_MAP);
}

static inline void xline_add_list(xmaker_ptr maker, const char *key, xmaker_ptr xlist)
{
    xline_append_object(maker, key, slength(key), xlist->head, xlist->wpos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_LIST);
}

static inline void xline_add_binary(xmaker_ptr maker, const char *key, const void *val, uint64_t size)
{
    xline_append_object(maker, key, slength(key), val, size, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_BIN);
}

static inline void xline_append_number(xmaker_ptr maker, const char *key, size_t keylen, struct xline val)
{
    // 最小长度 = 根 xline 的头 (XLINE_STATIC_SIZE) + key 长度 (keylen + XKEY_HEAD_SIZE) + value 长度 (XLINE_STATIC_SIZE)
    while ((int64_t)(maker->len - (XLINE_STATIC_SIZE + maker->wpos)) < ((keylen + XKEY_HEAD_SIZE) + XLINE_STATIC_SIZE)){
        if (maker->addr == NULL){
            __xloge("maker->addr == NULL\n");
            exit(0);
        }
        maker->len += maker->len;
        maker->addr = (uint8_t *)malloc(maker->len);
        if (maker->wpos > 0){
            mcopy(maker->addr + XLINE_STATIC_SIZE, maker->head, maker->wpos);
        }
        free((maker->head - XLINE_STATIC_SIZE));
        maker->head = maker->addr + XLINE_STATIC_SIZE;
        maker->xline = (xline_ptr)maker->addr;
    }
    maker->key = (xkey_ptr)(maker->head + maker->wpos);
    maker->wpos += xkey_fill(maker->key, key, keylen);
    maker->val = (xline_ptr)(maker->head + maker->wpos);
    *(maker->val) = val;
    maker->wpos += __xline_sizeof(maker->val);
    *(maker->xline) = __number_to_byte_64bit(maker->wpos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_MAP);
}

static inline void xline_add_int8(xmaker_ptr maker, const char *key, int8_t n8)
{
    xline_append_number(maker, key, slength(key), __n2b8(n8));
}

static inline void xline_add_int16(xmaker_ptr maker, const char *key, int16_t n16)
{
    xline_append_number(maker, key, slength(key), __n2b16(n16));
}

static inline void xline_add_int32(xmaker_ptr maker, const char *key, int32_t n32)
{
    xline_append_number(maker, key, slength(key), __n2b32(n32));
}

static inline void xline_add_int64(xmaker_ptr maker, const char *key, int64_t n64)
{
    xline_append_number(maker, key, slength(key), __n2b64(n64));
}

static inline void xline_add_uint8(xmaker_ptr maker, const char *key, uint8_t u8)
{
    xline_append_number(maker, key, slength(key), __u2b8(u8));
}

static inline void xline_add_uint16(xmaker_ptr maker, const char *key, uint16_t u16)
{
    xline_append_number(maker, key, slength(key), __u2b16(u16));
}

static inline void xline_add_uint32(xmaker_ptr maker, const char *key, uint32_t u32)
{
    xline_append_number(maker, key, slength(key), __u2b32(u32));
}

static inline void xline_add_uint64(xmaker_ptr maker, const char *key, uint64_t u64)
{
    xline_append_number(maker, key, slength(key), __u2b64(u64));
}

static inline void xline_add_real32(xmaker_ptr maker, const char *key, float f32)
{
    xline_append_number(maker, key, slength(key), __f2b32(f32));
}

static inline void xline_add_real64(xmaker_ptr maker, const char *key, double f64)
{
    xline_append_number(maker, key, slength(key), __f2b64(f64));
}

static inline void xline_add_ptr(xmaker_ptr maker, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    xline_append_number(maker, key, slength(key), __n2b64(n));
}

static inline void xline_add_bool(xmaker_ptr maker, const char *key, uint8_t b)
{
    xline_append_number(maker, key, slength(key), __bool_to_byte(b));
}

static inline struct xmaker xline_parse(xline_ptr xmap)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    struct xmaker maker = {0};    
    maker.wpos = __xline_sizeof_data(xmap);
    maker.head = (uint8_t*)__xline_to_data(xmap);
    return maker;
}

static inline xline_ptr xline_next(xmaker_ptr maker)
{
    if (maker->rpos < maker->wpos){
        maker->key = (xkey_ptr)(maker->head + maker->rpos);
        maker->rpos += __xkey_sizeof(maker->key);
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __xline_sizeof(maker->val);
        return maker->val;
    }
    return NULL;
}

static inline xline_ptr xline_find(xmaker_ptr maker, const char *key)
{
    maker->rpos = 0;
    while (maker->rpos < maker->wpos) {
        maker->key = (xkey_ptr)(maker->head + maker->rpos);
        maker->rpos += __xkey_sizeof(maker->key);
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        // printf("get key=%s xkey=%s\n", key, &xobj->key[1]);
        if (slength(key) == maker->key->byte[0]
            && mcompare(key, &maker->key->byte[1], maker->key->byte[0]) == 0){
            return maker->val;
        }
        maker->rpos += __xline_sizeof(maker->val);
    }
    return NULL;
}

static inline xline_ptr xline_after(xmaker_ptr maker, const char *key)
{
    if (maker->val){

        xline_ptr start_val = maker->val;
        maker->rpos = ((start_val->byte) - (maker->head) + __xline_sizeof(maker->val));

        while (maker->rpos < maker->wpos) {
            maker->key = (xkey_ptr)(maker->head + maker->rpos);
            maker->rpos += __xkey_sizeof(maker->key);
            maker->val = (xline_ptr)(maker->head + maker->rpos);
            if (slength(key) == maker->key->byte[0]
                && mcompare(key, &maker->key->byte[1], maker->key->byte[0]) == 0){
                return maker->val;
            }
            maker->rpos += __xline_sizeof(maker->val);
        }

        maker->rpos = 0;

        do {
            maker->key = (xkey_ptr)(maker->head + maker->rpos);
            maker->rpos += __xkey_sizeof(maker->key);
            maker->val = (xline_ptr)(maker->head + maker->rpos);
            maker->rpos += __xline_sizeof(maker->val);
            if (slength(key) == maker->key->byte[0]
                && mcompare(key, &maker->key->byte[1], maker->key->byte[0]) == 0){
                return maker->val;
            }
        } while (maker->val != start_val);
    }

    return xline_find(maker, key);
}

static inline uint8_t xline_find_bool(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __byte_to_bool(val);
    }
    return 0;
}

static inline int8_t xline_find_int8(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2n8(val);
    }
    return 0;
}

static inline int16_t xline_find_int16(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2n16(val);
    }
    return 0;
}

static inline int32_t xline_find_int32(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2n32(val);
    }
    return 0;
}

static inline int64_t xline_find_int64(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2n64(val);
    }
    return 0;
}

static inline uint8_t xline_find_uint8(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2u8(val);
    }
    return 0;
}

static inline uint16_t xline_find_uint16(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2u16(val);
    }
    return 0;
}

static inline uint32_t xline_find_uint32(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2u32(val);
    }
    return 0;
}

static inline uint64_t xline_find_uint64(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2u64(val);
    }
    return 0;
}

static inline float xline_find_real32(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2f32(val);
    }
    return 0.0f;
}

static inline double xline_find_real64(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return __b2f64(val);
    }
    return 0.0f;
}

static inline void* xline_find_ptr(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_after(maker, key);
    if (val){
        return (void *)(__b2n64(val));
    }
    return NULL;
}

static inline void xline_list_append(xmaker_ptr maker, xline_ptr x)
{
    uint64_t len = __xline_sizeof(x);
    while ((maker->len - (maker->wpos + XLINE_STATIC_SIZE)) < len){
        if (maker->addr == NULL){
            exit(0);
        }
        maker->len *= 2;
        maker->addr = (uint8_t *)malloc(maker->len);
        mcopy(maker->addr + XLINE_STATIC_SIZE, maker->head, maker->wpos);
        free((maker->head - XLINE_STATIC_SIZE));
        maker->head = maker->addr + XLINE_STATIC_SIZE;
        maker->xline = (xline_ptr)maker->addr;
    }
    mcopy(maker->head + maker->wpos, x->byte, len);
    maker->wpos += len;
    *(maker->xline) = __number_to_byte_64bit(maker->wpos, XLINE_TYPE_OBJECT | XLINE_OBJECT_TYPE_LIST);
}

static inline struct xmaker xline_list_parse(xline_ptr xlist)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    struct xmaker maker = {0};
    // maker.addr = NULL;
    // maker.key = NULL;
    // maker.len = 0;
    // maker.rpos = 0;
    maker.head = (uint8_t*)__xline_to_data(xlist);
    maker.val = (xline_ptr)maker.head;
    // 读取 xlist 的长度，然后设置 wpos
    maker.wpos = __xline_sizeof_data(xlist);
    return maker;
}

static inline xline_ptr xline_list_next(xmaker_ptr maker)
{
    if (maker->wpos - maker->rpos > 0){
        xline_ptr x = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __xline_sizeof(x);
        return x;
    }
    return NULL;
}

#endif //__XLINE_H__