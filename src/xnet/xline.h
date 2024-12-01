#ifndef __XLINE_H__
#define __XLINE_H__

#include "xmalloc.h"

enum {
    XLINE_TYPE_INT = 0x01, //整数
    XLINE_TYPE_UINT = 0x02, //自然数
    XLINE_TYPE_FLOAT = 0x04, //实数
    XLINE_TYPE_STR = 0x08, //字符串
    XLINE_TYPE_BIN = 0x10, //二进制数据
    XLINE_TYPE_OBJ = 0x20, //键值对
    XLINE_TYPE_LIST = 0x40 //列表
};

//xline 静态分配的最大长度（ 64bit 数的长度为 8 字节，加 1 字节头部标志位 ）
#define XLINE_SIZE          9


typedef struct xline {
    uint8_t b[XLINE_SIZE];
}xline_t;

typedef xline_t* xline_ptr;

union float64 {
    double f;
    char b[8];
};

#ifdef __LITTLE_ENDIAN__

#define __xl_n2b(n, type) \
        (struct xline){ \
            (type), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __xl_i2b(n) __xl_n2b(n, XLINE_TYPE_INT)
#define __xl_u2b(n) __xl_n2b(n, XLINE_TYPE_UINT)
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_FLOAT)


#define __xl_b2n(l, type) \
        ( (type)(l)->b[8] << 56 | (type)(l)->b[7] << 48 | (type)(l)->b[6] << 40 | (type)(l)->b[5] << 32 \
        | (type)(l)->b[4] << 24 | (type)(l)->b[3] << 16 | (type)(l)->b[2] << 8 | (type)(l)->b[1] )

static inline double __xl_b2float(xline_ptr l)
{
    union float64 f;
    f.b[0] = l->b[1];
    f.b[1] = l->b[2];
    f.b[2] = l->b[3];
    f.b[3] = l->b[4];
    f.b[4] = l->b[5];
    f.b[5] = l->b[6];
    f.b[6] = l->b[7];
    f.b[7] = l->b[8];
    return f.f;
}

#else //__LITTLE_ENDIAN__

#define __xl_n2b(n, type) \
        (struct xline){ \
            type, \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __xl_i2b(n) __xl_n2b(n, XLINE_TYPE_INT)
#define __xl_u2b(n) __xl_n2b(n, XLINE_TYPE_UINT)
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_FLOAT)


#define __xl_b2n(l, type) \
        ( (type)(l)->b[1] << 56 | (type)(l)->b[2] << 48 | (type)(l)->b[3] << 40 | (type)(l)->b[4] << 32 \
        | (type)(l)->b[5] << 24 | (type)(l)->b[6] << 16 | (type)(l)->b[7] << 8 | (type)(l)->b[8] )

static inline double __xl_b2float(xl_ptr l)
{
    union float64 f;
    f.b[0] = l->b[8];
    f.b[1] = l->b[7];
    f.b[2] = l->b[6];
    f.b[3] = l->b[5];
    f.b[4] = l->b[4];
    f.b[5] = l->b[3];
    f.b[6] = l->b[2];
    f.b[7] = l->b[1];
    return f.f;
}

#endif //__LITTLE_ENDIAN__

#define __xl_b2i(l)                 (__xl_b2n(l, int64_t))
#define __xl_b2u(l)                 (__xl_b2n(l, uint64_t))
#define __xl_b2f(l)                 (__xl_b2float(l))
#define __xl_b2o(l)                 ((void*)&(l)->b[XLINE_SIZE])

#define __xl_typeis_int(l)          ((l)->b[0] == XLINE_TYPE_INT)
#define __xl_typeis_uint(l)         ((l)->b[0] == XLINE_TYPE_UINT)
#define __xl_typeis_float(l)        ((l)->b[0] == XLINE_TYPE_FLOAT)
#define __xl_typeis_obj(l)          ((l)->b[0] > XLINE_TYPE_FLOAT)
#define __xl_typeis_str(l)          ((l)->b[0] == XLINE_TYPE_STR)
#define __xl_typeis_bin(l)          ((l)->b[0] == XLINE_TYPE_BIN)
#define __xl_typeis_xlkv(l)         ((l)->b[0] == XLINE_TYPE_OBJ)
#define __xl_typeis_list(l)         ((l)->b[0] == XLINE_TYPE_LIST)

