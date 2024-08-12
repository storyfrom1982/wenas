#ifndef __XLINE_H__
#define __XLINE_H__

#include "xmalloc.h"

enum {
    XLINE_TYPE_INT = 0x01, //整数
    XLINE_TYPE_FLOAT = 0x02, //实数
    XLINE_TYPE_STR = 0x04, //字符串
    XLINE_TYPE_BIN = 0x08, //二进制数据
    XLINE_TYPE_LIST = 0x10, //列表
    XLINE_TYPE_TREE = 0x20 //树
};

//xbyte 静态分配的最大长度（ 64bit 数的长度为 8 字节，加 1 字节头部标志位 ）
#define XLINE_HEAD_SIZE      9


typedef struct xbyte {
    uint8_t h[XLINE_HEAD_SIZE];
}xbyte_t;

typedef xbyte_t* xbyte_ptr;

union float64 {
    double f;
    char b[8];
};

#ifdef __LITTLE_ENDIAN__

#define __n2b(n) \
        (struct xbyte){ \
            XLINE_TYPE_INT, \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __f2b(n) \
        (struct xbyte){ \
            XLINE_TYPE_FLOAT, \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __o2b(n, type) \
        (struct xbyte){ \
            (type), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __b2n(b, type) \
        ( (type)(b)->h[8] << 56 | (type)(b)->h[7] << 48 | (type)(b)->h[6] << 40 | (type)(b)->h[5] << 32 \
        | (type)(b)->h[4] << 24 | (type)(b)->h[3] << 16 | (type)(b)->h[2] << 8 | (type)(b)->h[1] )

static inline double __xbyte2float(xbyte_ptr b)
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

#define __n2b(n) \
        (struct xbyte){ \
            XLINE_TYPE_INT, \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __f2b(n) \
        (struct xbyte){ \
            XLINE_TYPE_FLOAT, \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __o2b(n, type) \
        (struct xbyte){ \
            (type), \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __b2n(b, type) \
        ( (type)(b)->h[1] << 56 | (type)(b)->h[2] << 48 | (type)(b)->h[3] << 40 | (type)(b)->h[4] << 32 \
        | (type)(b)->h[5] << 24 | (type)(b)->h[6] << 16 | (type)(b)->h[7] << 8 | (type)(b)->h[8] )

static inline double __xbyte2float(xbyte_ptr b)
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
#define __typeis_float(b)       ((b)->h[0] == XLINE_TYPE_FLOAT)
#define __typeis_word(b)        ((b)->h[0] == XLINE_TYPE_STR)
#define __typeis_bin(b)         ((b)->h[0] == XLINE_TYPE_BIN)
#define __typeis_list(b)        ((b)->h[0] == XLINE_TYPE_LIST)
#define __typeis_tree(b)        ((b)->h[0] == XLINE_TYPE_TREE)
#define __typeis_object(b)      ((b)->h[0] > XLINE_TYPE_FLOAT)

#define __sizeof_head(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? XLINE_HEAD_SIZE : 1
#define __sizeof_data(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? __b2u((b)) : 8
#define __sizeof_line(b)        (b)->h[0] > XLINE_TYPE_FLOAT ? __b2u((b)) + XLINE_HEAD_SIZE : XLINE_HEAD_SIZE


typedef struct xline {
    uint64_t wpos, rpos, size;
    uint8_t *key;
    xbyte_t *val;
    union{
        uint8_t *head;
        xbyte_t *byte;
    };
    struct xline *maker;
}xline_t;

typedef xline_t* xline_ptr;


static inline struct xline xline_maker(uint64_t size)
{
    struct xline maker;
    if (size < 1024){
        size = 1024;
    }
    maker.size = size;
    maker.wpos = XLINE_HEAD_SIZE;
    maker.head = (uint8_t*) malloc(maker.size);
    maker.maker = &maker;
    return maker;
}

static inline uint64_t xline_hold_tree(xline_ptr maker, const char *key)
{
    maker->key = maker->head + maker->wpos;
    maker->key[0] = slength(key) + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    maker->wpos += 1 + maker->key[0];
    uint64_t pos = maker->wpos;
    maker->wpos += XLINE_HEAD_SIZE;
    return pos;
}

static inline void xline_save_tree(xline_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_HEAD_SIZE;
    *((xbyte_ptr)(maker->head + pos)) = __o2b(len, XLINE_TYPE_TREE);
}

static inline uint64_t xline_hold_list(xline_ptr maker, const char *key)
{
    return xline_hold_tree(maker, key);
}

static inline void xline_save_list(xline_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_HEAD_SIZE;
    *((xbyte_ptr)(maker->head + pos)) = __o2b(len, XLINE_TYPE_LIST);
}

static inline uint64_t xline_list_hold_tree(xline_ptr maker)
{
    maker->rpos = maker->wpos;
    maker->wpos += XLINE_HEAD_SIZE;
    return maker->rpos;
}

static inline void xline_list_save_tree(xline_ptr maker, uint64_t pos)
{
    return xline_save_tree(maker, pos);
}

static inline uint64_t xline_add_object(xline_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_HEAD_SIZE + size);
    if ((int64_t)(maker->size - maker->wpos) < len){
        maker->size += (maker->size + len);
        maker->key = (uint8_t*)malloc(maker->size);
        if(maker->key == NULL){
            return EENDED;
        }
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    maker->key = maker->head + maker->wpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的尾部
    maker->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(maker->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(maker->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(maker->key + maker->key[0]) = '\0';
    // 因为我们的 key 出了字符串的长度还有一个字节的头，所以这里再加 1
    maker->wpos += (1 + maker->key[0]);
    maker->val = (xbyte_ptr)(maker->head + maker->wpos);
    *(maker->val) = __o2b(size, flag);
    mcopy(&(maker->val->h[XLINE_HEAD_SIZE]), val, size);
    maker->wpos += (XLINE_HEAD_SIZE + size);
    maker->rpos = maker->wpos - XLINE_HEAD_SIZE;
    *((xbyte_ptr)(maker->head)) = __o2b(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline uint64_t xline_add_word(xline_ptr maker, const char *key, const char *word)
{
    return xline_add_object(maker, key, slength(key), word, slength(word) + 1, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_string(xline_ptr maker, const char *key, const char *str, uint64_t size)
{
    return xline_add_object(maker, key, slength(key), str, size, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_binary(xline_ptr maker, const char *key, const void *val, uint64_t size)
{
    return xline_add_object(maker, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline uint64_t xline_add_tree(xline_ptr maker, const char *key, xline_ptr treemaker)
{
    return xline_add_object(maker, key, slength(key), treemaker->head, treemaker->wpos, XLINE_TYPE_TREE);
}

static inline uint64_t xline_add_list(xline_ptr maker, const char *key, xline_ptr listmaker)
{
    return xline_add_object(maker, key, slength(key), listmaker->head, listmaker->wpos, XLINE_TYPE_LIST);
}

static inline uint64_t xline_add_number(xline_ptr maker, const char *key, size_t keylen, struct xbyte val)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_HEAD_SIZE);
    if ((int64_t)(maker->size - maker->wpos) < len){
        maker->size += (maker->size + len);
        maker->key = (uint8_t*)malloc(maker->size);
        if(maker->key == NULL){
            return EENDED;
        }
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    maker->key = maker->head + maker->wpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的尾部
    maker->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(maker->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(maker->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(maker->key + maker->key[0]) = '\0';
    // 因为我们的 key 出了字符串的长度还有一个字节的头，所以这里再加 1
    maker->wpos += (1 + maker->key[0]);
    maker->val = (xbyte_ptr)(maker->head + maker->wpos);
    *(maker->val) = val;
    maker->wpos += __sizeof_line(maker->val);
    maker->rpos = maker->wpos - XLINE_HEAD_SIZE;
    *((xbyte_ptr)(maker->head)) = __o2b(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline uint64_t xline_add_integer(xline_ptr maker, const char *key, int64_t i64)
{
    return xline_add_number(maker, key, slength(key), __n2b(i64));
}

static inline uint64_t xline_add_unsigned(xline_ptr maker, const char *key, uint64_t u64)
{
    return xline_add_number(maker, key, slength(key), __n2b(u64));
}

static inline uint64_t xline_add_real(xline_ptr maker, const char *key, double f64)
{
    return xline_add_number(maker, key, slength(key), __f2b(f64));
}

static inline uint64_t xline_add_pointer(xline_ptr maker, const char *key, void *p)
{
    uint64_t n = (uint64_t)(p);
    return xline_add_number(maker, key, slength(key), __n2b(n));
}

static inline xline_t xline_parser(xbyte_ptr byte)
{
    xline_t parser = {0};
    if (__typeis_tree(byte) || __typeis_list(byte)){
        parser.rpos = 0;
        parser.wpos = __sizeof_data(byte);
        parser.size = parser.wpos;
        parser.head = byte->h + XLINE_HEAD_SIZE;
    }else {
        parser.byte = byte;
    }
    return parser;
}

static inline xbyte_ptr xline_next(xline_ptr parser)
{
    if (parser->rpos < parser->wpos){
        parser->key = parser->head + parser->rpos;
        parser->rpos += 1 + parser->key[0];
        parser->key++;
        parser->val = (xbyte_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(parser->val);
        return parser->val;
    }
    return NULL;
}

static inline xbyte_ptr xline_find(xline_ptr parser, const char *key)
{
    uint64_t rpos = parser->rpos;

    while (parser->rpos < parser->wpos) {
        parser->key = parser->head + parser->rpos;
        parser->rpos += 1 + parser->key[0];
        parser->val = (xbyte_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(parser->val);
        if (slength(key) + 1 == parser->key[0]
            && mcompare(key, parser->key + 1, parser->key[0]) == 0){
            parser->key++;
            return parser->val;
        }
    }

    parser->rpos = 0;

    while (parser->rpos < rpos) {
        parser->key = parser->head + parser->rpos;
        parser->rpos += 1 + parser->key[0];
        parser->val = (xbyte_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(parser->val);
        if (slength(key) + 1 == parser->key[0]
            && mcompare(key, parser->key + 1, parser->key[0]) == 0){
            parser->key++;
            return parser->val;
        }
    }

    parser->rpos = 0;

    return NULL;
}

static inline int64_t xline_find_integer(xline_ptr parser, const char *key)
{
    xbyte_ptr val = xline_find(parser, key);
    if (val){
        return __b2i(val);
    }
    return EENDED;
}

static inline uint64_t xline_find_unsigned(xline_ptr parser, const char *key)
{
    xbyte_ptr val = xline_find(parser, key);
    if (val){
        return __b2u(val);
    }
    return EENDED;
}

static inline double xline_find_float(xline_ptr parser, const char *key)
{
    xbyte_ptr val = xline_find(parser, key);
    if (val){
        return __b2f(val);
    }
    return (double)EENDED;
}

static inline const char* xline_find_word(xline_ptr parser, const char *key)
{
    xbyte_ptr val = xline_find(parser, key);
    if (val){
        return (const char*)__b2d(val);
    }
    return NULL;
}

static inline void* xline_find_pointer(xline_ptr parser, const char *key)
{
    xbyte_ptr val = xline_find(parser, key);
    if (val){
        return (void *)(__b2u(val));
    }
    return NULL;
}

static inline uint64_t xline_list_append(xline_ptr maker, xbyte_ptr ptr)
{
    maker->rpos = __sizeof_line(ptr);
    if ((int64_t)(maker->size - maker->wpos) < maker->rpos){
        maker->size += (maker->size + maker->rpos);
        maker->key = (uint8_t*)malloc(maker->size);
        if(maker->key == NULL){
            return EENDED;
        }        
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    
    mcopy(maker->head + maker->wpos, ptr->h, maker->rpos);
    maker->wpos += maker->rpos;
    maker->rpos = maker->wpos - XLINE_HEAD_SIZE;
    *((xbyte_ptr)(maker->head)) = __o2b(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline xbyte_ptr xline_list_next(xline_ptr parser)
{
    if (parser->rpos < parser->wpos){
        xbyte_ptr ptr = (xbyte_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static inline void __xline_printf(xbyte_ptr xptr, const char *key, int depth)
{
    xline_t parser = xline_parser(xptr);

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

    while ((xptr = xline_next(&parser)) != NULL)
    {
        if (__typeis_int(xptr)){

            __xlogi("%*s: %ld,\n", (depth + 1) * 4, parser.key, __b2i(xptr));

        }else if (__typeis_float(xptr)){

            __xlogi("%*s: %lf,\n", (depth + 1) * 4, parser.key, __b2f(xptr));

        }else if (__typeis_word(xptr)){

            __xlogi("%*s: %s,\n", (depth + 1) * 4, parser.key, __b2d(xptr));

        }else if (__typeis_tree(xptr)){

            __xline_printf(xptr, (const char*)parser.key, depth + 1);

        }else if (__typeis_list(xptr)){

            __xlogi("%*s: {\n", (depth + 1) * 4, parser.key);

            xline_t plist = xline_parser(xptr);

            while ((xptr = xline_list_next(&plist)) != NULL)
            {
                if (__typeis_int(xptr)){

                    __xlogi("    %*d,\n", depth * 4, __b2i(xptr));

                }else if (__typeis_tree(xptr)){
                    
                    __xline_printf(xptr, "", depth + 1);
                }
            }

            __xlogi("  %*s},\n", depth * 4, "");

        }else {
            __xloge("__xline_printf >>>>--------> type error\n");
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


static inline void xline_printf(void *xptr)
{
    __xline_printf((xbyte_ptr)xptr, "root", 1);
}

#endif //__XLINE_H__
