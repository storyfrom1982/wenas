#ifndef __XLINE_H__
#define __XLINE_H__

#include "xmalloc.h"
#include "xlib/avlmini.h"

enum {
    XLINE_TYPE_INT  = 0x01, //整数
    XLINE_TYPE_UINT = 0x02, //自然数
    XLINE_TYPE_REAL = 0x04, //实数
    XLINE_TYPE_STR  = 0x08, //字符串
    XLINE_TYPE_BIN  = 0x10, //二进制数据
    XLINE_TYPE_OBJ  = 0x20, //键值对
    XLINE_TYPE_LIST = 0x40  //列表
};

#define XDATA_SIZE      9

typedef struct xdata {
    uint8_t b[XDATA_SIZE];
}xdata_t;

union xreal {
    double f;
    char b[8];
};


#ifdef __LITTLE_ENDIAN__

#define __xl_n2b(n, type) \
        (struct xdata){ \
            (type), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __xl_i2b(n) __xl_n2b(n, XLINE_TYPE_INT)
#define __xl_u2b(n) __xl_n2b(n, XLINE_TYPE_UINT)
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_REAL)


#define __xl_b2n(d, type) \
        ( (type)(d)->b[8] << 56 | (type)(d)->b[7] << 48 | (type)(d)->b[6] << 40 | (type)(d)->b[5] << 32 \
        | (type)(d)->b[4] << 24 | (type)(d)->b[3] << 16 | (type)(d)->b[2] << 8 | (type)(d)->b[1] )

static inline double __xl_b2float(xdata_t *d)
{
    union xreal f;
    f.b[0] = d->b[1];
    f.b[1] = d->b[2];
    f.b[2] = d->b[3];
    f.b[3] = d->b[4];
    f.b[4] = d->b[5];
    f.b[5] = d->b[6];
    f.b[6] = d->b[7];
    f.b[7] = d->b[8];
    return f.f;
}

#else //__LITTLE_ENDIAN__

#define __xl_n2b(n, type) \
        (struct xdata){ \
            type, \
            (((char*)&(n))[7]), (((char*)&(n))[6]), \
            (((char*)&(n))[5]), (((char*)&(n))[4]), \
            (((char*)&(n))[3]), (((char*)&(n))[2]), \
            (((char*)&(n))[1]), (((char*)&(n))[0]) \
        }

#define __xl_i2b(n) __xl_n2b(n, XLINE_TYPE_INT)
#define __xl_u2b(n) __xl_n2b(n, XLINE_TYPE_UINT)
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_REAL)


#define __xl_b2n(d, type) \
        ( (type)(d)->b[1] << 56 | (type)(d)->b[2] << 48 | (type)(d)->b[3] << 40 | (type)(d)->b[4] << 32 \
        | (type)(d)->b[5] << 24 | (type)(d)->b[6] << 16 | (type)(d)->b[7] << 8 | (type)(d)->b[8] )

static inline double __xl_b2float(xdata_t *d)
{
    union xreal f;
    f.b[0] = d->b[8];
    f.b[1] = d->b[7];
    f.b[2] = d->b[6];
    f.b[3] = d->b[5];
    f.b[4] = d->b[4];
    f.b[5] = d->b[3];
    f.b[6] = d->b[2];
    f.b[7] = d->b[1];
    return f.f;
}

#endif //__LITTLE_ENDIAN__

#define __xl_b2i(l)                 (__xl_b2n(l, int64_t))
#define __xl_b2u(l)                 (__xl_b2n(l, uint64_t))
#define __xl_b2f(l)                 (__xl_b2float(l))
#define __xl_b2o(l)                 ((void*)&(l)->b[XDATA_SIZE])

#define __xl_typeis_int(l)          ((l)->b[0] == XLINE_TYPE_INT)
#define __xl_typeis_uint(l)         ((l)->b[0] == XLINE_TYPE_UINT)
#define __xl_typeis_real(l)         ((l)->b[0] == XLINE_TYPE_REAL)
#define __xl_typeis_num(l)          ((l)->b[0] <= XLINE_TYPE_REAL)

