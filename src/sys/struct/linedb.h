#ifndef __LINEDB_H__
#define __LINEDB_H__


#include <env/env.h>

//linedb 结构体是为快速传输结构化数据而设计的
//linedb 只适合存储数据单元, 也可以使用 linedb 作为数据存储结构，但是需要进行一次拓展封装来维护 linedb 列表

// #define LINEDB_HEAD_MASK     0x1f
#define LINEDB_HEAD_MASK     0x0f
#define LINEDB_TYPE_MASK     0xc0

enum {
    // LINEDB_TYPE_4BIT
    LINEDB_TYPE_8BIT = 0x01,
    LINEDB_TYPE_16BIT = 0x02,
    LINEDB_TYPE_32BIT = 0x04,
    LINEDB_TYPE_64BIT = 0x08,
    LINEDB_TYPE_OBJECT = 0x10
    // LINEDB_TYPE_OBJECT = 0x20
};

enum {
    LINEDB_NUMBER_INTEGER = 0x00,
    LINEDB_NUMBER_UNSIGNED = 0x40,
    LINEDB_NUMBER_FLOAT = 0x80,
    LINEDB_NUMBER_BOOLEAN = 0xc0
};

enum {
    LINEDB_OBJECT_CUSTOM = 0x00,
    LINEDB_OBJECT_STRING = 0x40,
    LINEDB_OBJECT_BINARY = 0x80,
    LINEDB_OBJECT_LIST = 0xc0
};

#define __LINEDB_HEAD_ALLOC_SIZE      16


typedef struct linear_data_block {
    unsigned char byte[__LINEDB_HEAD_ALLOC_SIZE];
}linedb_t;

typedef union {
    float f;
    unsigned char byte[4];
}__real32;

typedef union {
    double f;
    unsigned char byte[8];
}__real64;





#define __number2block8bit(n, flag) \
        (linedb_t){ \
            LINEDB_TYPE_8BIT | (flag), \
            (((char*)&(n))[0]) \
        }

#define __block2number8bit(b)   ((b)->byte[1])



#define __number2block16bit(n, flag) \
        (linedb_t){ \
            LINEDB_TYPE_16BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]) \
        }

#define __block2number16bit(b)  ((b)->byte[2] << 8 | (b)->byte[1])  



#define __number2block32bit(n, flag) \
        (linedb_t){ \
            LINEDB_TYPE_32BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]) \
        }

#define __block2number32bit(b) \
        ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1])



