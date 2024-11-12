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
#define XLINE_STATIC_SIZE      9


typedef struct xline {
    uint8_t b[XLINE_STATIC_SIZE];
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

#define __xl_b2i(l)                 __xl_b2n(l, int64_t)
#define __xl_b2u(l)                 __xl_b2n(l, uint64_t)
#define __xl_b2f(l)                 __xl_b2float(l)
#define __xl_b2o(l)                 ((void*)&(l)->b[XLINE_STATIC_SIZE])

#define __xl_typeis_int(l)          ((l)->b[0] == XLINE_TYPE_INT)
#define __xl_typeis_uint(l)         ((l)->b[0] == XLINE_TYPE_UINT)
#define __xl_typeis_float(l)        ((l)->b[0] == XLINE_TYPE_FLOAT)
#define __xl_typeis_obj(l)          ((l)->b[0] > XLINE_TYPE_FLOAT)
#define __xl_typeis_str(l)          ((l)->b[0] == XLINE_TYPE_STR)
#define __xl_typeis_bin(l)          ((l)->b[0] == XLINE_TYPE_BIN)
#define __xl_typeis_xlkv(l)         ((l)->b[0] == XLINE_TYPE_OBJ)
#define __xl_typeis_list(l)         ((l)->b[0] == XLINE_TYPE_LIST)

#define __xl_sizeof_head(l)         __xl_typeis_obj(l) ? XLINE_SIZE : 1
#define __xl_sizeof_body(l)         __xl_typeis_obj(l) ? __xl_b2u((l)) : 8
#define __xl_sizeof_line(l)         __xl_typeis_obj(l) ? __xl_b2u((l)) + XLINE_STATIC_SIZE : XLINE_STATIC_SIZE


#define XLINE_DEFAULT_SIZE          (1024 * 8)

typedef struct xlmsg {
    __atom_size ref;
    void *ctx;
    uint64_t wpos, rpos, size;
    uint8_t *key;
    uint8_t *body;
    xline_t line;
}xlmsg_t;

typedef xlmsg_t* xlmsg_ptr;


static inline xlmsg_ptr xl_create(uint64_t size)
{
    xlmsg_ptr obj = (xlmsg_ptr)malloc(sizeof(xlmsg_t) + size);
    if (obj){
        obj->ref = 1;
        obj->size = size;
        obj->wpos = 0;
        obj->rpos = 0;
        obj->body = obj->line.b + XLINE_STATIC_SIZE;
    }
    return obj;
}

static inline xlmsg_ptr xl_maker()
{
    return xl_create(XLINE_DEFAULT_SIZE);
}

static inline void xl_fixed(xlmsg_ptr msg)
{
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
}

static inline void xl_free(xlmsg_ptr msg)
{
    if (__atom_sub(msg->ref, 1) == 0){
        free(msg);
    }
}

static inline uint64_t xl_hold_obj(xlmsg_ptr msg, const char *key)
{
    msg->key = msg->body + msg->wpos;
    msg->key[0] = slength(key) + 1;
    mcopy(msg->key + 1, key, msg->key[0]);
    msg->wpos += (msg->key[0] + 1 + XLINE_STATIC_SIZE);
    // obj 的 val 是一个 xline，xline 有 9 个字节的头
    // wpos 指向 val 里面的第一个元素的 key，所以要跳过 xline 头的 9 个字节
    // 返回的 pos 指向 xline 的头，因为 save 的时候，要更新 xline 的长度
    return msg->wpos - XLINE_STATIC_SIZE;
}

static inline void xl_fixed_obj(xlmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->wpos - pos - XLINE_STATIC_SIZE;
    *((xline_ptr)(msg->body + pos)) = __xl_n2b(len, XLINE_TYPE_OBJ);
}

static inline uint64_t xl_hold_list(xlmsg_ptr msg, const char *key)
{
    return xl_hold_obj(msg, key);
}

static inline void xl_fixed_list(xlmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->wpos - pos - XLINE_STATIC_SIZE;
    *((xline_ptr)(msg->body + pos)) = __xl_n2b(len, XLINE_TYPE_LIST);
}

static inline uint64_t xl_list_hold_obj(xlmsg_ptr msg)
{
    msg->wpos += XLINE_STATIC_SIZE;
    return msg->wpos - XLINE_STATIC_SIZE;
}

static inline void xl_list_fixed_obj(xlmsg_ptr msg, uint64_t pos)
{
    return xl_fixed_obj(msg, pos);
}

#define __xl_fill_key(msg, key, keylen) \
    if (keylen > 253) keylen = 253; \
    msg->key = msg->body + msg->wpos; \
    msg->key[0] = keylen + 1; \
    mcopy(msg->key + 1, key, keylen); \
    *(msg->key + msg->key[0]) = '\0'; \
    msg->wpos += (msg->key[0] + 1)

static inline uint64_t xl_add_word(xlmsg_ptr msg, const char *key, const char *word)
{
    uint64_t keylen = slength(key);
    uint64_t wordlen = slength(word) + 1;
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE + wordlen));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_n2b(wordlen, XLINE_TYPE_STR);
    msg->wpos += XLINE_STATIC_SIZE;
    mcopy(msg->body + msg->wpos, word, wordlen);
    msg->wpos += wordlen;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_str(xlmsg_ptr msg, const char *key, const char *str, size_t len)
{
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE + len));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_n2b(len, XLINE_TYPE_STR);
    msg->wpos += XLINE_STATIC_SIZE;
    mcopy(msg->body + msg->wpos, str, len);
    msg->wpos += len;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_bin(xlmsg_ptr msg, const char *key, const void *bin, uint64_t size)
{
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE + size));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_n2b(size, XLINE_TYPE_BIN);
    msg->wpos += XLINE_STATIC_SIZE;
    if (bin != NULL){
        mcopy(msg->body + msg->wpos, bin, size);
    }
    msg->wpos += size;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_obj(xlmsg_ptr msg, const char *key, xline_ptr xl)
{
    uint64_t keylen = slength(key);
    uint64_t size = __xl_sizeof_line(xl);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE + size));
    __xl_fill_key(msg, key, keylen);
    if (xl != NULL){
        mcopy(msg->body + msg->wpos, xl->b, size);
    }
    msg->wpos += size;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_int(xlmsg_ptr msg, const char *key, int64_t i64)
{
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_i2b(i64);
    msg->wpos += XLINE_STATIC_SIZE;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_uint(xlmsg_ptr msg, const char *key, uint64_t u64)
{
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_u2b(u64);
    msg->wpos += XLINE_STATIC_SIZE;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_float(xlmsg_ptr msg, const char *key, double f64)
{
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_f2b(f64);
    msg->wpos += XLINE_STATIC_SIZE;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_ptr(xlmsg_ptr msg, const char *key, void *ptr)
{
    uint64_t u64 = (uint64_t)(ptr);
    uint64_t keylen = slength(key);
    __xcheck((int64_t)(msg->size - msg->wpos) < (keylen + 2 + XLINE_STATIC_SIZE));
    __xl_fill_key(msg, key, keylen);
    *((xline_ptr)(msg->body + msg->wpos)) = __xl_u2b(u64);
    msg->wpos += XLINE_STATIC_SIZE;
    msg->line = __xl_n2b(msg->wpos, XLINE_TYPE_OBJ);
    return msg->wpos;
XClean:
    return EENDED;
}

static inline xline_ptr xl_next(xlmsg_ptr msg)
{
    if (msg->rpos < msg->wpos){
        msg->key = msg->body + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        msg->key++;
        xline_ptr val = (xline_ptr)(msg->body + msg->rpos);
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
        msg->key = msg->body + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        val = (xline_ptr)(msg->body + msg->rpos);
        msg->rpos += __xl_sizeof_line(val);
        if (slength(key) + 1 == msg->key[0]
            && mcompare(key, msg->key + 1, msg->key[0]) == 0){
            msg->key++;
            return val;
        }
    }

    msg->rpos = 0;

    while (msg->rpos < rpos) {
        msg->key = msg->body + msg->rpos;
        msg->rpos += (msg->key[0] + 1);
        val = (xline_ptr)(msg->body + msg->rpos);
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

static inline uint64_t xl_list_append(xlmsg_ptr msg, xline_ptr xl)
{
    uint64_t size = __xl_sizeof_line(xl);
    __xcheck((int64_t)(msg->size - msg->wpos) < size);
    if (xl != NULL){
        mcopy(msg->body + msg->wpos, xl->b, size);
    }
    msg->wpos += size;
    return msg->wpos;
XClean:
    return EENDED;
}

static inline xline_ptr xl_list_next(xlmsg_ptr msg)
{
    if (msg->rpos < msg->wpos){
        xline_ptr ptr = (xline_ptr)(msg->body + msg->rpos);
        msg->rpos += __xl_sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static xlmsg_t xl_parser(xline_ptr xl)
{
    xlmsg_t parser = {0};
    parser.rpos = 0;
    parser.wpos = __xl_sizeof_body(xl);
    parser.body = __xl_b2o(xl);
    return parser;
}

static inline void xl_format(xline_ptr xl, const char *key, int depth, char *buf, uint64_t *pos, uint64_t size)
{
    xlmsg_t parser = xl_parser(xl);

    int len = slength(key);

    if (depth == 1){
        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth) * 4, key);
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
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, "__xl_typeis_bin");

            }else if (__xl_typeis_xlkv(xl)){
                xl_format(xl, (const char*)parser.key, depth + 1, buf, pos, size);

            }else if (__xl_typeis_list(xl)){

                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
                xlmsg_t xllist = xl_parser(xl);

                while ((xl = xl_list_next(&xllist)) != NULL){
                    if (__xl_typeis_int(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*d,\n", depth * 4, __xl_b2i(xl));
                    }else if (__xl_typeis_xlkv(xl)){
                        xl_format(xl, "", depth + 1, buf, pos, size);
                    }
                }
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s},\n", depth * 4, "");

            }
        }

    }else if (__xl_typeis_list(xl)){

        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
        xlmsg_t xllist = xl_parser(xl);

        while ((xl = xl_list_next(&xllist)) != NULL){
            if (__xl_typeis_int(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*d,\n", depth * 4, __xl_b2i(xl));
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
        char buf[XLINE_DEFAULT_SIZE] = {0}; \
        uint64_t pos = 0; \
        xl_format((xl), "root", 1, buf, &pos, XLINE_DEFAULT_SIZE); \
        __xlogi("len=%lu\n%s\n", pos, buf); \
    }while(0)




#endif //__XLINE_H__
