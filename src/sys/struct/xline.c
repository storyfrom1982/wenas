#include "xmalloc.h"

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

struct n8 {
    uint8_t byte[2];
};

struct n16 {
    uint8_t byte[3];
};

struct n32 {
    uint8_t byte[5];
};

struct n64 {
    uint8_t byte[9];
};

union real32 {
    float f;
    char byte[4];
};

union real64 {
    double f;
    char byte[8];
};


#define __n8_to_byte(n, flag) \
        (struct n8){ \
            (((char*)&(n))[0]) \
            XLINE_TYPE_8BIT | (flag), \
        }

#define __byte_to_n8(b, type) \
        ((type)(b)->byte[0])



#define __n16_to_byte(n, flag) \
        (struct n16){ \
            (((char*)&(n))[0]), (((char*)&(n))[1]) \
            XLINE_TYPE_16BIT | (flag), \
        }

#define __byte_to_n16(b, type) \
        ((type)(b)->byte[1] << 8 | (type)(b)->byte[0])  



#define __n32_to_byte(n, flag) \
        (struct n32){ \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]) \
            XLINE_TYPE_32BIT | (flag), \
        }

#define __byte_to_n32(b, type) \
        ((type)(b)->byte[3] << 24 | (type)(b)->byte[2] << 16 | (type)(b)->byte[1] << 8 | (type)(b)->byte[0])



#define __n64_to_byte(n, flag) \
        (struct n64){ \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
            XLINE_TYPE_64BIT | (flag), \
        }

#define __byte_to_n64(b, type) \
        ( (type)(b)->byte[7] << 56 | (type)(b)->byte[6] << 48 | (type)(b)->byte[5] << 40 | (type)(b)->byte[4] << 32 \
        | (type)(b)->byte[3] << 24 | (type)(b)->byte[2] << 16 | (type)(b)->byte[1] << 8 | (type)(b)->byte[0] )



// #define __byte_to_float_32bit(b) \
//         (((union real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f)

// #define __byte_to_float_64bit(b) \
//         (((union real64) \
//             { \
//                 .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
//                 .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
//             } \
//         ).f)


static inline float __byte_to_f32(struct n32 *p)
{
    union real32 r;
    r.byte[0] = p->byte[0];
    r.byte[1] = p->byte[1];
    r.byte[2] = p->byte[2];
    r.byte[3] = p->byte[3];
    return r.f;
}

static inline double __byte_to_f64(struct n64 *p)
{
    union real64 r;
    r.byte[0] = p->byte[0];
    r.byte[1] = p->byte[1];
    r.byte[2] = p->byte[2];
    r.byte[3] = p->byte[3];
    r.byte[4] = p->byte[4];
    r.byte[5] = p->byte[5];
    r.byte[6] = p->byte[6];
    r.byte[7] = p->byte[7];
    return r.f;
}


#define __i8_to_byte(n)        __n8_to_byte(n, XLINE_NUMBER_TYPE_INTEGER)
#define __i16_to_byte(n)       __n16_to_byte(n, XLINE_NUMBER_TYPE_INTEGER)
#define __i32_to_byte(n)       __n32_to_byte(n, XLINE_NUMBER_TYPE_INTEGER)
#define __i64_to_byte(n)       __n64_to_byte(n, XLINE_NUMBER_TYPE_INTEGER)

#define __byte_to_i8(b)        __byte_to_n8(b, int8_t)
#define __byte_to_i16(b)       __byte_to_n16(b, int16_t)
#define __byte_to_i32(b)       __byte_to_n32(b, int32_t)
#define __byte_to_i64(b)       __byte_to_n64(b, int64_t)

#define __u8_to_byte(n)        __n8_to_byte(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u16_to_byte(n)       __n16_to_byte(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u32_to_byte(n)       __n32_to_byte(n, XLINE_NUMBER_TYPE_NATURAL)
#define __u64_to_byte(n)       __n64_to_byte(n, XLINE_NUMBER_TYPE_NATURAL)