#define __xl_sizeof_head(l)         (__xl_typeis_obj(l) ? XLINE_SIZE : 1)
#define __xl_sizeof_body(l)         (__xl_typeis_obj(l) ? __xl_b2u((l)) : 8)
#define __xl_sizeof_line(l)         (__xl_typeis_obj(l) ? __xl_b2u((l)) + XLINE_SIZE : XLINE_SIZE)


// #define XLINE_MAKER_SIZE            (1024 * 16)
#define XLINE_MAKER_SIZE            (16)

#define XLMSG_FLAG_RECV             0x00
#define XLMSG_FLAG_SEND             0x01
#define XLMSG_FLAG_CONNECT          0x02
#define XLMSG_FLAG_DISCONNECT       0x04

typedef struct xlinekv {
    uint8_t flag;
    __atom_size ref;
    void *cb;
    struct xchanne *channel;
    struct xchannel_ctx *ctx;
    struct xlinekv *prev, *next;
    uint64_t wpos, spos, rpos, range, size;
    uint8_t *key;
    uint8_t *data;
    xline_t head;
}xlinekv_t;

typedef xlinekv_t* xlmsg_ptr;


static inline xlmsg_ptr xl_creator(uint64_t size)
{
    xlmsg_ptr obj = (xlmsg_ptr)malloc(sizeof(xlinekv_t) + XLINE_SIZE + size);
    __xcheck(obj == NULL);
    obj->ref = 1;
    obj->size = size;
    obj->wpos = 0;
    obj->rpos = 0;
    obj->cb = NULL;
    obj->ctx = NULL;
    obj->prev = obj->next = NULL;
    obj->data = obj->head.b + XLINE_SIZE;
    return obj;
XClean:
    return NULL;
}

static inline xlmsg_ptr xl_maker()
{
    return xl_creator(XLINE_MAKER_SIZE);
}

static inline void xl_fixed(xlmsg_ptr msg)
{
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
}

static inline void xl_free(xlmsg_ptr msg)
{
    if (__atom_sub(msg->ref, 1) == 0){
        free(msg);
    }
}

static inline uint64_t xl_obj_begin(xlmsg_ptr msg, const char *key)
{
    msg->key = msg->data + msg->wpos;
    msg->key[0] = slength(key) + 1;
    mcopy(msg->key + 1, key, msg->key[0]);
    msg->wpos += (msg->key[0] + 1 + XLINE_SIZE);
    // obj 的 val 是一个 xline，xline 有 9 个字节的头
    // wpos 指向 val 里面的第一个元素的 key，所以要跳过 xline 头的 9 个字节
    // 返回的 pos 指向 xline 的头，因为 save 的时候，要更新 xline 的长度
    return msg->wpos - XLINE_SIZE;
}

static inline void xl_obj_end(xlmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(msg->data + pos)) = __xl_n2b(len, XLINE_TYPE_OBJ);
}

static inline uint64_t xl_list_begin(xlmsg_ptr msg, const char *key)
{
    return xl_obj_begin(msg, key);
}

static inline void xl_list_end(xlmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->wpos - pos - XLINE_SIZE;
    *((xline_ptr)(msg->data + pos)) = __xl_n2b(len, XLINE_TYPE_LIST);
}

static inline uint64_t xl_list_obj_begin(xlmsg_ptr msg)
{
    msg->wpos += XLINE_SIZE;
    return msg->wpos - XLINE_SIZE;
}

static inline void xl_list_obj_end(xlmsg_ptr msg, uint64_t pos)
{
    return xl_obj_end(msg, pos);
}

#define __xl_fill_key(msg, key, keylen) \
    do { \
        if (keylen > 253) keylen = 253; \
        msg->key = msg->data + msg->wpos; \
        msg->key[0] = keylen + 1; \
        mcopy(msg->key + 1, key, keylen); \
        *(msg->key + msg->key[0]) = '\0'; \
        msg->wpos += (msg->key[0] + 1); \
    }while(0)