#define __number2block64bit(n, flag) \
        (linedb_t){ \
            LINEDB_TYPE_64BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __block2number64bit(b, type) \
        ( (type)(b)->byte[8] << 56 | (type)(b)->byte[7] << 48 | (type)(b)->byte[6] << 40 | (type)(b)->byte[5] << 32 \
        | (type)(b)->byte[4] << 24 | (type)(b)->byte[3] << 16 | (type)(b)->byte[2] << 8 | (type)(b)->byte[1] )



#define __block2float32bit(b) \
        (((__real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f)

#   define __block2float64bit(b) \
        (((__real64) \
            { \
                .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
            } \
        ).f)



#define __n2b8(n)        __number2block8bit(n, LINEDB_NUMBER_INTEGER)
#define __b2n8(b)        __block2number8bit(b)
#define __n2b16(n)       __number2block16bit(n, LINEDB_NUMBER_INTEGER)
#define __b2n16(b)       __block2number16bit(b)
#define __n2b32(n)       __number2block32bit(n, LINEDB_NUMBER_INTEGER)
#define __b2n32(b)       __block2number32bit(b)
#define __n2b64(n)       __number2block64bit(n, LINEDB_NUMBER_INTEGER)
#define __b2n64(b)       __block2number64bit(b, int64_t)

#define __u2b8(n)        __number2block8bit(n, LINEDB_NUMBER_UNSIGNED)
#define __b2u8(b)        __block2number8bit(b)
#define __u2b16(n)       __number2block16bit(n, LINEDB_NUMBER_UNSIGNED)
#define __b2u16(b)       __block2number16bit(b)
#define __u2b32(n)       __number2block32bit(n, LINEDB_NUMBER_UNSIGNED)
#define __b2u32(b)       __block2number32bit(b)
#define __u2b64(n)       __number2block64bit(n, LINEDB_NUMBER_UNSIGNED)
#define __b2u64(b)       __block2number64bit(b, uint64_t)

#define __f2b32(f)       __number2block32bit(f, LINEDB_NUMBER_FLOAT)
#define __b2f32(b)       __block2float32bit(b)
#define __f2b64(f)       __number2block64bit(f, LINEDB_NUMBER_FLOAT)
#define __b2f64(b)       __block2float64bit(b)

#define __boolean2block(b)       __number2block8bit(b, LINEDB_NUMBER_BOOLEAN)
#define __block2boolean(b)       __block2number8bit(b)

#define __typeis_object(b)      ((b)->byte[0] & LINEDB_TYPE_OBJECT)
#define __typeis_number(b)      (!((b)->byte[0] & LINEDB_TYPE_OBJECT))
#define __typeof_linedb(b)      ((b)->byte[0] & LINEDB_TYPE_MASK)

#define __objectis_custom(b)    (__typeof_linedb(b) == LINEDB_OBJECT_CUSTOM)
#define __objectis_string(b)    (__typeof_linedb(b) == LINEDB_OBJECT_STRING)
#define __objectis_list(b)      (__typeof_linedb(b) == LINEDB_OBJECT_LIST)
#define __objectis_binary(b)    (__typeof_linedb(b) == LINEDB_OBJECT_BINARY)

#define __numberis_integer(b)   (__typeof_linedb(b) == LINEDB_NUMBER_INTEGER)
#define __numberis_unsigned(b)  (__typeof_linedb(b) == LINEDB_NUMBER_UNSIGNED)
#define __numberis_float(b)     (__typeof_linedb(b) == LINEDB_NUMBER_FLOAT)
#define __numberis_boolean(b)   (__typeof_linedb(b) == LINEDB_NUMBER_BOOLEAN)

#define __numberis_8bit(b)      ((b)->byte[0] & LINEDB_TYPE_8BIT)
#define __numberis_16bit(b)     ((b)->byte[0] & LINEDB_TYPE_16BIT)
#define __numberis_32bit(b)     ((b)->byte[0] & LINEDB_TYPE_32BIT)
#define __numberis_64bit(b)     ((b)->byte[0] & LINEDB_TYPE_64BIT)

#define __sizeof_head(b)        (1 + (((b)->byte[0]) & LINEDB_HEAD_MASK))

#define __sizeof_data(b) \
        (uint64_t)( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? (((b)->byte[0] & LINEDB_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & LINEDB_TYPE_16BIT)) ? __b2n16(b) \
        : (((b)->byte[0] & LINEDB_TYPE_32BIT)) ? __b2n32(b) \
        : __b2n64(b) : 0 )

#define __sizeof_linedb(b) \
        ( __sizeof_head(b) + __sizeof_data(b))

#define __planof_linedb(size) \
        (size < 0x100 ? (2 + size) : size < 0x10000 ? (3 + size) : size < 0x100000000 ? (5 + size) : (9 + size))

#define __dataof_linedb(b)       (&((b)->byte[0]) + __sizeof_head(b))


static inline void linedb_reset_type(linedb_t *ldb, char type)
{
    ldb->byte[0] &= (~LINEDB_TYPE_MASK);
    ldb->byte[0] |= type;
}


//使用宏现实这个函数，长度转字节用 number to byte
static inline uint64_t linedb_bind_address(linedb_t **ldb, void *addr, uint64_t size)
{
    linedb_t *db = (linedb_t*)addr;
    if (size < 0x100){
        *db = __number2block8bit(size, LINEDB_TYPE_OBJECT);
    }else if (size < 0x10000){
        *db = __number2block16bit(size, LINEDB_TYPE_OBJECT);
    }else if (size < 0x100000000){
        *db = __number2block32bit(size, LINEDB_TYPE_OBJECT);
    }else {
        *db = __number2block64bit(size, LINEDB_TYPE_OBJECT);
    }
    *ldb = db;
    return __sizeof_linedb(db);
}

static inline linedb_t* linedb_load_object(linedb_t *ldb, const void *object, uint64_t size)
{
    if (size < 0x100){
        *ldb = __number2block8bit(size, LINEDB_TYPE_OBJECT);
        if (object){
            memcpy(&ldb->byte[2], object, size);
        }
    }else if (size < 0x10000){
        *ldb = __number2block16bit(size, LINEDB_TYPE_OBJECT);
        if (object){
            memcpy(&ldb->byte[3], object, size);
        }
    }else if (size < 0x100000000){
        *ldb = __number2block32bit(size, LINEDB_TYPE_OBJECT);
        if (object){
            memcpy(&ldb->byte[5], object, size);
        }
    }else {
        *ldb = __number2block64bit(size, LINEDB_TYPE_OBJECT);
        if (object){
            memcpy(&ldb->byte[9], object, size);
        }
    }
    return ldb;
}

static inline linedb_t* string2linedb(const char *str)
{
    size_t len = strlen(str);
    linedb_t *ldb = (linedb_t *)malloc(__planof_linedb(len) + 1);
    *(ldb->byte + __sizeof_linedb(ldb)) = '\0';
    return linedb_load_object(ldb, str, len);
}

static inline linedb_t* object2linedb(const void *obj, size_t size)
{
    linedb_t *ldb = (linedb_t *)malloc(__planof_linedb(size));
    return linedb_load_object(ldb, obj, size);
}

static inline void linedb_destroy(linedb_t **pp_ldb)
{
    if (pp_ldb && *pp_ldb){
        linedb_t *ldb = *pp_ldb;
        *pp_ldb = NULL;
        if (ldb->byte[0] & LINEDB_TYPE_OBJECT){
            free(ldb);
        }
    }
}

////////////////////////////////////////////////////////
////////////////////////////////////////////////////////
////////////////////////////////////////////////////////

typedef struct linear_data_block_pipeline {
    uint64_t len;
    uint64_t read_pos;
    uint64_t write_pos;
    uint64_t leftover;
    uint64_t block_count;
    linedb_t *read_block;
    linedb_t *write_block;
    unsigned char *buf;
}linedb_pipe_t;


static inline linedb_pipe_t* linedb_pipe_create(uint64_t len)
{
    linedb_pipe_t *lp = (linedb_pipe_t *)malloc(sizeof(linedb_pipe_t));
    if (lp == NULL){
        return NULL;
    }

   if ((len & (len - 1)) == 0){
        lp->len = len;
    }else{
        lp->len = 1U;
        do {
            lp->len <<= 1;
        } while(len >>= 1);
    }

    // if (lp->len < (1U << 16)){
    //     lp->len = (1U << 16);
    // }

    lp->leftover = lp->len;

    lp->buf = (unsigned char *)malloc(lp->len);
    if (lp->buf == NULL){
        free(lp);
        return NULL;
    }

    lp->read_block = lp->write_block = (linedb_t*)lp->buf;
    lp->read_pos = lp->write_pos = 0;

    return lp;
}

static inline void linedb_pipe_destroy(linedb_pipe_t **pp_lp)
{
    if (pp_lp && *pp_lp){
        linedb_pipe_t *lp = *pp_lp;
        *pp_lp = NULL;
        free(lp->buf);
        free(lp);
    }
}

static inline uint64_t linedb_pipe_write(linedb_pipe_t *lp, void *data, uint64_t size)
{
    uint64_t ldb_size = __planof_linedb(size); 
    while ((uint64_t)(lp->len - lp->write_pos + lp->read_pos) >= ldb_size) {

        if (lp->leftover < ldb_size){
            ldb_size = linedb_bind_address(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->leftover;
            lp->leftover = lp->len;
        }else {
            linedb_load_object(lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            // ldb_size = __sizeof_linedb(lp->write_block);
            // memcpy(__dataof_linedb(lp->write_block), data, size);
            lp->write_pos += __sizeof_linedb(lp->write_block);
            lp->leftover = lp->len - (lp->write_pos & (lp->len - 1));
            lp->block_count++;
            return size;
        }
    }

    return 0;
}

static inline uint64_t linedb_pipe_read(linedb_pipe_t *lp, char *buf, uint64_t size)
{
    while (((uint64_t)(lp->write_pos - lp->read_pos)) > 0) {
        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        }else {
            uint64_t data_size = __sizeof_data(lp->read_block);
            if (size >= data_size){
                memcpy(buf, __dataof_linedb(lp->read_block), data_size);
                lp->read_pos += (data_size + __sizeof_head(lp->read_block));
                lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
                lp->block_count--;
            }
            return data_size;
        }
    }

    return 0;
}

static inline linedb_t* linedb_pipe_hold_block(linedb_pipe_t *lp)
{
    while (((uint64_t)(lp->write_pos - lp->read_pos)) > 0) {
        if (lp->read_block->byte[0] == 0){
            lp->read_pos += ((lp->buf + lp->len) - lp->read_block->byte);
            lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        }else {
            return lp->read_block;
        }
    }

    return NULL;
}

static inline void linedb_pipe_free_block(linedb_pipe_t *lp, linedb_t *ldb)
{
    if (ldb == lp->read_block){
        lp->read_pos += (__sizeof_linedb(lp->read_block));
        lp->read_block = (linedb_t *)(lp->buf + (lp->read_pos & (lp->len - 1)));
        lp->block_count--;
    }
}

static inline uint64_t linedb_pipe_readable(linedb_pipe_t *lp)
{
    return ((uint64_t)(lp->write_pos - lp->read_pos));
}

static inline uint64_t linedb_pipe_writable(linedb_pipe_t *lp)
{
    return ((uint64_t)(lp->len - lp->write_pos + lp->read_pos));
}

static inline uint64_t linedb_pipe_block_count(linedb_pipe_t *lp)
{
    return lp->block_count;
}


#endif //__LINEDB_H__