#define __byte_to_u8(b)        __byte_to_n8(b, uint8_t)
#define __byte_to_u16(b)       __byte_to_n16(b, uint16_t)
#define __byte_to_u32(b)       __byte_to_n32(b, uint32_t)
#define __byte_to_u64(b)       __byte_to_n64(b, uint64_t)

#define __f32_to_byte(f)       __n32_to_byte(f, XLINE_NUMBER_TYPE_REAL)
#define __f64_to_byte(f)       __n64_to_byte(f, XLINE_NUMBER_TYPE_REAL)

#define __byte_to_f32(b)       __byte_to_f32(b)
#define __byte_to_f64(b)       __byte_to_f64(b)

#define __bool_to_byte(b)       __n8_to_byte(b, XLINE_NUMBER_TYPE_BOOL)
#define __byte_to_bool(b)       __byte_to_n8(b, uint8_t)


static inline uint64_t xline_fill_val(uint8_t *xl, const void *data, uint64_t size, uint8_t flag)
{
    mcopy(xl, data, size);
    if (size < 0x100){
        *((struct n8*)(xl + size)) = __n8_to_byte(size, flag);
        size += 2;
    }else if (size < 0x10000){
        *((struct n16*)(xl + size)) = __n16_to_byte(size, flag);
        size += 3;
    }else if (size < 0x100000000){
        *((struct n32*)(xl + size)) = __n32_to_byte(size, flag);
        size += 5;
    }else {
        *((struct n64*)(xl + size)) = __n64_to_byte(size, flag);
        size += 9;
    }
    return size;
}


#define XKEY_HEAD_SIZE          1
#define XKEY_DATA_MAX_SIZE      255
#define XKEY_MAX_SIZE           (XKEY_HEAD_SIZE + XKEY_DATA_MAX_SIZE)

typedef struct xkey {
    char byte[1];
}*xkey_ptr;

typedef struct xpair {
    uint8_t type;
    uint8_t keylen;
    char *key;
    size_t vallen;
    union
    {
        void *v;
        size_t u;
        double f;
        int64_t i;
    };
}*xpair_ptr;

typedef struct xmaker {
    uint64_t wpos, rpos, len;
    struct xpair pair;
    uint8_t *addr, *head;
}*xmaker_ptr;

typedef struct xholder {
    xmaker_ptr maker;
    uint64_t wpos, rpos, len;
    xkey_ptr key;
    xline_ptr val;
    xline_ptr xline;
    uint8_t *addr, *head;
}*xholder_ptr;