#if 1
#define __xl_realloc(msg, msgpptr, keylen, vallen) \
    do { \
        uint64_t newlen = (msg->size * 2) + (keylen + 2 + XLINE_SIZE + vallen); \
        msg = (xlinekv_t*)malloc(sizeof(xlinekv_t) + newlen); \
        __xcheck(msg == NULL); \
        mcopy(msg, (*msgpptr), sizeof(xlinekv_t) + (*msgpptr)->wpos); \
        msg->data = msg->head.b + XLINE_SIZE; \
        msg->size = newlen; \
        free((*msgpptr)); \
        *msgpptr = msg; \
    }while(0)
#else
#define __xl_realloc(msg, msgpptr, keylen, vallen) \
    do {}while(0)
#endif

static inline uint64_t xl_add_word(xlinekv_t **msgpptr, const char *key, const char *word)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    uint64_t wordlen = slength(word) + 1;
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE + wordlen)){
        __xl_realloc(msg, msgpptr, keylen, wordlen);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_n2b(wordlen, XLINE_TYPE_STR);
    msg->wpos += XLINE_SIZE;
    mcopy(msg->data + msg->wpos, word, wordlen);
    msg->wpos += wordlen;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_str(xlinekv_t **msgpptr, const char *key, const char *str, size_t len)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE + len)){
        __xl_realloc(msg, msgpptr, keylen, len);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_n2b(len, XLINE_TYPE_STR);
    msg->wpos += XLINE_SIZE;
    mcopy(msg->data + msg->wpos, str, len);
    msg->wpos += len;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_bin(xlinekv_t **msgpptr, const char *key, const void *bin, uint64_t size)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE + size)){
        __xl_realloc(msg, msgpptr, keylen, size);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_n2b(size, XLINE_TYPE_BIN);
    msg->wpos += XLINE_SIZE;
    if (bin != NULL){
        mcopy(msg->data + msg->wpos, bin, size);
    }
    msg->wpos += size;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_obj(xlinekv_t **msgpptr, const char *key, xline_ptr xl)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    uint64_t size = __xl_sizeof_line(xl);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + size)){
        __xl_realloc(msg, msgpptr, keylen, size);
    }
    __xl_fill_key(msg, key, keylen);
    if (xl != NULL){
        mcopy(msg->data + msg->wpos, xl->b, size);
    }
    msg->wpos += size;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_int(xlinekv_t **msgpptr, const char *key, int64_t i64)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE)){
        __xl_realloc(msg, msgpptr, keylen, 0);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_i2b(i64);
    msg->wpos += XLINE_SIZE;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_uint(xlinekv_t **msgpptr, const char *key, uint64_t u64)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE)){
        __xl_realloc(msg, msgpptr, keylen, 0);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_u2b(u64);
    msg->wpos += XLINE_SIZE;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_float(xlinekv_t **msgpptr, const char *key, double f64)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE)){
        __xl_realloc(msg, msgpptr, keylen, 0);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_f2b(f64);
    msg->wpos += XLINE_SIZE;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_ptr(xlinekv_t **msgpptr, const char *key, void *ptr)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t u64 = (uint64_t)(ptr);
    uint64_t keylen = slength(key);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < (keylen + 2 + XLINE_SIZE)){
        __xl_realloc(msg, msgpptr, keylen, 0);
    }
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->data + msg->wpos)) = __xl_u2b(u64);
    msg->wpos += XLINE_SIZE;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline xline_ptr xl_next(xlmsg_ptr msg)
{
    if (msg->rpos < msg->wpos){
        msg->key = msg->data + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        msg->key++;
        xline_ptr val = (xline_ptr)(msg->data + msg->rpos);
        msg->rpos += __xl_sizeof_line(val);
        return val;
    }
    return NULL;
}

