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

#define __sizeof_head(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? XLINE_SIZE : 1
#define __sizeof_data(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? __l2u((l)) : 8
#define __sizeof_line(l)        (l)->b[0] > XLINE_TYPE_FLOAT ? __l2u((l)) + XLINE_SIZE : XLINE_SIZE


typedef struct xmaker {
    uint64_t wpos, rpos, len;
    uint8_t *key;
    xline_ptr val;
    uint8_t *head;
}xmaker_t;

typedef xmaker_t* xmaker_ptr;


static inline void xmaker_free(xmaker_ptr maker)
{
    if (maker && maker->head){
        free(maker->head);
    }
}

static inline struct xmaker xmaker_build(uint64_t len)
{
    struct xmaker maker;
    if (len < XLINE_SIZE){
        len = XLINE_SIZE;
    }
    maker.wpos = XLINE_SIZE;
    maker.len = len;
    maker.head = (uint8_t*) malloc(maker.len);
    return maker;
}

static inline void xmaker_clear(xmaker_ptr maker)
{
    maker->wpos = maker->rpos = 0;
    maker->key = NULL;
    maker->val = NULL;
}

static inline uint64_t xmaker_hold_tree(xmaker_ptr maker, const char *key)
{
    maker->key = maker->head + maker->wpos;
    maker->key[0] = slength(key) + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    maker->wpos += 1 + maker->key[0];
    uint64_t pos = maker->wpos;
    maker->wpos += XLINE_SIZE;
    return pos;
}

static inline void xmaker_save_tree(xmaker_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(maker->head + pos)) = __s2l(len, XLINE_TYPE_TREE);
}

static inline uint64_t xmaker_hold_list(xmaker_ptr maker, const char *key)
{
    return xmaker_hold_tree(maker, key);
}

static inline void xmaker_save_list(xmaker_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(maker->head + pos)) = __s2l(len, XLINE_TYPE_LIST);
}

static inline uint64_t xmaker_list_hold_tree(xmaker_ptr maker)
{
    maker->rpos = maker->wpos;
    maker->wpos += XLINE_SIZE;
    return maker->rpos;
}

static inline void xmaker_list_save_tree(xmaker_ptr maker, uint64_t pos)
{
    return xmaker_save_tree(maker, pos);
}