static inline uint64_t xline_fill_key(uint8_t *xl, const char *str, size_t len)
{
    // key 的字符串的最大长度不能超过 255
    // 因为 xkey 用一个字节存储字符串长度，长度一旦大于 255 就会溢出 uint8 类型
    if (len > UINT8_MAX){
        len = UINT8_MAX;
    }
    mcopy(xl, str, len);
    xl[len] = len;
    return len + 1;
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

static inline struct xholder xline_maker_hold(xmaker_ptr maker, uint8_t *ptr, uint64_t len)
{
    struct xholder holder;
    holder.maker = maker;
    holder.wpos = holder.rpos = 0;
    holder.len = len;
    holder.head = ptr;
    holder.key = NULL;
    holder.val = NULL;
}

static inline void xline_maker_update(xmaker_ptr parent, xmaker_ptr child)
{
    parent->wpos += __xline_sizeof(child->xline);
    *(parent->xline) = __number_to_byte_64bit(parent->wpos, __xline_typeof(child->xline) | __xline_subclass_typeof(child->xline));
}

static inline void xline_append_object(xmaker_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    while ((maker->len - maker->wpos) < (XKEY_HEAD_SIZE + keylen + XLINE_STATIC_SIZE + size)){
        __xcheck(maker->addr != NULL);
        maker->len += maker->len;
        maker->addr = (uint8_t *)malloc(maker->len);
        mcopy(maker->addr, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->addr;
    }
    maker->wpos += xline_fill_val(maker->head + maker->wpos, val, size, flag);
    maker->wpos += xline_fill_key(maker->head + maker->wpos, key, keylen);
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

static inline void xline_append_number(xmaker_ptr maker, const char *key, size_t keylen, const uint8_t *val, uint64_t size)
{
    // 最小长度 = 根 xline 的头 (XLINE_STATIC_SIZE) + key 长度 (keylen + XKEY_HEAD_SIZE) + value 长度 (XLINE_STATIC_SIZE)
    while ((int64_t)(maker->len - maker->wpos) < ((keylen + XKEY_HEAD_SIZE) + size)){
        __xcheck(maker->addr != NULL);
        maker->len += maker->len;
        maker->addr = (uint8_t *)malloc(maker->len);
        if (maker->wpos > 0){
            mcopy(maker->addr, maker->head, maker->wpos);
        }
        free(maker->head);
        maker->head = maker->addr;
    }

    mcopy(maker->head + maker->wpos, val, size);
    maker->wpos += size;
    maker->wpos += xline_fill_key(maker->head + maker->wpos, key, keylen);
}

static inline void xline_add_int8(xmaker_ptr maker, const char *key, int8_t n8)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__i8_to_byte(i8), 2);
}

static inline void xline_add_int16(xmaker_ptr maker, const char *key, int16_t n16)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__i16_to_byte(i16), 3);
}

static inline void xline_add_int32(xmaker_ptr maker, const char *key, int32_t n32)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__i32_to_byte(i32), 5);
}

static inline void xline_add_int64(xmaker_ptr maker, const char *key, int64_t n64)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__i64_to_byte(i64), 9);
}

static inline void xline_add_uint8(xmaker_ptr maker, const char *key, uint8_t u8)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__u8_to_byte(u8), 2);
}

static inline void xline_add_uint16(xmaker_ptr maker, const char *key, uint16_t u16)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__u16_to_byte(u16), 3);
}

static inline void xline_add_uint32(xmaker_ptr maker, const char *key, uint32_t u32)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__u32_to_byte(u32), 5);
}

static inline void xline_add_uint64(xmaker_ptr maker, const char *key, uint64_t u64)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__u64_to_byte(u64), 9);
}

static inline void xline_add_real32(xmaker_ptr maker, const char *key, float f32)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__f32_to_byte(f32), 5);
}

static inline void xline_add_real64(xmaker_ptr maker, const char *key, double f64)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__f64_to_byte(f64), 9);
}

static inline void xline_add_ptr(xmaker_ptr maker, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    xline_append_number(maker, key, slength(key), (uint8_t*)&__i64_to_byte(n), 9);
}

static inline void xline_add_bool(xmaker_ptr maker, const char *key, uint8_t b)
{
    xline_append_number(maker, key, slength(key), (uint8_t*)&__bool_to_byte(b), 2);
}

static inline struct xmaker xline_parse(uint8_t *data, size_t size)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    struct xmaker maker = {0};
    maker.wpos = size;
    maker.rpos = maker.wpos - 1;
    maker.head = data;
    return maker;
}

static inline struct xpair_ptr xline_next(xmaker_ptr maker)
{
    if (maker->rpos > 0){
        maker->pair.keylen = *(maker->head + maker->rpos);
        maker->rpos -= maker->pair.keylen;
        kv->key = (char*)(maker->head + maker->rpos);
        maker->rpos --;
        maker->pair.type = *(maker->head + maker->rpos);
        maker->rpos --;        
        kv->vallen = *((size_t*)(maker->head + maker->rpos));
        maker->rpos -= maker->pair.vallen;
        if (maker->pair.byte & XLINE_TYPE_OBJECT){
            kv->v = maker->head + maker->rpos;
        }else {
            kv->u = *((size_t*)(maker->head + maker->rpos));
        }
        maker->rpos -= maker->vallen;
        return &maker->pair;
    }
    return NULL;
}

