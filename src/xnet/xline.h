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

#define XLINE_SIZE      9

typedef struct xline {
    uint8_t b[XLINE_SIZE];
}xline_t;

union xreal {
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
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_REAL)


#define __xl_b2n(l, type) \
        ( (type)(l)->b[8] << 56 | (type)(l)->b[7] << 48 | (type)(l)->b[6] << 40 | (type)(l)->b[5] << 32 \
        | (type)(l)->b[4] << 24 | (type)(l)->b[3] << 16 | (type)(l)->b[2] << 8 | (type)(l)->b[1] )

static inline double __xl_b2float(xline_t *l)
{
    union xreal f;
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
#define __xl_f2b(n) __xl_n2b(n, XLINE_TYPE_REAL)


#define __xl_b2n(l, type) \
        ( (type)(l)->b[1] << 56 | (type)(l)->b[2] << 48 | (type)(l)->b[3] << 40 | (type)(l)->b[4] << 32 \
        | (type)(l)->b[5] << 24 | (type)(l)->b[6] << 16 | (type)(l)->b[7] << 8 | (type)(l)->b[8] )

static inline double __xl_b2float(xline_t *l)
{
    union xreal f;
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
#define __xl_l2b(l)                 ((void*)&(l)->b[XLINE_SIZE])

#define __xl_typeis_int(l)          ((l)->b[0] == XLINE_TYPE_INT)
#define __xl_typeis_uint(l)         ((l)->b[0] == XLINE_TYPE_UINT)
#define __xl_typeis_real(l)         ((l)->b[0] == XLINE_TYPE_REAL)
#define __xl_typeis_num(l)          ((l)->b[0] <= XLINE_TYPE_REAL)

#define __xl_typeis_str(l)          ((l)->b[0] == XLINE_TYPE_STR)
#define __xl_typeis_bin(l)          ((l)->b[0] == XLINE_TYPE_BIN)
#define __xl_typeis_obj(l)          ((l)->b[0] == XLINE_TYPE_OBJ)
#define __xl_typeis_list(l)         ((l)->b[0] == XLINE_TYPE_LIST)

#define __xl_sizeof_head(l)         (__xl_typeis_num(l) ? 1 : XLINE_SIZE)
#define __xl_sizeof_body(l)         (__xl_typeis_num(l) ? 8 : __xl_b2u((l)))
#define __xl_sizeof_line(l)         (__xl_typeis_num(l) ? XLINE_SIZE : __xl_b2u((l)) + XLINE_SIZE)


// #define XLINE_MAKER_SIZE            (1024 * 64)
#define XLINE_MAKER_SIZE            (16)


typedef struct xframe {
    __atom_size ref;
    uint8_t flag;
    uint16_t type;
    uint32_t id;
    uint64_t spos, range;
    uint64_t rpos, wpos, size;
    uint8_t *key;
    xline_t *val;
    uint8_t *ptr;
    void *args[4];
    struct avl_node node;
    struct xframe *prev, *next;
    xline_t line;
}xframe_t;


static inline xframe_t* xl_creator(uint64_t size)
{
    xframe_t* frame = (xframe_t*)malloc(sizeof(xframe_t) + size);
    __xcheck(frame == NULL);
    frame->ref = 1;
    frame->size = size;
    frame->wpos = frame->rpos = frame->spos = frame->range = 0;
    frame->args[0] = frame->args[1] = frame->args[2] = frame->args[3] = NULL;
    frame->prev = frame->next = NULL;
    frame->ptr = frame->line.b + XLINE_SIZE;
    return frame;
XClean:
    return NULL;
}

static inline xframe_t* xl_maker()
{
    return xl_creator(XLINE_MAKER_SIZE);
}

static inline void xl_free(xframe_t **pptr)
{
    if (pptr && *pptr){
        if (__atom_sub((*pptr)->ref, 1) == 0){
            free((*pptr));
            *pptr = NULL;
        }
    }
}

static inline void xl_hold(xframe_t **pptr)
{
    __atom_add((*pptr)->ref, 1);
}

static inline void xl_fixed(xframe_t **pptr)
{
    (*pptr)->line = __xl_n2b((*pptr)->wpos, XLINE_TYPE_OBJ);
}

static inline void xl_clear(xframe_t **pptr)
{
    (*pptr)->wpos = (*pptr)->rpos = 0;
}

static inline uint64_t xl_usable(xframe_t **pptr, const char *key)
{
    int headlen = slength(key) + 2 + XLINE_SIZE;
    if (((*pptr)->size - (*pptr)->wpos) > headlen){
        return ((*pptr)->size - (*pptr)->wpos) - headlen;
    }
    return 0;
}

#define __xl_fill_key(xf, key, klen) \
    do { \
        if (klen > 64) klen = 64; \
        xf->key = xf->ptr + xf->wpos; \
        xf->key[0] = klen + 1; \
        mcopy(xf->key + 1, key, klen); \
        *(xf->key + xf->key[0]) = '\0'; \
        xf->wpos += (xf->key[0] + 1); \
    }while(0)

#define __xl_realloc(xf, pptr, newlen) \
    do { \
        xf->size = (xf->size * 2) + (newlen); \
        xf = (xframe_t*)malloc(sizeof(xframe_t) + xf->size); \
        __xcheck(xf == NULL); \
        mcopy(xf, (*pptr), sizeof(xframe_t) + (*pptr)->wpos); \
        xf->ptr = xf->line.b + XLINE_SIZE; \
        xf->size = (*pptr)->size; \
        free((*pptr)); \
        *pptr = xf; \
    }while(0)


static inline uint64_t xl_obj_begin(xframe_t **pptr, const char *key)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = (key != NULL ? keylen + 2 + XLINE_SIZE : XLINE_SIZE);
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    if (key != NULL){
        __xl_fill_key(frame, key, keylen);
    }
    frame->wpos += XLINE_SIZE;
    return frame->wpos - XLINE_SIZE;
XClean:
    return XEOF;
}

static inline uint64_t xl_obj_end(xframe_t **pptr, uint64_t pos)
{
    if (pos < XEOF){
        uint64_t len = (*pptr)->wpos - pos - XLINE_SIZE;
        *((xline_t*)((*pptr)->ptr + pos)) = __xl_n2b(len, XLINE_TYPE_OBJ);
    }
    return pos;
}

static inline uint64_t xl_list_begin(xframe_t **pptr, const char *key)
{
    return xl_obj_begin(pptr, key);
}

static inline uint64_t xl_list_end(xframe_t **pptr, uint64_t pos)
{
    if (pos < XEOF){
        uint64_t len = (*pptr)->wpos - pos - XLINE_SIZE;
        *((xline_t*)((*pptr)->ptr + pos)) = __xl_n2b(len, XLINE_TYPE_LIST);
        (*pptr)->line = __xl_n2b((*pptr)->wpos, XLINE_TYPE_OBJ);
    }
    return pos;
}

static inline uint64_t xl_add_word(xframe_t **pptr, const char *key, const char *word)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t wordlen = slength(word) + 1;
    uint64_t newlen = keylen + 2 + XLINE_SIZE + wordlen;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_n2b(wordlen, XLINE_TYPE_STR);
    frame->wpos += XLINE_SIZE;
    mcopy(frame->ptr + frame->wpos, word, wordlen);
    frame->wpos += wordlen;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_str(xframe_t **pptr, const char *key, const char *str, size_t size)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE + size;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_n2b(size, XLINE_TYPE_STR);
    frame->wpos += XLINE_SIZE;
    mcopy(frame->ptr + frame->wpos, str, size);
    frame->wpos += size;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_bin(xframe_t **pptr, const char *key, const void *bin, uint64_t size)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE + size;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_n2b(size, XLINE_TYPE_BIN);
    frame->wpos += XLINE_SIZE;
    if (bin != NULL){
        mcopy(frame->ptr + frame->wpos, bin, size);
    }
    frame->wpos += size;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_obj(xframe_t **pptr, const char *key, xline_t *xd)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t size = __xl_sizeof_line(xd);
    uint64_t newlen = keylen + 2 + XLINE_SIZE + size;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    if (xd != NULL){
        mcopy(frame->ptr + frame->wpos, xd->b, size);
    }
    frame->wpos += size;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_int(xframe_t **pptr, const char *key, int64_t i64)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_i2b(i64);
    frame->wpos += XLINE_SIZE;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_uint(xframe_t **pptr, const char *key, uint64_t u64)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_u2b(u64);
    frame->wpos += XLINE_SIZE;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_float(xframe_t **pptr, const char *key, double f64)
{
    xframe_t *frame = *pptr;
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_f2b(f64);
    frame->wpos += XLINE_SIZE;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_add_ptr(xframe_t **pptr, const char *key, void *ptr)
{
    xframe_t *frame = *pptr;
    uint64_t u64 = (uint64_t)(ptr);
    uint64_t keylen = slength(key);
    uint64_t newlen = keylen + 2 + XLINE_SIZE;
    if ((frame->size - frame->wpos) < newlen){
        __xl_realloc(frame, pptr, newlen);
    }
    __xl_fill_key(frame, key, keylen);
    *((xline_t*)(frame->ptr + frame->wpos)) = __xl_u2b(u64);
    frame->wpos += XLINE_SIZE;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_OBJ);
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_list_append(xframe_t **pptr, xline_t *xl)
{
    xframe_t *frame = *pptr;
    uint64_t size = __xl_sizeof_line(xl);
    if ((frame->size - frame->wpos) < size){
        __xl_realloc(frame, pptr, size);
    }
    if (xl != NULL){
        mcopy(frame->ptr + frame->wpos, xl->b, size);
    }
    frame->wpos += size;
    return frame->wpos;
XClean:
    return XEOF;
}

static inline uint64_t xl_list_add(xframe_t **pptr, xline_t *xl)
{
    xframe_t *frame = *pptr;
    uint64_t size = __xl_sizeof_line(xl);
    if ((frame->size - frame->wpos) < size){
        __xl_realloc(frame, pptr, size);
    }
    if (xl != NULL){
        mcopy(frame->ptr + frame->wpos, xl->b, size);
    }
    frame->wpos += size;
    frame->line = __xl_n2b(frame->wpos, XLINE_TYPE_LIST);
    return frame->wpos;
XClean:
    return XEOF;
}


typedef struct xparser {
    uint64_t rpos, wpos, size;
    uint8_t *key;
    xline_t *val;
    uint8_t *ptr;
}xparser_t;


static xparser_t xl_parser(xline_t *xl)
{
    xparser_t parser = {0};
    parser.rpos = 0;
    parser.wpos = __xl_sizeof_body(xl);
    parser.ptr = __xl_l2b(xl);
    return parser;
}

static inline xline_t* xl_list_next(xparser_t *parser)
{
    if (parser->rpos < parser->wpos){
        xline_t *ptr = (xline_t*)(parser->ptr + parser->rpos);
        parser->rpos += __xl_sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}

static inline xline_t* xl_next(xparser_t *parser)
{
    if (parser->rpos < parser->wpos){
        parser->key = parser->ptr + parser->rpos;
        parser->rpos += (parser->key[0] + 1);
        parser->key++;
        xline_t *val = (xline_t*)(parser->ptr + parser->rpos);
        parser->rpos += __xl_sizeof_line(val);
        return val;
    }
    return NULL;
}

static inline xline_t* xl_find(xparser_t *parser, const char *key)
{
    uint64_t rpos = parser->rpos;

    while (parser->rpos < parser->wpos) {
        parser->key = parser->ptr + parser->rpos;
        parser->rpos += (parser->key[0] + 1);
        parser->val = (xline_t*)(parser->ptr + parser->rpos);
        parser->rpos += __xl_sizeof_line(parser->val);
        if (slength(key) + 1 == parser->key[0]
            && mcompare(key, parser->key + 1, parser->key[0]) == 0){
            parser->key++;
            return parser->val;
        }
    }

    parser->rpos = 0;

    while (parser->rpos < rpos) {
        parser->key = parser->ptr + parser->rpos;
        parser->rpos += (parser->key[0] + 1);
        parser->val = (xline_t*)(parser->ptr + parser->rpos);
        parser->rpos += __xl_sizeof_line(parser->val);
        if (slength(key) + 1 == parser->key[0]
            && mcompare(key, parser->key + 1, parser->key[0]) == 0){
            parser->key++;
            return parser->val;
        }
    }

    parser->val = NULL;

    return parser->val;
}

static inline xline_t* xl_find_int(xparser_t *parser, const char *key, int64_t *ptr)
{
    if (xl_find(parser, key) && ptr){
        *ptr = __xl_b2i(parser->val);
    }
    return parser->val;
}

static inline xline_t* xl_find_uint(xparser_t *parser, const char *key, uint64_t *ptr)
{
    if (xl_find(parser, key) && ptr){
        *ptr = __xl_b2u(parser->val);
    }
    return parser->val;
}

static inline xline_t* xl_find_float(xparser_t *parser, const char *key, double *ptr)
{
    if (xl_find(parser, key) && ptr){
        *ptr = __xl_b2f(parser->val);
    }
    return parser->val;
}

static inline xline_t* xl_find_word(xparser_t *parser, const char *key, char **pptr)
{
    if (xl_find(parser, key) && pptr){
        *pptr = (char*)__xl_l2b(parser->val);
    }
    return parser->val;
}

static inline xline_t* xl_find_ptr(xparser_t *parser, const char *key, void **pptr)
{
    if (xl_find(parser, key) && pptr){
        *pptr = (void*)(__xl_b2u(parser->val));
    }
    return parser->val;
}

static inline xline_t* xl_set_value(xparser_t *parser, const char *key, uint64_t nb)
{
    xline_t *val = xl_find(parser, key);
    if (val){
        *val = __xl_u2b(nb);
    }
    return val;
}

static void xl_format(xline_t *xl, const char *key, int depth, char *buf, uint64_t *pos, uint64_t size)
{
    xparser_t parser = xl_parser(xl);

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

    if (__xl_typeis_obj(xl)){

        while ((xl = xl_next(&parser)) != NULL){

            if (__xl_typeis_int(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %ld,\n", (depth + 1) * 4, parser.key, __xl_b2i(xl));
            }else if (__xl_typeis_uint(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lu,\n", (depth + 1) * 4, parser.key, __xl_b2i(xl));
            }else if (__xl_typeis_real(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %lf,\n", (depth + 1) * 4, parser.key, __xl_b2f(xl));
            }else if (__xl_typeis_str(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: %s,\n", (depth + 1) * 4, parser.key, __xl_l2b(xl));
            }else if (__xl_typeis_bin(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: size[%lu],\n", (depth + 1) * 4, parser.key, __xl_sizeof_body(xl));
            }else if (__xl_typeis_obj(xl)){
                xl_format(xl, (const char*)parser.key, depth + 1, buf, pos, size);
            }else if (__xl_typeis_list(xl)){

                *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: [\n", (depth + 1) * 4, parser.key);
                xparser_t xllist = xl_parser(xl);

                while ((xl = xl_list_next(&xllist)) != NULL){
                    if (__xl_typeis_int(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", (depth + 1) * 4, __xl_b2i(xl));
                    }else if (__xl_typeis_uint(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", (depth + 1) * 4, __xl_b2u(xl));
                    }else if (__xl_typeis_real(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", (depth + 1) * 4, __xl_b2f(xl));
                    }else if (__xl_typeis_str(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*s,\n", (depth + 1) * 4, __xl_l2b(xl));
                    }else if (__xl_typeis_bin(xl)){
                        *pos += __xapi->snprintf(buf + *pos, size - *pos, "    bin,\n");
                    }else if (__xl_typeis_obj(xl)){
                        xl_format(xl, "", depth + 1, buf, pos, size);
                    }
                }
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s],\n", depth * 4, "");

            }
        }

    }else if (__xl_typeis_list(xl)){

        *pos += __xapi->snprintf(buf + *pos, size - *pos, "%*s: [\n", (depth + 1) * 4, parser.key);
        xparser_t xllist = xl_parser(xl);

        while ((xl = xl_list_next(&xllist)) != NULL){
            if (__xl_typeis_int(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*ld,\n", (depth + 1) * 4, __xl_b2i(xl));
            }else if (__xl_typeis_uint(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lu,\n", (depth + 1) * 4, __xl_b2u(xl));
            }else if (__xl_typeis_real(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*lf,\n", (depth + 1) * 4, __xl_b2f(xl));
            }else if (__xl_typeis_str(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    %*s,\n", (depth + 1) * 4, __xl_l2b(xl));
            }else if (__xl_typeis_bin(xl)){
                *pos += __xapi->snprintf(buf + *pos, size - *pos, "    bin,\n");
            }else if (__xl_typeis_obj(xl)){
                xl_format(xl, "", depth + 1, buf, pos, size);
            }
        }
        *pos += __xapi->snprintf(buf + *pos, size - *pos, "  %*s],\n", depth * 4, "");
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
        if (pos < 1024 * 1024) __xlogi("\n####################################\n%s####################################\n", buf); \
    }while(0)



static xframe_t* xl_test(int count)
{
    xframe_t *xobj, *xlist, *xstrlist;
    xframe_t *root = xl_maker();
    char bin[1024];
    xl_add_word(&root, "api", "test");
    xl_add_int(&root, "count", count);
    xl_add_word(&root, "word", "hello word");
    xl_add_str(&root, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&root, "bin", bin, sizeof(bin));
    xl_add_int(&root, "int", -123456789);
    xl_add_uint(&root, "uint", 123456789);
    xl_add_float(&root, "float", 123456789.123);
    // xl_printf(&xl->line);

    uint64_t obj_pos = xl_obj_begin(&root, "obj");
        xl_add_word(&root, "word", "hello word");
        xl_add_str(&root, "str", "hello string", slength("hello string")+1);
        xl_add_bin(&root, "bin", bin, sizeof(bin));
        xl_add_int(&root, "int", -123456789);
        xl_add_uint(&root, "uint", 123456789);
        xl_add_float(&root, "float", 123456789.123);
    xl_obj_end(&root, obj_pos);
    // xl_printf(&xl->line);

    uint64_t list_pos = xl_list_begin(&root, "list");
    for (int i = 0; i < 3; ++i){
        uint64_t tmp_pos = xl_obj_begin(&root, NULL);
            xl_add_word(&root, "word", "hello word");
            xl_add_str(&root, "str", "hello string", slength("hello string")+1);
            xl_add_bin(&root, "bin", bin, sizeof(bin));
            xl_add_int(&root, "int", -123456789);
            xl_add_uint(&root, "uint", 123456789);
            xl_add_float(&root, "float", 123456789.123);
        xl_obj_end(&root, tmp_pos);
    }
    xl_list_end(&root, list_pos);
    // xl_printf(&xl->line);

    xobj = xl_maker();
    xl_add_word(&xobj, "word", "hello word");
    xl_add_str(&xobj, "str", "hello string", slength("hello string")+1);
    xl_add_bin(&xobj, "bin", bin, sizeof(bin));
    xl_add_int(&xobj, "int", -123456789);
    xl_add_uint(&xobj, "uint", 123456789);
    xl_add_float(&xobj, "float", 123456789.123);
    xl_add_obj(&root, "addobj", &xobj->line);
    
    // xl_printf(&xl->line);

    xlist = xl_maker();
    for (int i = 0; i < 10; ++i){
        double f = i * 0.1f;
        xl_list_add(&xlist, &__xl_f2b(f));
    }
    // xl_printf(&xlist->line);

    xl_add_obj(&root, "addlist", &xlist->line);
    

    xstrlist = xl_maker();
    for (int i = 0; i < 10; ++i){
        char str[64] = {0};
        uint64_t len = slength("hello") + 1;
        *((xline_t*)(str)) = __xl_n2b(len, XLINE_TYPE_STR);
        mcopy(str + 9, "hello", len);
        xl_list_add(&xstrlist, (xline_t*)str);
    }
    // xl_printf(&xstrlist->line);

    xl_add_obj(&root, "strlist", &xstrlist->line);

    // xl_printf(&xl->line);

    xl_free(&xstrlist);
    xl_free(&xobj);
    xl_free(&xlist);

    return root;
}


#endif //__XLINE_H__
