#ifndef __LINEARDB_H__
#define __LINEARDB_H__


#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#define LINEDB_HEAD_MASK     0x0f
#define LINEDB_TYPE_MASK     0xc0


enum {
    LINEDB_TYPE_8BIT = 0x01,
    LINEDB_TYPE_16BIT = 0x02,
    LINEDB_TYPE_32BIT = 0x04,
    LINEDB_TYPE_64BIT = 0x08,
    LINEDB_TYPE_OBJECT = 0x10,
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
    uint8_t byte[__LINEDB_HEAD_ALLOC_SIZE];
}linedb_t;

typedef union {
    float f;
    uint8_t byte[4];
}float32_t;

typedef union {
    double f;
    uint8_t byte[8];
}float64_t;



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
                ? ((float32_t){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
                : ((float32_t){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((float64_t) \
                    { \
                        .byte[0] = (b)->byte[8], .byte[1] = (b)->byte[7], .byte[2] = (b)->byte[6], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[4], .byte[5] = (b)->byte[3], .byte[6] = (b)->byte[2], .byte[7] = (b)->byte[1], \
                    } \
                ).f \
                : ((float64_t) \
                    { \
                        .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
                    } \
                ).f \
            )

#else //__BIG_ENDIAN__

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
                ? ((float32_t){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
                : ((float32_t){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & LINEDB_TYPE_BIGENDIAN) \
                ? ((float64_t) \
                    { \
                        .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                        .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
                    } \
                ).f \
                : ((float64_t) \
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


static inline void linedb_reset_type(linedb_t *ldb, uint8_t type)
{
    ldb->byte[0] &= (~LINEDB_TYPE_MASK);
    ldb->byte[0] |= type;
}

static inline uint32_t linedb_bind_buffer(linedb_t **ldb, const void *b, uint32_t s)
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

static inline linedb_t* linedb_load_binary(linedb_t *db, const void *b, uint32_t s)
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
    size_t l = strlen(s);
    linedb_load_binary(db, (const uint8_t *)s, l + 1);
    db->byte[0] |= LINEDB_OBJECT_STRING;
    db->byte[(1 + (((db)->byte[0]) & LINEDB_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline linedb_t* string2inedb(const char *s)
{
    // fprintf(stdout, "strlen=%lu\n", strlen("1\0\0")); 
    // output strlen=1
    size_t l = strlen(s);
    linedb_t *db = (linedb_t *)malloc(__LINEDB_HEAD_ALLOC_SIZE + l + 1);
    linedb_load_binary(db, (const uint8_t *)s, l + 1);
    db->byte[0] |= LINEDB_OBJECT_STRING;
    db->byte[(1 + (((db)->byte[0]) & LINEDB_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline linedb_t* linedb_build(const void *b, size_t size)
{
    linedb_t *db = (linedb_t *)malloc(__LINEDB_HEAD_ALLOC_SIZE + size);
    return linedb_load_binary(db, b, size);
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


#endif //__LINEARDB_H__