static inline uint64_t xline_append_object(xmaker_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    // key[keylen,key,'\0']
    // (1 + keylen + 1)
    maker->rpos = (2 + keylen + XLINE_SIZE + size);
    if ((int64_t)(maker->len - maker->wpos) < maker->rpos){
        maker->len += (maker->len + maker->rpos);
        maker->key = (uint8_t*)malloc(maker->len);
        if(maker->key == NULL){
            return EENDED;
        }
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    maker->key = maker->head + maker->wpos;
    maker->key[0] = keylen + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    maker->wpos += 1 + maker->key[0];
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
    return xline_append_object(maker, key, slength(key), word, slength(word) + 1, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_string(xmaker_ptr maker, const char *key, const char *str, uint64_t size)
{
    return xline_append_object(maker, key, slength(key), str, size, XLINE_TYPE_STR);
}

static inline uint64_t xline_add_map(xmaker_ptr maker, const char *key, xmaker_ptr xmap)
{
    return xline_append_object(maker, key, slength(key), xmap->head, xmap->wpos, XLINE_TYPE_TREE);
}

static inline uint64_t xline_add_list(xmaker_ptr maker, const char *key, xmaker_ptr xlist)
{
    return xline_append_object(maker, key, slength(key), xlist->head, xlist->wpos, XLINE_TYPE_LIST);
}

static inline uint64_t xline_add_binary(xmaker_ptr maker, const char *key, const void *val, uint64_t size)
{
    return xline_append_object(maker, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline uint64_t xline_append_number(xmaker_ptr maker, const char *key, size_t keylen, struct xline val)
{
    // key[keylen,key,'\0']
    // (1 + keylen + 1)
    //TODO key 长度不能大于 256 因为只用一个字节存储长度，这里要重写
    maker->rpos = (2 + keylen + XLINE_SIZE);
    if ((int64_t)(maker->len - maker->wpos) < maker->rpos){
        maker->len += (maker->len + maker->rpos);
        maker->key = (uint8_t*)malloc(maker->len);
        if(maker->key == NULL){
            return EENDED;
        }
        mcopy(maker->key, maker->head, maker->wpos);
        free(maker->head);
        maker->head = maker->key;
    }
    maker->key = maker->head + maker->wpos;
    maker->key[0] = keylen + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    maker->wpos += 1 + maker->key[0];
    maker->val = (xline_ptr)(maker->head + maker->wpos);
    *(maker->val) = val;
    maker->wpos += __sizeof_line(maker->val);
    maker->rpos = maker->wpos - XLINE_SIZE;
    *((xline_ptr)(maker->head)) = __s2l(maker->rpos, XLINE_TYPE_TREE);
    return maker->wpos;
}

static inline uint64_t xline_add_int(xmaker_ptr maker, const char *key, int64_t n64)
{
    return xline_append_number(maker, key, slength(key), __n2l(n64));
}

static inline uint64_t xline_add_uint(xmaker_ptr maker, const char *key, uint64_t u64)
{
    return xline_append_number(maker, key, slength(key), __n2l(u64));
}

static inline uint64_t xline_add_float(xmaker_ptr maker, const char *key, double f64)
{
    return xline_append_number(maker, key, slength(key), __f2l(f64));
}

static inline uint64_t xline_add_ptr(xmaker_ptr maker, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    return xline_append_number(maker, key, slength(key), __n2l(n));
}

static inline struct xmaker xline_parse(xline_ptr xmap)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    struct xmaker maker = {0};
    maker.rpos = 0;
    maker.len = __sizeof_data(xmap);
    maker.head = xmap->b + XLINE_SIZE;
    return maker;
}

static inline xline_ptr xline_next(xmaker_ptr maker)
{
    if (maker->rpos < maker->len){
        maker->key = maker->head + maker->rpos;
        maker->rpos += 1 + maker->key[0];
        maker->key++;
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(maker->val);
        return maker->val;
    }
    return NULL;
}

static inline xline_ptr xline_find(xmaker_ptr maker, const char *key)
{
    maker->wpos = maker->rpos;

    while (maker->rpos < maker->len) {
        maker->key = maker->head + maker->rpos;
        maker->rpos += 1 + maker->key[0];
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(maker->val);
        if (slength(key) + 1 == maker->key[0]
            && mcompare(key, maker->key + 1, maker->key[0]) == 0){
            maker->key++;
            return maker->val;
        }
    }

    maker->rpos = 0;

    while (maker->rpos < maker->wpos) {
        maker->key = maker->head + maker->rpos;
        maker->rpos += 1 + maker->key[0];
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(maker->val);
        if (slength(key) + 1 == maker->key[0]
            && mcompare(key, maker->key + 1, maker->key[0]) == 0){
            return maker->val;
        }
    }

    maker->rpos = 0;

    return NULL;
}

static inline int64_t xline_find_int(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2i(val);
    }
    return EENDED;
}

static inline uint64_t xline_find_uint(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2u(val);
    }
    return EENDED;
}

static inline double xline_find_float(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2f(val);
    }
    return EENDED;
}

static inline const char* xline_find_word(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return (const char*)__l2data(val);
    }
    return NULL;
}

static inline void* xline_find_ptr(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return (void *)(__l2u(val));
    }
    return NULL;
}

static inline uint64_t xline_list_append(xmaker_ptr maker, xline_ptr ptr)
{
    maker->rpos = __sizeof_line(ptr);
    if ((int64_t)(maker->len - maker->wpos) < maker->rpos){
        maker->len += (maker->len + maker->rpos);
        maker->key = (uint8_t*)malloc(maker->len);
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

static inline xline_ptr xline_list_next(xmaker_ptr maker)
{
    if (maker->rpos < maker->len){
        xline_ptr ptr = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

#endif //__XLINE_H__