#define __xl_typeis_str(l)          ((l)->b[0] == XLINE_TYPE_STR)
#define __xl_typeis_bin(l)          ((l)->b[0] == XLINE_TYPE_BIN)
#define __xl_typeis_obj(l)          ((l)->b[0] == XLINE_TYPE_OBJ)
#define __xl_typeis_list(l)         ((l)->b[0] == XLINE_TYPE_LIST)

#define __xl_sizeof_head(l)         (__xl_typeis_num(l) ? 1 : XDATA_SIZE)
#define __xl_sizeof_body(l)         (__xl_typeis_num(l) ? 8 : __xl_b2u((l)))
#define __xl_sizeof_line(l)         (__xl_typeis_num(l) ? XDATA_SIZE : __xl_b2u((l)) + XDATA_SIZE)


// #define XLINE_MAKER_SIZE            (1024 * 16)
#define XLINE_MAKER_SIZE            (16)

#define XLMSG_FLAG_RECV             0x00
#define XLMSG_FLAG_SEND             0x01
#define XLMSG_FLAG_CONNECT          0x02
#define XLMSG_FLAG_DISCONNECT       0x04
#define XLMSG_FLAG_FINAL            0x08

typedef struct xline {
    __atom_size ref;
    uint8_t flag;
    void *cb, *ctx;
    uint64_t index;
    struct avl_node node;
    struct xline *prev, *next;
    uint64_t wpos, rpos, size;
    uint8_t *key;
    uint8_t *ptr;
    xdata_t data;
}xline_t;


static inline xline_t* xl_creator(uint64_t size)
{
    xline_t* obj = (xline_t*)malloc(sizeof(xline_t) + size);
    __xcheck(obj == NULL);
    obj->ref = 1;
    obj->size = size;
    obj->wpos = 0;
    obj->rpos = 0;
    obj->cb = obj->ctx = NULL;
    obj->prev = obj->next = NULL;
    obj->ptr = obj->data.b + XDATA_SIZE;
    return obj;
XClean:
    return NULL;
}

static inline xline_t* xl_maker()
{
    return xl_creator(XLINE_MAKER_SIZE);
}

static inline void xl_hold(xline_t *xl)
{
    __atom_add(xl->ref, 1);
}

static inline void xl_fixed(xline_t *xl)
{
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
}

static inline void xl_free(xline_t **pptr)
{
    if (pptr && *pptr){
        if (__atom_sub((*pptr)->ref, 1) == 0){
            __xlogd("xl_free >>>>---------------> enter\n");
            free((*pptr));
            __xlogd("xl_free >>>>---------------> exit\n");
        }
    }
}

#define __xl_fill_key(xl, key, klen) \
    do { \
        if (klen > 64) klen = 64; \
        xl->key = xl->ptr + xl->wpos; \
        xl->key[0] = klen + 1; \
        mcopy(xl->key + 1, key, klen); \
        *(xl->key + xl->key[0]) = '\0'; \
        xl->wpos += (xl->key[0] + 1); \
    }while(0)

#define __xl_realloc(xl, pptr, newlen) \
    do { \
        xl->size = (xl->size * 2) + (newlen); \
        xl = (xline_t*)malloc(sizeof(xline_t) + xl->size); \
        __xcheck(xl == NULL); \
        mcopy(xl, (*pptr), sizeof(xline_t) + (*pptr)->wpos); \
        xl->ptr = xl->data.b + XDATA_SIZE; \
        xl->size = (*pptr)->size; \
        free((*pptr)); \
        *pptr = xl; \
    }while(0)

static inline uint64_t xl_add_obj_begin(xline_t **pptr, const char *key)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = (key != NULL ? keylen + 2 + XDATA_SIZE : XDATA_SIZE);
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    if (key != NULL){
        __xl_fill_key(xl, key, keylen);
    }
    xl->wpos += XDATA_SIZE;
    return xl->wpos - XDATA_SIZE;
XClean:
    return EENDED;
}

static inline void xl_add_obj_end(xline_t **pptr, uint64_t pos)
{
    uint64_t len = (*pptr)->wpos - pos - XDATA_SIZE;
    *((xdata_t*)((*pptr)->ptr + pos)) = __xl_n2b(len, XLINE_TYPE_OBJ);
}