static inline xline_ptr xl_find(xlmsg_ptr msg, const char *key)
{
    xline_ptr val = NULL;
    uint64_t rpos = msg->rpos;

    while (msg->rpos < msg->wpos) {
        msg->key = msg->data + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        val = (xline_ptr)(msg->data + msg->rpos);
        msg->rpos += __xl_sizeof_line(val);
        if (slength(key) + 1 == msg->key[0]
            && mcompare(key, msg->key + 1, msg->key[0]) == 0){
            msg->key++;
            return val;
        }
    }

    msg->rpos = 0;

    while (msg->rpos < rpos) {
        msg->key = msg->data + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        val = (xline_ptr)(msg->data + msg->rpos);
        msg->rpos += __xl_sizeof_line(val);
        if (slength(key) + 1 == msg->key[0]
            && mcompare(key, msg->key + 1, msg->key[0]) == 0){
            msg->key++;
            return val;
        }
    }

    return NULL;
}

static inline int64_t xl_find_int(xlmsg_ptr msg, const char *key)
{
    xline_ptr val = xl_find(msg, key);
    if (val){
        return __xl_b2i(val);
    }
    return EENDED;
}

static inline uint64_t xl_find_uint(xlmsg_ptr obj, const char *key)
{
    xline_ptr val = xl_find(obj, key);
    if (val){
        return __xl_b2u(val);
    }
    return EENDED;
}

static inline double xl_find_float(xlmsg_ptr msg, const char *key)
{
    xline_ptr val = xl_find(msg, key);
    if (val){
        return __xl_b2f(val);
    }
    return (double)EENDED;
}

static inline char* xl_find_word(xlmsg_ptr msg, const char *key)
{
    xline_ptr val = xl_find(msg, key);
    if (val){
        return (char*)__xl_b2o(val);
    }
    return NULL;
}

static inline void* xl_find_ptr(xlmsg_ptr msg, const char *key)
{
    xline_ptr val = xl_find(msg, key);
    if (val){
        return (void *)(__xl_b2u(val));
    }
    return NULL;
}