static inline struct xpair_ptr xline_find(xmaker_ptr maker, const char *key)
{
    size_t pos = maker->rpos;
    while (maker->rpos > 0) {
        maker->pair.keylen = *(maker->head + maker->rpos);
        maker->rpos -= maker->pair.keylen;
        kv->key = (char*)(maker->head + maker->rpos);
        maker->rpos --;
        maker->pair.type = *(maker->head + maker->rpos);
        maker->rpos --;        
        kv->vallen = *((size_t*)(maker->head + maker->rpos));
        maker->rpos -= maker->pair.vallen;
        if (maker->pair.byte & XLINE_TYPE_OBJECT){
            kv->v = maker->head + maker->rpos;
        }else {
            kv->u = *((size_t*)(maker->head + maker->rpos));
        }
        maker->rpos -= maker->vallen;
        if (slength(key) == maker->pair.keylen
            && mcompare(key, maker->pair.keylen, kv->key) == 0){
            return &maker->pair;
        }
    }
    if (pos != maker->wpos - 1){
        while (maker->rpos > pos) {
            maker->rpos = maker->wpos - 1;
            maker->pair.keylen = *(maker->head + maker->rpos);
            maker->rpos -= maker->pair.keylen;
            kv->key = (char*)(maker->head + maker->rpos);
            maker->rpos --;
            maker->pair.type = *(maker->head + maker->rpos);
            maker->rpos --;        
            kv->vallen = *((size_t*)(maker->head + maker->rpos));
            maker->rpos -= maker->pair.vallen;
            if (maker->pair.byte & XLINE_TYPE_OBJECT){
                kv->v = maker->head + maker->rpos;
            }else {
                kv->u = *((size_t*)(maker->head + maker->rpos));
            }
            maker->rpos -= maker->vallen;
            if (slength(key) == maker->pair.keylen
                && mcompare(key, maker->pair.keylen, kv->key) == 0){
                return &maker->pair;
            }
        }
    }
    return NULL;
}

static inline bool xline_find_uint(xmaker_ptr maker, const char *key)
{
    xpair_ptr val = xline_find(maker, key);
    if (val && val->type & XLINE_OBJECT_TYPE_MASK == XLINE_NUMBER_TYPE_NATURAL){
        return (bool)val->u;
    }
    return (uint64_t)-1;
}

static inline int64_t xline_find_int64(xmaker_ptr maker, const char *key)
{
    xpair_ptr val = xline_find(maker, key);
    if (val && val->type & XLINE_OBJECT_TYPE_MASK == XLINE_NUMBER_TYPE_INTEGER){
        return (bool)val->i;
    }
    return (int64_t)-1;
}

static inline int64_t xline_find_real64(xmaker_ptr maker, const char *key)
{
    xpair_ptr val = xline_find(maker, key);
    if (val && val->type & XLINE_OBJECT_TYPE_MASK == XLINE_NUMBER_TYPE_REAL){
        return (bool)val->f;
    }
    return (double)-1;
}

static inline int64_t xline_find_ptr(xmaker_ptr maker, const char *key)
{
    xpair_ptr val = xline_find(maker, key);
    if (val && val->type & XLINE_OBJECT_TYPE_MASK == XLINE_NUMBER_TYPE_NATURAL){
        return (bool)val->v;
    }
    return NULL;
}

static inline void xline_list_append(xmaker_ptr maker, xline_ptr x)
{
    uint64_t len = __xline_sizeof(x);
    while ((maker->len - (maker->wpos + XLINE_STATIC_SIZE)) < len){
        __xcheck(maker->addr != NULL);
        // if (maker->addr == NULL){
        //     exit(0);
        // }
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