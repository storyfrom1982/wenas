#ifndef __XLINE_H__
#define __XLINE_H__

#include "xmalloc.h"

enum {
    XLINE_TYPE_INT = 0x01, //整数
    XLINE_TYPE_REAL = 0x02, //实数
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

union real64 {
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
            XLINE_TYPE_REAL, \
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

#define __l2i(l)   __l2n(l, int64_t)
#define __l2u(l)   __l2n(l, uint64_t)

static inline double __l2f(xline_ptr l)
{
    union real64 r;
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

#define __l2data(l)         (&(l)->b[XLINE_SIZE])

#define __typeis_int(l)         ((l)->b[0] == XLINE_TYPE_INT)
#define __typeis_float(l)       ((l)->b[0] == XLINE_TYPE_REAL)
#define __typeis_str(l)         ((l)->b[0] == XLINE_TYPE_STR)
#define __typeis_bin(l)         ((l)->b[0] == XLINE_TYPE_BIN)
#define __typeis_list(l)        ((l)->b[0] == XLINE_TYPE_LIST)
#define __typeis_tree(l)        ((l)->b[0] == XLINE_TYPE_TREE)

#define __sizeof_head(l)        (l)->b[0] > XLINE_TYPE_REAL ? XLINE_SIZE : 1
#define __sizeof_data(l)        (l)->b[0] > XLINE_TYPE_REAL ? __l2u((l)) : 8
#define __sizeof_line(l)        (l)->b[0] > XLINE_TYPE_REAL ? __l2u((l)) + XLINE_SIZE : XLINE_SIZE

#define XKEY_HEAD_SIZE          1
#define XKEY_DATA_MAX_SIZE      255
#define XKEY_MAX_SIZE           (XKEY_HEAD_SIZE + XKEY_DATA_MAX_SIZE)

typedef struct xkey {
    char b[1];
}*xkey_ptr;

#define __xkey_sizeof(xk)       (XKEY_HEAD_SIZE + (xk)->b[0] + 1)


typedef struct xmaker {
    uint64_t wpos, rpos, len;
    uint8_t *key;
    xline_ptr val;
    uint8_t *head;
}*xmaker_ptr;


static inline uint64_t xkey_fill(xkey_ptr xkey, const char *str, size_t len)
{
    mcopy(xkey->b + XKEY_HEAD_SIZE, str, len);
    xkey->b[len + 1] = '\0';
    xkey->b[0] = len + 1;
    return __xkey_sizeof(xkey);
}

static inline void xline_maker_free(xmaker_ptr maker)
{
    if (maker){
        if (maker->head){
            free(maker->head);
        }
        free(maker);
    }
}

static inline xmaker_ptr xline_maker_create(uint64_t len)
{
    xmaker_ptr maker = (xmaker_ptr)malloc(sizeof(struct xmaker));
    if (len < XLINE_SIZE){
        len = XLINE_SIZE;
    }
    maker->wpos = XLINE_SIZE;
    maker->len = len;
    maker->head = (uint8_t*) malloc(maker->len);
}

static inline void xline_maker_reset(xmaker_ptr maker)
{
    maker->wpos = maker->rpos = 0;
    maker->key = NULL;
    maker->val = NULL;
}

static inline uint64_t xline_maker_hold(xmaker_ptr maker, const char *key)
{
    maker->key = maker->head + maker->wpos;
    maker->key[0] = slength(key) + 1;
    mcopy(maker->key + 1, key, maker->key[0]);
    __xlogd("xline_maker_hold --------===================================----------------------=========================================:(%u)%s %s\n", maker->key[0], maker->key, key);
    maker->wpos += 1 + maker->key[0];
    uint64_t pos = maker->wpos;
    maker->wpos += XLINE_SIZE;
    return pos;
}

static inline void xline_maker_update(xmaker_ptr maker, uint64_t pos)
{
    uint64_t len = maker->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(maker->head + pos)) = __s2l(len, XLINE_TYPE_TREE);
}

static inline void xline_append_object(xmaker_ptr maker, const char *key, size_t keylen, const void *val, size_t size, uint8_t flag)
{
    __xlogd("xline_append_object enter >>>>-------------------------> key: %s  %lu %lu  %lu\n", key, maker->len, size, maker->wpos);
    __xlogd("xline_append_object len - pos: %ld    size: %ld\n", (int64_t)(maker->len - maker->wpos), (int64_t)(XKEY_HEAD_SIZE + keylen + XLINE_SIZE + size));
    maker->rpos = (XKEY_HEAD_SIZE + keylen + XLINE_SIZE + size);
    if ((int64_t)(maker->len - maker->wpos) < maker->rpos){
        maker->len += (maker->len + maker->rpos);
        __xlogd("realloc xmaker %lu\n", maker->len);
        maker->key = (uint8_t*)malloc(maker->len);
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
    __xlogd("xline_append_object exit >>>>-------------------------> %lu\n", maker->wpos);
}

static inline void xline_add_text(xmaker_ptr maker, const char *key, const char *text, uint64_t size)
{
    xline_append_object(maker, key, slength(key), text, size, XLINE_TYPE_STR);
}

static inline void xline_add_map(xmaker_ptr maker, const char *key, xmaker_ptr xmap)
{
    xline_append_object(maker, key, slength(key), xmap->head, xmap->wpos, XLINE_TYPE_TREE);
}

static inline void xline_add_list(xmaker_ptr maker, const char *key, xmaker_ptr xlist)
{
    xline_append_object(maker, key, slength(key), xlist->head, xlist->wpos, XLINE_TYPE_LIST);
}

static inline void xline_add_binary(xmaker_ptr maker, const char *key, const void *val, uint64_t size)
{
    xline_append_object(maker, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline void xline_append_number(xmaker_ptr maker, const char *key, size_t keylen, struct xline val)
{
    __xlogd("xline_append_number enter >>>>-------------------------> key: %s  %lu %lu  %lu\n", key, maker->len, __sizeof_line(maker->val), maker->wpos);
    // 最小长度 = 根 xline 的头 (XLINE_SIZE) + key 长度 (keylen + XKEY_HEAD_SIZE) + value 长度 (XLINE_SIZE)
    __xlogd("xline_append_number len -pos: %ld size: %ld\n", (int64_t)(maker->len - maker->wpos), (int64_t)(XKEY_HEAD_SIZE + keylen + XLINE_SIZE));
    maker->rpos = (XKEY_HEAD_SIZE + keylen + XLINE_SIZE);
    if ((int64_t)(maker->len - maker->wpos) < maker->rpos){
        maker->len += (maker->len + maker->rpos);
        __xlogd("realloc xmaker %lu\n", maker->len);
        maker->key = (uint8_t*)malloc(maker->len);
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
    // *((xline_ptr)(maker->head)) = __s2l(maker->wpos, XLINE_TYPE_TREE);
    maker->rpos = maker->wpos - XLINE_SIZE;
    *((xline_ptr)(maker->head)) = __s2l(maker->rpos, XLINE_TYPE_TREE);    
    __xlogd("xline_append_number exit >>>>-------------------------> %lu\n", maker->wpos);
}

static inline void xline_add_int(xmaker_ptr maker, const char *key, int64_t n64)
{
    xline_append_number(maker, key, slength(key), __n2l(n64));
}

static inline void xline_add_uint(xmaker_ptr maker, const char *key, uint64_t u64)
{
    xline_append_number(maker, key, slength(key), __n2l(u64));
}

static inline void xline_add_float(xmaker_ptr maker, const char *key, double f64)
{
    xline_append_number(maker, key, slength(key), __f2l(f64));
}

static inline void xline_add_ptr(xmaker_ptr maker, const char *key, void *p)
{
    int64_t n = (int64_t)(p);
    xline_append_number(maker, key, slength(key), __n2l(n));
}

static inline struct xmaker xline_parse(xline_ptr xmap)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    __xlogd("xline_parse line len       >>>>-------------------------> %lu\n", __sizeof_data(xmap));
    struct xmaker maker = {0};
    maker.rpos = 0;
    maker.len = __sizeof_data(xmap);
    maker.head = xmap->b + XLINE_SIZE;
    return maker;
}

static inline xline_ptr xline_next(xmaker_ptr maker)
{
    __xlogd("xline_next     line len     enter  >>>>-------------------------> rpos: %lu len: %lu\n", maker->rpos, maker->len);
    if (maker->rpos < maker->len){
        maker->key = maker->head + maker->rpos;
        maker->rpos += 1 + maker->key[0];
        maker->key++;
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(maker->val);
        __xlogd("xline_next     line len       >>>>-------------------------> lsize: %lu rpos: %lu len: %lu\n", __sizeof_line(maker->val), maker->rpos, maker->len);
        return maker->val;
    }
    __xlogd("xline_next     line len      exit >>>>-------------------------> rpos: %lu len: %lu\n", maker->rpos, maker->len);
    return NULL;
}

// static inline xline_ptr xline_find(xmaker_ptr maker, const char *key)
// {
//     maker->rpos = 0;
//     while (maker->rpos < maker->wpos) {
//         maker->key = (xkey_ptr)(maker->head + maker->rpos);
//         maker->rpos += __xkey_sizeof(maker->key);
//         maker->val = (xline_ptr)(maker->head + maker->rpos);
//         // printf("get key=%s xkey=%s\n", key, &xobj->key[1]);
//         if (slength(key) == maker->key->b[0]
//             && mcompare(key, &maker->key->b[1], maker->key->b[0]) == 0){
//             return maker->val;
//         }
//         maker->rpos += __xline_sizeof(maker->val);
//     }
//     return NULL;
// }

static inline xline_ptr xline_find(xmaker_ptr maker, const char *key)
{
    __xlogd("xline_find() rpos: %lu len: %lu\n", maker->rpos, maker->len);
    maker->wpos = maker->rpos;

    while (maker->rpos < maker->len) {
        maker->key = maker->head + maker->rpos;
        maker->rpos += 1 + maker->key[0];
        __xlogd("xline_find() 1 %s\n", maker->key + 1);
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
        __xlogd("xline_find() 2 %s\n", maker->key + 1);
        maker->rpos += 1 + maker->key[0];
        maker->val = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(maker->val);
        if (slength(key) + 1 == maker->key[0]
            && mcompare(key, maker->key + 1, maker->key[0]) == 0){
            return maker->val;
        }
    }

    maker->rpos = 0;
    __xlogd("xline_find() cannot fond ------------------------\n");

    return NULL;
}

static inline int64_t xline_find_int(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2i(val);
    }
    return ENDSYM;
}

static inline uint64_t xline_find_uint(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2u(val);
    }
    return ENDSYM;
}

static inline double xline_find_float(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return __l2f(val);
    }
    return ENDSYM;
}

static inline void* xline_find_ptr(xmaker_ptr maker, const char *key)
{
    xline_ptr val = xline_find(maker, key);
    if (val){
        return (void *)(__l2u(val));
    }
    return NULL;
}

static inline void xline_list_append(xmaker_ptr maker, xline_ptr x)
{
    uint64_t len = __sizeof_line(x);
    // if ((maker->len - maker->wpos) < len){
    //     maker->len += (maker->len + len);
    //     maker->key = (xkey_ptr)malloc(maker->len);
    //     mcopy(maker->key->b, maker->head, maker->wpos);
    //     free(maker->head);
    //     maker->head = (uint8_t*)maker->key;
    // }
    
    mcopy(maker->head + maker->wpos, x->b, len);
    maker->wpos += len;
    *((xline_ptr)(maker->head)) = __s2l(maker->wpos, XLINE_TYPE_LIST);
}

static inline struct xmaker xline_list_parse(xline_ptr xlist)
{
    // parse 生成的 maker 是在栈上分配的，离开作用域，会自动释放
    struct xmaker maker = {0};
    // maker.addr = NULL;
    // maker.key = NULL;
    // maker.len = 0;
    // maker.rpos = 0;
    maker.head = (uint8_t*)__l2data(xlist);
    maker.val = (xline_ptr)maker.head;
    // 读取 xlist 的长度，然后设置 wpos
    maker.wpos = __sizeof_data(xlist);
    return maker;
}

static inline xline_ptr xline_list_next(xmaker_ptr maker)
{
    if (maker->wpos - maker->rpos > 0){
        xline_ptr x = (xline_ptr)(maker->head + maker->rpos);
        maker->rpos += __sizeof_line(x);
        return x;
    }
    return NULL;
}

#endif //__XLINE_H__