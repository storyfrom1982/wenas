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

//xline 静态分配的最大长度（ 64bit 数的长度为 8 字节，加 1 字节头部标志位 ）
#define XLINE_SIZE      9


typedef struct xline {
    uint8_t b[XLINE_SIZE];
}*xline_ptr;

union float64 {
    double f;
    char b[8];
};

#define __n2l(n) \
        (struct xline){ \
            XLINE_TYPE_INT, \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __f2l(n) \
        (struct xline){ \
            XLINE_TYPE_FLOAT, \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __s2l(n, type) \
        (struct xline){ \
            (type), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __l2n(l, type) \
        ( (type)(l)->b[8] << 56 | (type)(l)->b[7] << 48 | (type)(l)->b[6] << 40 | (type)(l)->b[5] << 32 \
        | (type)(l)->b[4] << 24 | (type)(l)->b[3] << 16 | (type)(l)->b[2] << 8 | (type)(l)->b[1] )

static inline double __xline2float(xline_ptr l)
{
    union float64 r;
    r.b[0] = l->b[1];
    r.b[1] = l->b[2];
    r.b[2] = l->b[3];
    r.b[3] = l->b[4];
    r.b[4] = l->b[5];
    r.b[5] = l->b[6];
    r.b[6] = l->b[7];
    r.b[7] = l->b[8];
    return r.f;
}

#define __l2i(l)   __l2n(l, int64_t)
#define __l2u(l)   __l2n(l, uint64_t)
#define __l2f(l)   __xline2float(l)

#define __l2data(l)         (&(l)->b[XLINE_SIZE])

#define __typeis_int(l)         ((l)->b[0] == XLINE_TYPE_INT)
#define __typeis_float(l)       ((l)->b[0] == XLINE_TYPE_FLOAT)
#define __typeis_word(l)        ((l)->b[0] == XLINE_TYPE_STR)
#define __typeis_bin(l)         ((l)->b[0] == XLINE_TYPE_BIN)
#define __typeis_list(l)        ((l)->b[0] == XLINE_TYPE_LIST)
#define __typeis_tree(l)        ((l)->b[0] == XLINE_TYPE_TREE)
#define __typeis_object(l)      ((l)->b[0] > XLINE_TYPE_FLOAT)

#define __sizeof_head(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? XLINE_SIZE : 1
#define __sizeof_data(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? __l2u((l)) : 8
#define __sizeof_line(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? __l2u((l)) + XLINE_SIZE : XLINE_SIZE


typedef struct xmaker {
    uint64_t wpos, rpos, range;
    uint8_t *key;
    xline_ptr val;
    union{
        uint8_t *head;
        xline_ptr line;
    };
}xmaker_t;

typedef xmaker_t xparser_t;
typedef xmaker_t* xmaker_ptr;
typedef xparser_t* xparser_ptr;


static inline struct xmaker xline_make(uint64_t size)
{
    struct xmaker maker;
    if (size < 1024){
        size = 1024;
    }
    maker.wpos = XLINE_SIZE;
    maker.range = size;
    maker.head = (uint8_t*) malloc(maker.range);
    return maker;
}

static inline void xline_clear(xmaker_ptr maker)
{
    if (maker && maker->head){
        free(maker->head);
    }
}

static inline uint64_t xline_hold_tree(xmaker_ptr maker, const char *key)
{
    maker->key = maker->head + maker->wpos;
    maker->key[0] = slength(key) + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    maker->wpos += 1 + maker->key[0];
    uint64_t pos = maker->wpos;
    maker->wpos += XLINE_SIZE;
    return pos;
}

static inline void xline_save_tree(xmaker_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(maker->head + pos)) = __s2l(len, XLINE_TYPE_TREE);
}

static inline uint64_t xline_hold_list(xmaker_ptr maker, const char *key)
{
    return xline_hold_tree(maker, key);
}

static inline void xline_save_list(xmaker_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(maker->head + pos)) = __s2l(len, XLINE_TYPE_LIST);
}
// TODO xline_hold_list_tree
static inline uint64_t xline_list_hold_tree(xmaker_ptr maker)
{
    maker->rpos = maker->wpos;
    maker->wpos += XLINE_SIZE;
    return maker->rpos;
}
// TODO xline_save_list_tree
static inline void xline_list_save_tree(xmaker_ptr maker, uint64_t pos)
{
    return xline_save_tree(maker, pos);
}

static inline uint64_t xline_add_object(xmaker_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_SIZE + size);
    if ((int64_t)(maker->range - maker->wpos) < len){
        maker->range += (maker->range + len);
        maker->key = (uint8_t*)malloc(maker->range);
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
    maker->val = (xline_ptr)(maker->head + maker->wpos);
    *(maker->val) = __s2l(size, flag);
    mcopy(&(maker->val->b[XLINE_SIZE]), val, size);
    maker->wpos += (XLINE_SIZE + size);
    maker->rpos = maker->wpos - XLINE_SIZE;
    *((xline_ptr)(maker->head)) = __s2l(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline uint64_t xline_add_word(xmaker_ptr maker, const char *key, const char *word)
{
    return xline_add_object(maker, key, slength(key), word, slength(word) + 1, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_string(xmaker_ptr maker, const char *key, const char *str, uint64_t size)
{
    return xline_add_object(maker, key, slength(key), str, size, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_binary(xmaker_ptr maker, const char *key, const void *val, uint64_t size)
{
    return xline_add_object(maker, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline uint64_t xline_add_tree(xmaker_ptr maker, const char *key, xmaker_ptr mapmaker)
{
    return xline_add_object(maker, key, slength(key), mapmaker->head, mapmaker->wpos, XLINE_TYPE_TREE);
}

static inline uint64_t xline_add_list(xmaker_ptr maker, const char *key, xmaker_ptr listmaker)
{
    return xline_add_object(maker, key, slength(key), listmaker->head, listmaker->wpos, XLINE_TYPE_LIST);
}

static inline uint64_t xline_add_real_numbers(xmaker_ptr maker, const char *key, size_t keylen, struct xline val)
{
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    uint64_t len = (2 + keylen + XLINE_SIZE);
    if ((int64_t)(maker->range - maker->wpos) < len){
        maker->range += (maker->range + len);
        maker->key = (uint8_t*)malloc(maker->range);
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
    maker->val = (xline_ptr)(maker->head + maker->wpos);
    *(maker->val) = val;
    maker->wpos += __sizeof_line(maker->val);
    maker->rpos = maker->wpos - XLINE_SIZE;
    *((xline_ptr)(maker->head)) = __s2l(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline uint64_t xline_add_integer(xmaker_ptr maker, const char *key, int64_t n64)
{
    return xline_add_real_numbers(maker, key, slength(key), __n2l(n64));
}

static inline uint64_t xline_add_number(xmaker_ptr maker, const char *key, uint64_t u64)
{
    return xline_add_real_numbers(maker, key, slength(key), __n2l(u64));
}

static inline uint64_t xline_add_float(xmaker_ptr maker, const char *key, double f64)
{
    return xline_add_real_numbers(maker, key, slength(key), __f2l(f64));
}

static inline uint64_t xline_add_pointer(xmaker_ptr maker, const char *key, void *p)
{
    uint64_t n = (uint64_t)(p);
    return xline_add_real_numbers(maker, key, slength(key), __n2l(n));
}

static inline xparser_t xline_parse(xline_ptr line)
{
    xparser_t parser = {0};
    if (__typeis_tree(line) || __typeis_list(line)){
        parser.rpos = 0;
        parser.wpos = __sizeof_data(line);
        parser.range = parser.wpos;
        parser.head = line->b + XLINE_SIZE;
    }else {
        parser.line = line;
    }
    return parser;
}

static inline xline_ptr xline_next(xparser_ptr parser)
{
    if (parser->rpos < parser->wpos){
        parser->key = parser->head + parser->rpos;
        parser->rpos += 1 + parser->key[0];
        parser->key++;
        parser->val = (xline_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(parser->val);
        return parser->val;
    }
    return NULL;
}

static inline xline_ptr xline_find(xparser_ptr parser, const char *key)
{
    uint64_t rpos = parser->rpos;

    while (parser->rpos < parser->wpos) {
        parser->key = parser->head + parser->rpos;
        parser->rpos += 1 + parser->key[0];
        parser->val = (xline_ptr)(parser->head + parser->rpos);
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
        parser->val = (xline_ptr)(parser->head + parser->rpos);
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

static inline int64_t xline_find_integer(xparser_ptr parser, const char *key)
{
    xline_ptr val = xline_find(parser, key);
    if (val){
        return __l2i(val);
    }
    return EENDED;
}

static inline uint64_t xline_find_number(xparser_ptr parser, const char *key)
{
    xline_ptr val = xline_find(parser, key);
    if (val){
        return __l2u(val);
    }
    return EENDED;
}

static inline double xline_find_float(xparser_ptr parser, const char *key)
{
    xline_ptr val = xline_find(parser, key);
    if (val){
        return __l2f(val);
    }
    return EENDED;
}

static inline const char* xline_find_word(xparser_ptr parser, const char *key)
{
    xline_ptr val = xline_find(parser, key);
    if (val){
        return (const char*)__l2data(val);
    }
    return NULL;
}

static inline void* xline_find_pointer(xparser_ptr parser, const char *key)
{
    xline_ptr val = xline_find(parser, key);
    if (val){
        return (void *)(__l2u(val));
    }
    return NULL;
}

static inline uint64_t xline_list_append(xmaker_ptr maker, xline_ptr ptr)
{
    maker->rpos = __sizeof_line(ptr);
    if ((int64_t)(maker->range - maker->wpos) < maker->rpos){
        maker->range += (maker->range + maker->rpos);
        maker->key = (uint8_t*)malloc(maker->range);
        if(maker->key == NULL){
            return EENDED;
        }        
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    
    mcopy(maker->head + maker->wpos, ptr->b, maker->rpos);
    maker->wpos += maker->rpos;
    maker->rpos = maker->wpos - XLINE_SIZE;
    *((xline_ptr)(maker->head)) = __s2l(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline xline_ptr xline_list_next(xparser_ptr parser)
{
    if (parser->rpos < parser->wpos){
        xline_ptr ptr = (xline_ptr)(parser->head + parser->rpos);
        parser->rpos += __sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static inline void __xline_printf(xline_ptr xptr, const char *key, int depth)
{
    xparser_t parser = xline_parse(xptr);

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

            __xlogi("%*s: %ld,\n", (depth + 1) * 4, parser.key, __l2i(xptr));

        }else if (__typeis_float(xptr)){

            __xlogi("%*s: %lf,\n", (depth + 1) * 4, parser.key, __l2f(xptr));

        }else if (__typeis_word(xptr)){

            __xlogi("%*s: %s,\n", (depth + 1) * 4, parser.key, __l2data(xptr));

        }else if (__typeis_tree(xptr)){

            __xline_printf(xptr, (const char*)parser.key, depth + 1);

        }else if (__typeis_list(xptr)){

            __xlogi("%*s: {\n", (depth + 1) * 4, parser.key);

            xparser_t plist = xline_parse(xptr);

            while ((xptr = xline_list_next(&plist)) != NULL)
            {
                if (__typeis_int(xptr)){

                    __xlogi("    %*d,\n", depth * 4, __l2i(xptr));

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
    __xline_printf((xline_ptr)xptr, "root", 1);
}

#endif //__XLINE_H__
