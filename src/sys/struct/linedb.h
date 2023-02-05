#ifndef __LINEDB_H__
#define __LINEDB_H__


#include <env/env.h>


#define LINEDB_HEAD_MASK     0x0f
#define LINEDB_TYPE_MASK     0xc0

//一个 linedb 的长度是从一个已经分配完成的 linedb 中获取的，所以，这个 linedb 中包含的所有子孙的长度的总和不大于64bit，
//一个子 linedb 的长度必定小于64bit

enum {
    //LINEDB_TYPE_4BIT
    LINEDB_TYPE_8BIT = 0x01,
    LINEDB_TYPE_16BIT = 0x02,
    LINEDB_TYPE_32BIT = 0x04,
    LINEDB_TYPE_64BIT = 0x08, //启用64bit的长度的对象，为支持 LINEDB_TYPE_LIST
    LINEDB_TYPE_OBJECT = 0x10,
    //LINEDB_TYPE_LIST //LIST 的长度32bit 条目数32bit 两项组合放在64bit中 LIST 的总容量不大于64bit
    LINEDB_TYPE_BIGENDIAN = 0x20, 
};

enum {
    LINEDB_NUMBER_INTEGER = 0x00,
    LINEDB_NUMBER_UNSIGNED = 0x40,
    LINEDB_NUMBER_FLOAT = 0x80,
    LINEDB_NUMBER_BOOLEAN = 0xc0
};