static inline uint64_t xl_list_append(xlinekv_t **msgpptr, xline_ptr xl)
{
    xlinekv_t *msg = *msgpptr;
    uint64_t size = __xl_sizeof_line(xl);
    if ((msg->size - (msg->wpos + XLINE_SIZE)) < size){
        __xl_realloc(msg, msgpptr, size, 0);
    }
    if (xl != NULL){
        mcopy(msg->data + msg->wpos, xl->b, size);
    }
    msg->wpos += size;
    msg->head = __xl_n2b(msg->wpos, XLINE_TYPE_LIST);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline xline_ptr xl_list_next(xlmsg_ptr msg)
{
    if (msg->rpos < msg->wpos){
        xline_ptr ptr = (xline_ptr)(msg->data + msg->rpos);
        msg->rpos += __xl_sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static xlinekv_t xl_parser(xline_ptr xl)
{
    xlinekv_t parser = {0};
    parser.rpos = 0;
    parser.wpos = __xl_sizeof_body(xl);
    parser.data = __xl_b2o(xl);
    return parser;
}

static inline void xl_format(xline_ptr xl, const char *key, int depth, char *buf, uint64_t *pos, uint64_t size)
{
    xlinekv_t parser = xl_parser(xl);

    int len = slength(key);

    if (depth == 1){
        *pos += __xapi->snprintf(buf + *pos, size - *pos, "{\n", (depth) * 4, key);
    }else {
        if (len == 0){
            *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s{\n", (depth) * 4, "");
        }else {
            *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth) * 4, key);
        }
    }

    if (__xl_typeis_xlkv(xl)){

        while ((xl = xl_next(&parser)) != NULL){

            if (__xl_typeis_int(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %ld,\n", (depth + 1) * 4, parser.key, __xl_b2i(xl));

            }else if (__xl_typeis_uint(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lu,\n", (depth + 1) * 4, parser.key, __xl_b2i(xl));

            }else if (__xl_typeis_float(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lf,\n", (depth + 1) * 4, parser.key, __xl_b2f(xl));

            }else if (__xl_typeis_str(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, __xl_b2o(xl));

            }else if (__xl_typeis_bin(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: size[%lu],\n", (depth + 1) * 4, parser.key, __xl_sizeof_body(xl));

            }else if (__xl_typeis_xlkv(xl)){
                xl_format(xl, (const char*)parser.key, depth + 1, buf, pos, size);

            }else if (__xl_typeis_list(xl)){

                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
                xlinekv_t xllist = xl_parser(xl);

                while ((xl = xl_list_next(&xllist)) != NULL){
                    if (__xl_typeis_int(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", depth * 4, __xl_b2i(xl));
                    }else if (__xl_typeis_uint(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", depth * 4, __xl_b2u(xl));
                    }else if (__xl_typeis_float(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", depth * 4, __xl_b2f(xl));
                    }else if (__xl_typeis_xlkv(xl)){
                        xl_format(xl, "", depth + 1, buf, pos, size);
                    }
                }
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s},\n", depth * 4, "");

            }
        }

    }else if (__xl_typeis_list(xl)){

        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
        xlinekv_t xllist = xl_parser(xl);

        while ((xl = xl_list_next(&xllist)) != NULL){
            if (__xl_typeis_int(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", depth * 4, __xl_b2i(xl));
            }else if (__xl_typeis_uint(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", depth * 4, __xl_b2u(xl));
            }else if (__xl_typeis_float(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", depth * 4, __xl_b2f(xl));
            }else if (__xl_typeis_xlkv(xl)){
                xl_format(xl, "", depth + 1, buf, pos, size);
            }
        }
        *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s},\n", depth * 4, "");
    }

    if (depth == 1){
        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s}\n", (depth - 1) * 4, "");
    }else {
        if (len == 0){
            *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s},\n", (depth) * 4, "");
        }else {
            *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s},\n", (depth) * 4, "");
        }   
    }
}


#define xl_printf(xl) \
    do { \
        uint64_t pos = 0; \
        uint64_t len = __xl_sizeof_line(xl) * 2; \
        char buf[len]; \
        xl_format((xl), "", 1, buf, &pos, len); \
        __xlogi("xline len[%lu] >>>>----------->\n%s", pos, buf); \
    }while(0)



static void xl_test()
{
    xlinekv_t *xobj, *xlist;
    xlinekv_t *msg = xl_maker();
    char bin[1024];
    xl_add_word(&msg, "word", "hello word");
    xl_add_str(&msg, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&msg, "bin", bin, sizeof(bin));
    xl_add_int(&msg, "int", -123456789);
    xl_add_uint(&msg, "uint", 123456789);
    xl_add_float(&msg, "float", 123456789.123);
    xl_printf(&msg->head);

    uint64_t pos = xl_obj_begin(msg, "obj");
    xl_add_word(&msg, "word", "hello word");
    xl_add_str(&msg, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&msg, "bin", bin, sizeof(bin));
    xl_add_int(&msg, "int", -123456789);
    xl_add_uint(&msg, "uint", 123456789);
    xl_add_float(&msg, "float", 123456789.123);
    xl_obj_end(msg, pos);
    xl_printf(&msg->head);

    pos = xl_list_begin(msg, "list");
    for (int i = 0; i < 3; ++i){
        uint64_t opos = xl_list_obj_begin(msg);
            xl_add_word(&msg, "word", "hello word");
            xl_add_str(&msg, "str", "hello string", slength("hello string")+1);
            xl_add_bin(&msg, "bin", bin, sizeof(bin));
            xl_add_int(&msg, "int", -123456789);
            xl_add_uint(&msg, "uint", 123456789);
            xl_add_float(&msg, "float", 123456789.123);
        xl_list_obj_end(msg, opos);
    }
    xl_list_end(msg, pos);
    xl_printf(&msg->head);

    xobj = xl_maker();
    xl_add_word(&xobj, "word", "hello word");
    xl_add_str(&xobj, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&xobj, "bin", bin, sizeof(bin));
    xl_add_int(&xobj, "int", -123456789);
    xl_add_uint(&xobj, "uint", 123456789);
    xl_add_float(&xobj, "float", 123456789.123);
    xl_add_obj(&msg, "addobj", &xobj->head);
    
    xl_printf(&msg->head);

    xlist = xl_maker();
    for (int i = 0; i < 10; ++i){
        double f = i * 0.1f;
        xl_list_append(&xlist, &__xl_f2b(f));
    }
    xl_printf(&xlist->head);

    xl_add_obj(&msg, "addlist", &xlist->head);
    
    xl_printf(&msg->head);

    xl_free(msg);
    xl_free(xobj);
    xl_free(xlist);
}


#endif //__XLINE_H__
