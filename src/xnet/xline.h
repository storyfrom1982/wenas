#ifndef __XLINE_H__
#define __XLINE_H__

#include "xmalloc.h"

enum {
    XLINE_TYPE_INT = 0x01, //整数
    XLINE_TYPE_UINT = 0x02, //自然数
    XLINE_TYPE_FLOAT = 0x04, //实数
    XLINE_TYPE_STR = 0x08, //字符串
    XLINE_TYPE_BIN = 0x10, //二进制数据
    XLINE_TYPE_LIST = 0x20, //列表
    XLINE_TYPE_TREE = 0x40 //树
};

//xbyte 静态分配的最大长度（ 64bit 数的长度为 8 字节，加 1 字节头部标志位 ）
#define XLINE_HEAD_SIZE      9


typedef struct xl {
    uint8_t h[XLINE_HEAD_SIZE];
}xl_t;

typedef xl_t* xl_ptr;

union float64 {
    double f;
    char b[8];
};

#ifdef __LITTLE_ENDIAN__

#define __n2b(n, type) \
        (struct xl){ \
            type, \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __i2b(n) __n2b(n, XLINE_TYPE_INT)
#define __u2b(n) __n2b(n, XLINE_TYPE_UINT)
#define __f2b(n) __n2b(n, XLINE_TYPE_FLOAT)

#define __o2b(n, type) \
        (struct xl){ \
            (type), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __b2n(b, type) \
        ( (type)(b)->h[8] << 56 | (type)(b)->h[7] << 48 | (type)(b)->h[6] << 40 | (type)(b)->h[5] << 32 \
        | (type)(b)->h[4] << 24 | (type)(b)->h[3] << 16 | (type)(b)->h[2] << 8 | (type)(b)->h[1] )

static inline double __xbyte2float(xl_ptr b)
{
    union float64 f;
    f.b[0] = b->h[1];
    f.b[1] = b->h[2];
    f.b[2] = b->h[3];
    f.b[3] = b->h[4];
    f.b[4] = b->h[5];
    f.b[5] = b->h[6];
    f.b[6] = b->h[7];
    f.b[7] = b->h[8];
    return f.f;
}

#else //__LITTLE_ENDIAN__

#define __n2b(n, type) \
        (struct xl){ \
            type, \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __i2b(n) __n2b(n, XLINE_TYPE_INT)
#define __u2b(n) __n2b(n, XLINE_TYPE_UINT)
#define __f2b(n) __n2b(n, XLINE_TYPE_FLOAT)

#define __o2b(n, type) \
        (struct xl){ \
            (type), \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __b2n(b, type) \
        ( (type)(b)->h[1] << 56 | (type)(b)->h[2] << 48 | (type)(b)->h[3] << 40 | (type)(b)->h[4] << 32 \
        | (type)(b)->h[5] << 24 | (type)(b)->h[6] << 16 | (type)(b)->h[7] << 8 | (type)(b)->h[8] )

static inline double __xbyte2float(xl_ptr b)
{
    union float64 f;
    f.b[0] = b->h[8];
    f.b[1] = b->h[7];
    f.b[2] = b->h[6];
    f.b[3] = b->h[5];
    f.b[4] = b->h[4];
    f.b[5] = b->h[3];
    f.b[6] = b->h[2];
    f.b[7] = b->h[1];
    return f.f;
}

#endif //__LITTLE_ENDIAN__

#define __b2i(b)   __b2n(b, int64_t)
#define __b2u(b)   __b2n(b, uint64_t)
#define __b2f(b)   __xbyte2float(b)
#define __b2d(b)    (&(b)->h[XLINE_HEAD_SIZE])

#define __typeis_int(b)         ((b)->h[0] == XLINE_TYPE_INT)
#define __typeis_uint(b)        ((b)->h[0] == XLINE_TYPE_UINT)
#define __typeis_float(b)       ((b)->h[0] == XLINE_TYPE_FLOAT)
#define __typeis_word(b)        ((b)->h[0] == XLINE_TYPE_STR)
#define __typeis_bin(b)         ((b)->h[0] == XLINE_TYPE_BIN)
#define __typeis_list(b)        ((b)->h[0] == XLINE_TYPE_LIST)
#define __typeis_tree(b)        ((b)->h[0] == XLINE_TYPE_TREE)
#define __typeis_object(b)      ((b)->h[0] > XLINE_TYPE_FLOAT)

#define __sizeof_head(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? XLINE_HEAD_SIZE : 1
#define __sizeof_data(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? __b2u((b)) : 8
#define __sizeof_line(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? __b2u((b)) + XLINE_HEAD_SIZE : XLINE_HEAD_SIZE


typedef struct xlkv {
    uint64_t wpos, rpos, size;
    uint8_t *head;
    uint8_t *key;
    xl_ptr val;
}xlkv_t;

typedef xlkv_t* xlkv_ptr;


static inline struct xlkv xl_maker(uint64_t size)
{
    struct xlkv kv;
    if (size < 1024){
        size = 1024;
    }
    kv.size = size;
    kv.wpos = XLINE_HEAD_SIZE;
    kv.head = (uint8_t*) malloc(kv.size);
    return kv;
}

static inline uint64_t xl_hold_kv(xlkv_ptr kv, const char *key)
{
    kv->key = kv->head + kv->wpos;
    kv->key[0] = slength(key) + 1;
    mcopy(kv->key + 1, key, kv->key[0]);
    kv->wpos += 1 + kv->key[0];
    uint64_t pos = kv->wpos;
    kv->wpos += XLINE_HEAD_SIZE;
    return pos;
}

static inline void xl_save_kv(xlkv_ptr kv, uint64_t pos)
{
    uint64_t len = kv->wpos - pos - XLINE_HEAD_SIZE;
    *((xl_ptr)(kv->head + pos)) = __o2b(len, XLINE_TYPE_TREE);
}

static inline uint64_t xl_hold_list(xlkv_ptr kv, const char *key)
{
    return xl_hold_kv(kv, key);
}

static inline void xl_save_list(xlkv_ptr kv, uint64_t pos)
{
    uint64_t len = kv->wpos - pos - XLINE_HEAD_SIZE;
    *((xl_ptr)(kv->head + pos)) = __o2b(len, XLINE_TYPE_LIST);
}

static inline uint64_t xl_list_hold_kv(xlkv_ptr kv)
{
    kv->rpos = kv->wpos;
    kv->wpos += XLINE_HEAD_SIZE;
    return kv->rpos;
}

static inline void xl_list_save_kv(xlkv_ptr kv, uint64_t pos)
{
    return xl_save_kv(kv, pos);
}

static inline uint64_t xl_append_object(xlkv_ptr kv, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_HEAD_SIZE + size);
    if ((int64_t)(kv->size - kv->wpos) < len){
        kv->size += (kv->size + len);
        kv->key = (uint8_t*)malloc(kv->size);
        if(kv->key == NULL){
            return EENDED;
        }
        mcopy(kv->key, kv->head, kv->wpos);
        free(kv->head);
        kv->head = kv->key;
    }
    kv->key = kv->head + kv->wpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的尾部
    kv->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(kv->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(maker->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(kv->key + kv->key[0]) = '\0';
    // 因为我们的 key 出了字符串的长度还有一个字节的头，所以这里再加 1
    kv->wpos += (1 + kv->key[0]);
    kv->val = (xl_ptr)(kv->head + kv->wpos);
    *(kv->val) = __o2b(size, flag);
    mcopy(&(kv->val->h[XLINE_HEAD_SIZE]), val, size);
    kv->wpos += (XLINE_HEAD_SIZE + size);
    kv->rpos = kv->wpos - XLINE_HEAD_SIZE;
    *((xl_ptr)(kv->head)) = __o2b(kv->rpos, XLINE_TYPE_TREE);
    return kv->wpos;
}

static inline uint64_t xl_add_word(xlkv_ptr kv, const char *key, const char *word)
{
    return xl_append_object(kv, key, slength(key), word, slength(word) + 1, XLINE_TYPE_STR);
}

static inline uint64_t xl_add_str(xlkv_ptr kv, const char *key, const char *str, uint64_t size)
{
    return xl_append_object(kv, key, slength(key), str, size, XLINE_TYPE_STR);
}

static inline uint64_t xl_add_bin(xlkv_ptr kv, const char *key, const void *val, uint64_t size)
{
    return xl_append_object(kv, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline uint64_t xl_add_kv(xlkv_ptr kv, const char *key, xlkv_ptr subkv)
{
    return xl_append_object(kv, key, slength(key), subkv->head, subkv->wpos, XLINE_TYPE_TREE);
}

static inline uint64_t xl_add_list(xlkv_ptr kv, const char *key, xlkv_ptr sublist)
{
    return xl_append_object(kv, key, slength(key), sublist->head, sublist->wpos, XLINE_TYPE_LIST);
}

static inline uint64_t xl_append_number(xlkv_ptr kv, const char *key, size_t keylen, struct xl val)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_HEAD_SIZE);
    if ((int64_t)(kv->size - kv->wpos) < len){
        kv->size += (kv->size + len);
        kv->key = (uint8_t*)malloc(kv->size);
        if(kv->key == NULL){
            return EENDED;
        }
        mcopy(kv->key, kv->head, kv->wpos);
        free(kv->head);
        kv->head = kv->key;
    }
    kv->key = kv->head + kv->wpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的尾部
    kv->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(kv->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(maker->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(kv->key + kv->key[0]) = '\0';
    // 因为我们的 key 出了字符串的长度还有一个字节的头，所以这里再加 1
    kv->wpos += (1 + kv->key[0]);
    kv->val = (xl_ptr)(kv->head + kv->wpos);
    *(kv->val) = val;
    kv->wpos += __sizeof_line(kv->val);
    kv->rpos = kv->wpos - XLINE_HEAD_SIZE;
    *((xl_ptr)(kv->head)) = __o2b(kv->rpos, XLINE_TYPE_TREE);
    return kv->wpos;
}

static inline uint64_t xl_add_integer(xlkv_ptr kv, const char *key, int64_t i64)
{
    return xl_append_number(kv, key, slength(key), __i2b(i64));
}

static inline uint64_t xl_add_number(xlkv_ptr kv, const char *key, uint64_t u64)
{
    return xl_append_number(kv, key, slength(key), __u2b(u64));
}

static inline uint64_t xl_add_float(xlkv_ptr kv, const char *key, double f64)
{
    return xl_append_number(kv, key, slength(key), __f2b(f64));
}

static inline uint64_t xl_add_ptr(xlkv_ptr kv, const char *key, void *p)
{
    uint64_t n = (uint64_t)(p);
    return xl_append_number(kv, key, slength(key), __u2b(n));
}

static inline xlkv_t xl_parser(xl_ptr xl)
{
    xlkv_t kv = {0};
    if (__typeis_tree(xl) || __typeis_list(xl)){
        kv.rpos = XLINE_HEAD_SIZE;
        kv.wpos = __sizeof_data(xl);
        kv.size = kv.wpos;
        kv.head = xl->h;
    }
    return kv;
}

static inline xl_ptr xl_next(xlkv_ptr kv)
{
    if (kv->rpos < kv->wpos){
        kv->key = kv->head + kv->rpos;
        kv->rpos += 1 + kv->key[0];
        kv->key++;
        kv->val = (xl_ptr)(kv->head + kv->rpos);
        kv->rpos += __sizeof_line(kv->val);
        return kv->val;
    }
    return NULL;
}

static inline xl_ptr xl_find(xlkv_ptr kv, const char *key)
{
    uint64_t rpos = kv->rpos;

    while (kv->rpos < kv->wpos) {
        kv->key = kv->head + kv->rpos;
        kv->rpos += 1 + kv->key[0];
        kv->val = (xl_ptr)(kv->head + kv->rpos);
        kv->rpos += __sizeof_line(kv->val);
        if (slength(key) + 1 == kv->key[0]
            && mcompare(key, kv->key + 1, kv->key[0]) == 0){
            kv->key++;
            return kv->val;
        }
    }

    kv->rpos = XLINE_HEAD_SIZE;

    while (kv->rpos < rpos) {
        kv->key = kv->head + kv->rpos;
        kv->rpos += 1 + kv->key[0];
        kv->val = (xl_ptr)(kv->head + kv->rpos);
        kv->rpos += __sizeof_line(kv->val);
        if (slength(key) + 1 == kv->key[0]
            && mcompare(key, kv->key + 1, kv->key[0]) == 0){
            kv->key++;
            return kv->val;
        }
    }

    kv->rpos = XLINE_HEAD_SIZE;

    return NULL;
}

static inline int64_t xl_find_integer(xlkv_ptr kv, const char *key)
{
    xl_ptr val = xl_find(kv, key);
    if (val){
        return __b2i(val);
    }
    return EENDED;
}

static inline uint64_t xl_find_number(xlkv_ptr kv, const char *key)
{
    xl_ptr val = xl_find(kv, key);
    if (val){
        return __b2u(val);
    }
    return EENDED;
}

static inline double xl_find_float(xlkv_ptr kv, const char *key)
{
    xl_ptr val = xl_find(kv, key);
    if (val){
        return __b2f(val);
    }
    return (double)EENDED;
}

static inline const char* xl_find_word(xlkv_ptr kv, const char *key)
{
    xl_ptr val = xl_find(kv, key);
    if (val){
        return (const char*)__b2d(val);
    }
    return NULL;
}

static inline void* xl_find_ptr(xlkv_ptr kv, const char *key)
{
    xl_ptr val = xl_find(kv, key);
    if (val){
        return (void *)(__b2u(val));
    }
    return NULL;
}

static inline uint64_t xl_list_append(xlkv_ptr kv, xl_ptr ptr)
{
    kv->rpos = __sizeof_line(ptr);
    if ((int64_t)(kv->size - kv->wpos) < kv->rpos){
        kv->size += (kv->size + kv->rpos);
        kv->key = (uint8_t*)malloc(kv->size);
        if(kv->key == NULL){
            return EENDED;
        }        
        mcopy(kv->key, kv->head, kv->wpos);
        free(kv->head);
        kv->head = kv->key;
    }
    
    mcopy(kv->head + kv->wpos, ptr->h, kv->rpos);
    kv->wpos += kv->rpos;
    kv->rpos = kv->wpos - XLINE_HEAD_SIZE;
    *((xl_ptr)(kv->head)) = __o2b(kv->rpos, XLINE_TYPE_TREE);
    return kv->wpos;
}

static inline xl_ptr xl_list_next(xlkv_ptr kv)
{
    if (kv->rpos < kv->wpos){
        xl_ptr ptr = (xl_ptr)(kv->head + kv->rpos);
        kv->rpos += __sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static inline void __xl_printf(xl_ptr xptr, const char *key, int depth)
{
    xlkv_t parser = xl_parser(xptr);

    int len = slength(key);

    if (depth == 1){
        __xlogi("%*s: {\n", (depth) * 4, key);
    }else {
        if (len == 0){
            __xlogi("%*s{\n", (depth) * 4, "");
        }else {
            __xlogi("%*s: {\n", (depth) * 4, key);
        }
    }

    while ((xptr = xl_next(&parser)) != NULL)
    {
        if (__typeis_int(xptr)){

            __xlogi("%*s: %ld,\n", (depth + 1) * 4, parser.key, __b2i(xptr));

        }else if (__typeis_uint(xptr)){

            __xlogi("%*s: %lu,\n", (depth + 1) * 4, parser.key, __b2i(xptr));

        }else if (__typeis_float(xptr)){

            __xlogi("%*s: %lf,\n", (depth + 1) * 4, parser.key, __b2f(xptr));

        }else if (__typeis_word(xptr)){

            __xlogi("%*s: %s,\n", (depth + 1) * 4, parser.key, __b2d(xptr));

        }else if (__typeis_tree(xptr)){

            __xl_printf(xptr, (const char*)parser.key, depth + 1);

        }else if (__typeis_list(xptr)){

            __xlogi("%*s: {\n", (depth + 1) * 4, parser.key);

            xlkv_t plist = xl_parser(xptr);

            while ((xptr = xl_list_next(&plist)) != NULL)
            {
                if (__typeis_int(xptr)){

                    __xlogi("    %*d,\n", depth * 4, __b2i(xptr));

                }else if (__typeis_tree(xptr)){
                    
                    __xl_printf(xptr, "", depth + 1);
                }
            }

            __xlogi("  %*s},\n", depth * 4, "");

        }else {
            __xloge("__xl_printf >>>>--------> type error\n");
        }
    }

    if (depth == 1){
        __xlogi("%*s}\n", (depth - 1) * 4, "");
    }else {
        if (len == 0){
            __xlogi("%*s},\n", (depth) * 4, "");
        }else {
            __xlogi("%*s},\n", (depth) * 4, "");
        }   
    }
}


static inline void xl_printf(void *xptr)
{
    __xl_printf((xl_ptr)xptr, "root", 1);
}

#endif //__XLINE_H__