enum {
    LINEDB_OBJECT_BINARY = 0x00,
    LINEDB_OBJECT_STRING = 0x40,
    LINEDB_OBJECT_CUSTOM = 0x80,
    LINEDB_OBJECT_RESERVED = 0xc0
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



#ifdef __LITTLE_ENDIAN__

#   define __number2block8bit(n, flag) \
            (linedb_t){ \
                LINEDB_TYPE_8BIT | (flag), \
                (((char*)&(n))[0]) \
            }

#   define __block2number8bit(b)    ((b)->byte[1])

#   define __number2block16bit(n, flag) \
            (linedb_t){ \
                LINEDB_TYPE_16BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block2number16bit(b) \
            (((b)->byte[0] & LINEDB_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 8 | (b)->byte[2]) \
                : ((b)->byte[2] << 8 | (b)->byte[1]) \
			)       

#   define __number2block32bit(n, flag) \
            (linedb_t){ \
                LINEDB_TYPE_32BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block2number32bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
                : ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
			)

#   define __number2block64bit(n, flag) \
            (linedb_t){ \
                LINEDB_TYPE_64BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __block2number64bit(b, type) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((type)(b)->byte[1] << 56 \
                | (type)(b)->byte[2] << 48 \
                | (type)(b)->byte[3] << 40 \
                | (type)(b)->byte[4] << 32 \
                | (type)(b)->byte[5] << 24 \
                | (type)(b)->byte[6] << 16 \
                | (type)(b)->byte[7] << 8 \
                | (type)(b)->byte[8]) \
                : ((type)(b)->byte[8] << 56 \
                | (type)(b)->byte[7] << 48 \
                | (type)(b)->byte[6] << 40 \
                | (type)(b)->byte[5] << 32 \
                | (type)(b)->byte[4] << 24 \
                | (type)(b)->byte[3] << 16 \
                | (type)(b)->byte[2] << 8 \
                | (type)(b)->byte[1]) \
			)

#   define __block2float32bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((__real32){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
                : ((__real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((__real64) \
                    { \
                        .byte[0] = (b)->byte[8], .byte[1] = (b)->byte[7], .byte[2] = (b)->byte[6], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[4], .byte[5] = (b)->byte[3], .byte[6] = (b)->byte[2], .byte[7] = (b)->byte[1], \
                    } \
                ).f \
                : ((__real64) \
                    { \
                        .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
                    } \
                ).f \
            )

#else //__BIG_ENDIAN__ //不再单独适配大端，大端存储需要先交换字节序

#   define __number2block8bit(n, flag) \
            (lineardb_t){ \
                LINEDB_TYPE_8BIT | LINEDB_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]) \
            }
#   define __block2number8it(b)    ((b)->byte[1])

#   define __number2block16bit(n, flag) \
            (lineardb_t){ \
                LINEDB_TYPE_16BIT | LINEDB_TYPE_BIGENDIAN | (flag),\
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block2number16bit(b) \
            (((b)->byte[0] & LINEDB_TYPE_BIGENDIAN) \
                ? ((b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 8 | (b)->byte[2]) \
			)       

#   define __number2block32bit(n, flag) \
            (lineardb_t){ \
                LINEDB_TYPE_32BIT | LINEDB_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block2number32bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
			)

#   define __number2block64bit(n, flag) \
            (lineardb_t){ \
                LINEDB_TYPE_64BIT | LINEDB_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __block2number64bit(b, type) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((type)(b)->byte[8] << 56 \
                | (type)(b)->byte[7] << 48 \
                | (type)(b)->byte[6] << 40 \
                | (type)(b)->byte[5] << 32 \
                | (type)(b)->byte[4] << 24 \
                | (type)(b)->byte[3] << 16 \
                | (type)(b)->byte[2] << 8 \
                | (type)(b)->byte[1]) \
                : ((type)(b)->byte[1] << 56 \
                | (type)(b)->byte[2] << 48 \
                | (type)(b)->byte[3] << 40 \
                | (type)(b)->byte[4] << 32 \
                | (type)(b)->byte[5] << 24 \
                | (type)(b)->byte[6] << 16 \
                | (type)(b)->byte[7] << 8 \
                | (type)(b)->byte[8]) \
			)

#   define __block2float32bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((__real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
                : ((__real32){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((__real64) \
                    { \
                        .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
                    } \
                ).f \
                : ((__real64) \
                    { \
                        .byte[0] = (b)->byte[8], .byte[1] = (b)->byte[7], .byte[2] = (b)->byte[6], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[4], .byte[5] = (b)->byte[3], .byte[6] = (b)->byte[2], .byte[7] = (b)->byte[1], \
                    } \
                ).f \
            )

#endif //__LITTLE_ENDIAN__



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

#define __objectis_binary(b)    (__typeof_linedb(b) == LINEDB_OBJECT_BINARY)
#define __objectis_string(b)    (__typeof_linedb(b) == LINEDB_OBJECT_STRING)

#define __numberis_float(b)     (__typeof_linedb(b) == LINEDB_NUMBER_FLOAT)
#define __numberis_integer(b)   (__typeof_linedb(b) == LINEDB_NUMBER_INTEGER)
#define __numberis_unsigned(b)  (__typeof_linedb(b) == LINEDB_NUMBER_UNSIGNED)
#define __numberis_boolean(b)   (__typeof_linedb(b) == LINEDB_NUMBER_BOOLEAN)

#define __numberis_8bit(b)      ((b)->byte[0] & LINEDB_TYPE_8BIT)
#define __numberis_16bit(b)     ((b)->byte[0] & LINEDB_TYPE_16BIT)
#define __numberis_32bit(b)     ((b)->byte[0] & LINEDB_TYPE_32BIT)
#define __numberis_64bit(b)     ((b)->byte[0] & LINEDB_TYPE_64BIT)

#define __sizeof_head(b)        (1 + (((b)->byte[0]) & LINEDB_HEAD_MASK))

#define __sizeof_data(b) \
        ( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? (((b)->byte[0] & LINEDB_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & LINEDB_TYPE_16BIT)) ? __b2n16(b) : (__b2n32(b)) \
        : 0 )

#define __sizeof_linedb(b) \
        ( __sizeof_head(b) + __sizeof_data(b))

#define __plan_sizeof_linedb(size) \
        (size < 0x100 ? (2 + size) : size < 0x10000 ? (3 + size) : (5 + size));

#define __dataof_linedb(b)       (&((b)->byte[0]) + __sizeof_head(b))


static inline void linedb_reset_type(linedb_t *ldb, char type)
{
    ldb->byte[0] &= (~LINEDB_TYPE_MASK);
    ldb->byte[0] |= type;
}


//使用宏现实这个函数，长度转字节用 number to byte
static inline uint64_t linedb_bind_buffer(linedb_t **ldb, const void *b, uint64_t s)
{
    *ldb = (linedb_t*)b;
    if (s < 0x100){
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = LINEDB_TYPE_8BIT | LINEDB_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = LINEDB_TYPE_8BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif
        (*ldb)->byte[1] = ((char*)&s)[0];

        return 2 + s;

    }else if (s < 0x10000){
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = LINEDB_TYPE_16BIT | LINEDB_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = LINEDB_TYPE_16BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif        
        (*ldb)->byte[1] = ((char*)&s)[0];
        (*ldb)->byte[2] = ((char*)&s)[1];

        return 3 + s;

    }else {
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = LINEDB_TYPE_32BIT | LINEDB_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = LINEDB_TYPE_32BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif            
        (*ldb)->byte[1] = ((char*)&s)[0];
        (*ldb)->byte[2] = ((char*)&s)[1];
        (*ldb)->byte[3] = ((char*)&s)[2];
        (*ldb)->byte[4] = ((char*)&s)[3];

        return 5 + s;
    }
    
    return 0;
}

static inline linedb_t* linedb_load_binary(linedb_t *db, const void *b, uint64_t  s)
{
    if (s < 0x100){
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = LINEDB_TYPE_8BIT | LINEDB_TYPE_OBJECT;
#else
        db->byte[0] = LINEDB_TYPE_8BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif
        db->byte[1] = ((char*)&s)[0];
        memcpy(&db->byte[2], b, s);

    }else if (s < 0x10000){
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = LINEDB_TYPE_16BIT | LINEDB_TYPE_OBJECT;
#else
        db->byte[0] = LINEDB_TYPE_16BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif        
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        memcpy(&db->byte[3], b, s);

    }else {
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = LINEDB_TYPE_32BIT | LINEDB_TYPE_OBJECT;
#else
        db->byte[0] = LINEDB_TYPE_32BIT | LINEDB_TYPE_OBJECT | LINEDB_TYPE_BIGENDIAN;
#endif            
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        db->byte[3] = ((char*)&s)[2];
        db->byte[4] = ((char*)&s)[3];
        memcpy(&db->byte[5], b, s);
    }
    return db;
}

static inline linedb_t* linedb_load_string(linedb_t *db, const char *s)
{
    uint64_t l = strlen(s);
    linedb_load_binary(db, (const char *)s, l + 1);
    db->byte[0] |= LINEDB_OBJECT_STRING;
    db->byte[(1 + (((db)->byte[0]) & LINEDB_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline linedb_t* string2inedb(const char *s)
{
    // fprintf(stdout, "strlen=%lu\n", strlen("1\n\0"));
    // output strlen=2
    uint64_t l = strlen(s);
    linedb_t *db = (linedb_t *)malloc(__LINEDB_HEAD_ALLOC_SIZE + l + 1);
    linedb_load_binary(db, (const char *)s, l + 1);
    db->byte[0] |= LINEDB_OBJECT_STRING;
    db->byte[(1 + (((db)->byte[0]) & LINEDB_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline linedb_t* linedb_create(const void *data, uint64_t size)
{
    linedb_t *db = (linedb_t *)malloc(__LINEDB_HEAD_ALLOC_SIZE + size);
    if (data){
        return linedb_load_binary(db, data, size);
    }
    linedb_bind_buffer(&db, db, size);
    return db;
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
    uint64_t ldb_size = __plan_sizeof_linedb(size); 
    while ((uint64_t)(lp->len - lp->write_pos + lp->read_pos) >= ldb_size) {

        if (lp->leftover < ldb_size){
            ldb_size = linedb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            lp->write_block->byte[0] = 0;
            lp->write_pos += lp->leftover;
            lp->leftover = lp->len;
        }else {
            ldb_size = linedb_bind_buffer(&lp->write_block, lp->buf + (lp->write_pos & (lp->len - 1)), size);
            memcpy(__dataof_linedb(lp->write_block), data, size);
            lp->write_pos += ldb_size;
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