static inline uint64_t xl_add_list_begin(xline_t **pptr, const char *key)
{
    return xl_add_obj_begin(pptr, key);
}

static inline void xl_add_list_end(xline_t **pptr, uint64_t pos)
{
    uint64_t len = (*pptr)->wpos - pos - XDATA_SIZE;
    *((xdata_t*)((*pptr)->ptr + pos)) = __xl_n2b(len, XLINE_TYPE_LIST);
}

static inline uint64_t xl_add_word(xline_t **pptr, const char *key, const char *word)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t wordlen = slength(word) + 1;
    uint64_t newlen = keylen + 2 + XDATA_SIZE + wordlen;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_n2b(wordlen, XLINE_TYPE_STR);
    xl->wpos += XDATA_SIZE;
    mcopy(xl->ptr + xl->wpos, word, wordlen);
    xl->wpos += wordlen;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_str(xline_t **pptr, const char *key, const char *str, size_t size)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE + size;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_n2b(size, XLINE_TYPE_STR);
    xl->wpos += XDATA_SIZE;
    mcopy(xl->ptr + xl->wpos, str, size);
    xl->wpos += size;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_bin(xline_t **pptr, const char *key, const void *bin, uint64_t size)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE + size;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_n2b(size, XLINE_TYPE_BIN);
    xl->wpos += XDATA_SIZE;
    if (bin != NULL){
        mcopy(xl->ptr + xl->wpos, bin, size);
    }
    xl->wpos += size;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_obj(xline_t **pptr, const char *key, xdata_t *xd)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t size = __xl_sizeof_line(xd);
    uint64_t newlen = keylen + 2 + XDATA_SIZE + size;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    if (xd != NULL){
        mcopy(xl->ptr + xl->wpos, xd->b, size);
    }
    xl->wpos += size;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_int(xline_t **pptr, const char *key, int64_t i64)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_i2b(i64);
    xl->wpos += XDATA_SIZE;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_uint(xline_t **pptr, const char *key, uint64_t u64)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_u2b(u64);
    xl->wpos += XDATA_SIZE;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_float(xline_t **pptr, const char *key, double f64)
{
    xline_t *xl = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_f2b(f64);
    xl->wpos += XDATA_SIZE;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_add_ptr(xline_t **pptr, const char *key, void *ptr)
{
    xline_t *xl = *pptr;
    uint64_t u64 = (uint64_t)(ptr);
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XDATA_SIZE;
    if ((xl->size - xl->wpos) < newlen){
        __xl_realloc(xl, pptr, newlen);
    }
    __xl_fill_key(xl, key, keylen);
    *((xdata_t*)(xl->ptr + xl->wpos)) = __xl_u2b(u64);
    xl->wpos += XDATA_SIZE;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_OBJ);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline uint64_t xl_list_add(xline_t **pptr, xdata_t *xd)
{
    xline_t *xl = *pptr;
    uint64_t size = __xl_sizeof_line(xd);
    if ((xl->size - xl->wpos) < size){
        __xl_realloc(xl, pptr, size);
    }
    if (xd != NULL){
        mcopy(xl->ptr + xl->wpos, xd->b, size);
    }
    xl->wpos += size;
    xl->data = __xl_n2b(xl->wpos, XLINE_TYPE_LIST);
    return xl->wpos;
XClean:
    return EENDED;
}

static inline xdata_t* xl_list_next(xline_t *xl)
{
    if (xl->rpos < xl->wpos){
        xdata_t *ptr = (xdata_t*)(xl->ptr + xl->rpos);
        xl->rpos += __xl_sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static inline xdata_t* xl_next(xline_t *xl)
{
    if (xl->rpos < xl->wpos){
        xl->key = xl->ptr + xl->rpos;
        xl->rpos += (xl->key[0] + 1);
        xl->key++;
        xdata_t *val = (xdata_t*)(xl->ptr + xl->rpos);
        xl->rpos += __xl_sizeof_line(val);
        return val;
    }
    return NULL;
}

static inline xdata_t* xl_find(xline_t *xl, const char *key)
{
    xdata_t *val = NULL;
    uint64_t rpos = xl->rpos;

    while (xl->rpos < xl->wpos) {
        xl->key = xl->ptr + xl->rpos;
        xl->rpos += (xl->key[0] + 1);
        val = (xdata_t*)(xl->ptr + xl->rpos);
        xl->rpos += __xl_sizeof_line(val);
        if (slength(key) + 1 == xl->key[0]
            && mcompare(key, xl->key + 1, xl->key[0]) == 0){
            xl->key++;
            return val;
        }
    }

    xl->rpos = 0;

    while (xl->rpos < rpos) {
        xl->key = xl->ptr + xl->rpos;
        xl->rpos += (xl->key[0] + 1);
        val = (xdata_t*)(xl->ptr + xl->rpos);
        xl->rpos += __xl_sizeof_line(val);
        if (slength(key) + 1 == xl->key[0]
            && mcompare(key, xl->key + 1, xl->key[0]) == 0){
            xl->key++;
            return val;
        }
    }

    return NULL;
}

static inline int64_t xl_find_int(xline_t *xl, const char *key)
{
    xdata_t *val = xl_find(xl, key);
    if (val){
        return __xl_b2i(val);
    }
    return EENDED;
}

static inline uint64_t xl_find_uint(xline_t *xl, const char *key)
{
    xdata_t *val = xl_find(xl, key);
    if (val){
        return __xl_b2u(val);
    }
    return EENDED;
}

static inline double xl_find_float(xline_t *xl, const char *key)
{
    xdata_t *val = xl_find(xl, key);
    if (val){
        return __xl_b2f(val);
    }
    return (double)EENDED;
}

static inline char* xl_find_word(xline_t *xl, const char *key)
{
    xdata_t *val = xl_find(xl, key);
    if (val){
        return (char*)__xl_b2o(val);
    }
    return NULL;
}

static inline void* xl_find_ptr(xline_t *xl, const char *key)
{
    xdata_t *val = xl_find(xl, key);
    if (val){
        return (void *)(__xl_b2u(val));
    }
    return NULL;
}

static xline_t xl_parser(xdata_t *xd)
{
    xline_t parser = {0};
    parser.rpos = 0;
    parser.wpos = __xl_sizeof_body(xd);
    parser.ptr = __xl_b2o(xd);
    return parser;
}

static void xl_format(xdata_t *xd, const char *key, int depth, char *buf, uint64_t *pos, uint64_t size)
{
    xline_t parser = xl_parser(xd);

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

    if (__xl_typeis_obj(xd)){

        while ((xd = xl_next(&parser)) != NULL){

            if (__xl_typeis_int(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %ld,\n", (depth + 1) * 4, parser.key, __xl_b2i(xd));
            }else if (__xl_typeis_uint(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lu,\n", (depth + 1) * 4, parser.key, __xl_b2i(xd));
            }else if (__xl_typeis_real(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lf,\n", (depth + 1) * 4, parser.key, __xl_b2f(xd));
            }else if (__xl_typeis_str(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, __xl_b2o(xd));
            }else if (__xl_typeis_bin(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: size[%lu],\n", (depth + 1) * 4, parser.key, __xl_sizeof_body(xd));
            }else if (__xl_typeis_obj(xd)){
                xl_format(xd, (const char*)parser.key, depth + 1, buf, pos, size);
            }else if (__xl_typeis_list(xd)){

                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
                xline_t xllist = xl_parser(xd);

                while ((xd = xl_list_next(&xllist)) != NULL){
                    if (__xl_typeis_int(xd)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", depth * 4, __xl_b2i(xd));
                    }else if (__xl_typeis_uint(xd)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", depth * 4, __xl_b2u(xd));
                    }else if (__xl_typeis_real(xd)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", depth * 4, __xl_b2f(xd));
                    }else if (__xl_typeis_str(xd)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, __xl_b2o(xd));
                    }else if (__xl_typeis_bin(xd)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: size[%lu],\n", (depth + 1) * 4, parser.key, __xl_sizeof_body(xd));
                    }else if (__xl_typeis_obj(xd)){
                        xl_format(xd, "", depth + 1, buf, pos, size);
                    }
                }
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s},\n", depth * 4, "");

            }
        }

    }else if (__xl_typeis_list(xd)){

        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: {\n", (depth + 1) * 4, parser.key);
        xline_t xllist = xl_parser(xd);

        while ((xd = xl_list_next(&xllist)) != NULL){
            if (__xl_typeis_int(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", depth * 4, __xl_b2i(xd));
            }else if (__xl_typeis_uint(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", depth * 4, __xl_b2u(xd));
            }else if (__xl_typeis_real(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", depth * 4, __xl_b2f(xd));
            }else if (__xl_typeis_str(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, __xl_b2o(xd));
            }else if (__xl_typeis_bin(xd)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: size[%lu],\n", (depth + 1) * 4, parser.key, __xl_sizeof_body(xd));
            }else if (__xl_typeis_obj(xd)){
                xl_format(xd, "", depth + 1, buf, pos, size);
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


#define xl_printf(xd) \
    do { \
        uint64_t pos = 0; \
        uint64_t len = __xl_sizeof_line(xd) * 2; \
        char buf[len]; \
        xl_format((xd), "", 1, buf, &pos, len); \
        __xlogi("\n####################################\n%s####################################\n", buf); \
    }while(0)



static xline_t* xl_test(int count)
{
    xline_t *xobj, *xlist;
    xline_t *xl = xl_maker();
    xl->flag = XLMSG_FLAG_SEND;
    char bin[1024];
    xl_add_word(&xl, "api", "test");
    xl_add_int(&xl, "count", count);
    xl_add_word(&xl, "word", "hello word");
    xl_add_str(&xl, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&xl, "bin", bin, sizeof(bin));
    xl_add_int(&xl, "int", -123456789);
    xl_add_uint(&xl, "uint", 123456789);
    xl_add_float(&xl, "float", 123456789.123);
    // xl_printf(&xl->data);

    uint64_t obj_pos = xl_add_obj_begin(&xl, "obj");
        xl_add_word(&xl, "word", "hello word");
        xl_add_str(&xl, "str", "hello string", slength("hello string")+1);
        xl_add_bin(&xl, "bin", bin, sizeof(bin));
        xl_add_int(&xl, "int", -123456789);
        xl_add_uint(&xl, "uint", 123456789);
        xl_add_float(&xl, "float", 123456789.123);
    xl_add_obj_end(&xl, obj_pos);
    // xl_printf(&xl->data);

    uint64_t list_pos = xl_add_list_begin(&xl, "list");
    for (int i = 0; i < 3; ++i){
        uint64_t tmp_pos = xl_add_obj_begin(&xl, NULL);
            xl_add_word(&xl, "word", "hello word");
            xl_add_str(&xl, "str", "hello string", slength("hello string")+1);
            xl_add_bin(&xl, "bin", bin, sizeof(bin));
            xl_add_int(&xl, "int", -123456789);
            xl_add_uint(&xl, "uint", 123456789);
            xl_add_float(&xl, "float", 123456789.123);
        xl_add_obj_end(&xl, tmp_pos);
    }
    xl_add_list_end(&xl, list_pos);
    // xl_printf(&xl->data);

    xobj = xl_maker();
    xl_add_word(&xobj, "word", "hello word");
    xl_add_str(&xobj, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&xobj, "bin", bin, sizeof(bin));
    xl_add_int(&xobj, "int", -123456789);
    xl_add_uint(&xobj, "uint", 123456789);
    xl_add_float(&xobj, "float", 123456789.123);
    xl_add_obj(&xl, "addobj", &xobj->data);
    
    // xl_printf(&xl->data);

    xlist = xl_maker();
    for (int i = 0; i < 10; ++i){
        double f = i * 0.1f;
        xl_list_add(&xlist, &__xl_f2b(f));
    }
    // xl_printf(&xlist->data);

    xl_add_obj(&xl, "addlist", &xlist->data);
    
    xl_printf(&xl->data);

    // xl_free(&xl);
    xl_free(&xobj);
    xl_free(&xlist);

    return xl;
}


#endif //__XLINE_H__
