#ifndef __LINEARDB_H__
#define __LINEARDB_H__


#include <stdint.h>
#include <string.h>
#include <stdlib.h>


#define BLOCK_HEAD_MASK     0x0f
#define BLOCK_TYPE_MASK     0xc0

enum {
    BLOCK_TYPE_8BIT = 0x01,
    BLOCK_TYPE_16BIT = 0x02,
    BLOCK_TYPE_32BIT = 0x04,
    BLOCK_TYPE_64BIT = 0x08,
    BLOCK_TYPE_OBJECT = 0x10,
    BLOCK_TYPE_BIGENDIAN = 0x20,
};

enum {
    BLOCK_TYPE_INTEGER = 0x00,
    BLOCK_TYPE_UNSIGNED = 0x40,
    BLOCK_TYPE_FLOAT = 0x80,
    BLOCK_TYPE_BOOLEAN = 0xc0
};

enum {
    BLOCK_TYPE_BINARY = 0x00,
    BLOCK_TYPE_STRING = 0x40,
    BLOCK_TYPE_BLOCK = 0x80,
    BLOCK_TYPE_ARRAY = 0xc0
};


#define BLOCK_HEAD      16



typedef struct linear_data_block {
    uint8_t byte[BLOCK_HEAD];
}lineardb_t;

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
            (lineardb_t){ \
                BLOCK_TYPE_8BIT | (flag), \
                (((char*)&(n))[0]) \
            }

#   define __block2number8bit(b)    ((b)->byte[1])

#   define __number2block16bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_16BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block2number16bit(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 8 | (b)->byte[2]) \
                : ((b)->byte[2] << 8 | (b)->byte[1]) \
			)       

#   define __number2block32bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_32BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block2number32bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
                : ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
			)

#   define __number2block64bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_64BIT | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __block2number64bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((int64_t)(b)->byte[1] << 56 \
                | (int64_t)(b)->byte[2] << 48 \
                | (int64_t)(b)->byte[3] << 40 \
                | (int64_t)(b)->byte[4] << 32 \
                | (int64_t)(b)->byte[5] << 24 \
                | (int64_t)(b)->byte[6] << 16 \
                | (int64_t)(b)->byte[7] << 8 \
                | (int64_t)(b)->byte[8]) \
                : ((int64_t)(b)->byte[8] << 56 \
                | (int64_t)(b)->byte[7] << 48 \
                | (int64_t)(b)->byte[6] << 40 \
                | (int64_t)(b)->byte[5] << 32 \
                | (int64_t)(b)->byte[4] << 24 \
                | (int64_t)(b)->byte[3] << 16 \
                | (int64_t)(b)->byte[2] << 8 \
                | (int64_t)(b)->byte[1]) \
			)

#   define __block2float32bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((float32_t){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
                : ((float32_t){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
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
                BLOCK_TYPE_8BIT | BLOCK_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]) \
            }
#   define __block2number8it(b)    ((b)->byte[1])

#   define __number2block16bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_16BIT | BLOCK_TYPE_BIGENDIAN | (flag),\
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block2number16bit(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 8 | (b)->byte[2]) \
			)       

#   define __number2block32bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_32BIT | BLOCK_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block2number32bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
			)

#   define __number2block64bit(n, flag) \
            (lineardb_t){ \
                BLOCK_TYPE_64BIT | BLOCK_TYPE_BIGENDIAN | (flag), \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __block2number64bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((int64_t)(b)->byte[8] << 56 \
                | (int64_t)(b)->byte[7] << 48 \
                | (int64_t)(b)->byte[6] << 40 \
                | (int64_t)(b)->byte[5] << 32 \
                | (int64_t)(b)->byte[4] << 24 \
                | (int64_t)(b)->byte[3] << 16 \
                | (int64_t)(b)->byte[2] << 8 \
                | (int64_t)(b)->byte[1]) \
                : ((int64_t)(b)->byte[1] << 56 \
                | (int64_t)(b)->byte[2] << 48 \
                | (int64_t)(b)->byte[3] << 40 \
                | (int64_t)(b)->byte[4] << 32 \
                | (int64_t)(b)->byte[5] << 24 \
                | (int64_t)(b)->byte[6] << 16 \
                | (int64_t)(b)->byte[7] << 8 \
                | (int64_t)(b)->byte[8]) \
			)

#   define __block2float32bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((float32_t){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f \
                : ((float32_t){.byte[0] = (b)->byte[4], .byte[1] = (b)->byte[3], .byte[2] = (b)->byte[2], .byte[3] = (b)->byte[1]}).f \
            )

#   define __block2float64bit(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
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



#define __n2b8(n)        __number2block8bit(n, BLOCK_TYPE_INTEGER)
#define __b2n8(b)        __block2number8bit(b)
#define __n2b16(n)       __number2block16bit(n, BLOCK_TYPE_INTEGER)
#define __b2n16(b)       __block2number16bit(b)
#define __n2b32(n)       __number2block32bit(n, BLOCK_TYPE_INTEGER)
#define __b2n32(b)       __block2number32bit(b)
#define __n2b64(n)       __number2block64bit(n, BLOCK_TYPE_INTEGER)
#define __b2n64(b)       __block2number64bit(b)

#define __u2b8(n)        __number2block8bit(n, BLOCK_TYPE_UNSIGNED)
#define __b2u8(b)        __block2number8bit(b)
#define __u2b16(n)       __number2block16bit(n, BLOCK_TYPE_UNSIGNED)
#define __b2u16(b)       __block2number16bit(b)
#define __u2b32(n)       __number2block32bit(n, BLOCK_TYPE_UNSIGNED)
#define __b2u32(b)       __block2number32bit(b)
#define __u2b64(n)       __number2block64bit(n, BLOCK_TYPE_UNSIGNED)
#define __b2u64(b)       __block2number64bit(b)

#define __f2b32(f)       __number2block32bit(f, BLOCK_TYPE_FLOAT)
#define __b2f32(b)       __block2float32bit(b)
#define __f2b64(f)       __number2block64bit(f, BLOCK_TYPE_FLOAT)
#define __b2f64(b)       __block2float64bit(b)

#define __boolean2block(b)       __number2block8bit(b, BLOCK_TYPE_BOOLEAN)
#define __block2boolean(b)       __block2number8bit(b)



#define __typeis_object(b)      ((b)->byte[0] & BLOCK_TYPE_OBJECT)
#define __typeis_number(b)      (!((b)->byte[0] & BLOCK_TYPE_OBJECT))

#define __typeof_block(b)       ((b)->byte[0] & BLOCK_TYPE_MASK)

#define __sizeof_head(b)        (1 + (((b)->byte[0]) & BLOCK_HEAD_MASK))
#define __dataof_block(b)       (&((b)->byte[0]) + __sizeof_head(b))

#define __sizeof_data(b) \
        ( (((b)->byte[0]) & BLOCK_TYPE_OBJECT) \
        ? (((b)->byte[0] & BLOCK_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & BLOCK_TYPE_16BIT)) ? __b2n16(b) : (__b2n32(b)) \
        : 0 )

#define __sizeof_block(b) \
        ( __sizeof_head(b) + __sizeof_data(b))

#define __plan_sizeof_block(size) \
        (size < 0x100 ? (2 + size) : size < 0x10000 ? (3 + size) : (5 + size));




static inline void lineardb_reset_type(lineardb_t *ldb, uint8_t type)
{
    ldb->byte[0] &= (~BLOCK_TYPE_MASK);
    ldb->byte[0] |= type;
}

static inline uint32_t lineardb_bind_buffer(lineardb_t **ldb, const void *b, uint32_t s)
{
    *ldb = (lineardb_t*)b;
    if (s < 0x100){
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = BLOCK_TYPE_8BIT | BLOCK_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = BLOCK_TYPE_8BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif
        (*ldb)->byte[1] = ((char*)&s)[0];

        return 2 + s;

    }else if (s < 0x10000){
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = BLOCK_TYPE_16BIT | BLOCK_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = BLOCK_TYPE_16BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif        
        (*ldb)->byte[1] = ((char*)&s)[0];
        (*ldb)->byte[2] = ((char*)&s)[1];

        return 3 + s;

    }else {
#ifdef __LITTLE_ENDIAN__
        (*ldb)->byte[0] = BLOCK_TYPE_32BIT | BLOCK_TYPE_OBJECT;
#else
        (*ldb)->byte[0] = BLOCK_TYPE_32BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif            
        (*ldb)->byte[1] = ((char*)&s)[0];
        (*ldb)->byte[2] = ((char*)&s)[1];
        (*ldb)->byte[3] = ((char*)&s)[2];
        (*ldb)->byte[4] = ((char*)&s)[3];

        return 5 + s;
    }
    
    return 0;
}

static inline lineardb_t* lineardb_load_binary(lineardb_t *db, const void *b, uint32_t s)
{
    if (s < 0x100){
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = BLOCK_TYPE_8BIT | BLOCK_TYPE_OBJECT;
#else
        db->byte[0] = BLOCK_TYPE_8BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif
        // db->byte[1] = s & 0xff;
        db->byte[1] = ((char*)&s)[0];
        memcpy(&db->byte[2], b, s);

    }else if (s < 0x10000){
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = BLOCK_TYPE_16BIT | BLOCK_TYPE_OBJECT;
#else
        db->byte[0] = BLOCK_TYPE_16BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif        
        // db->byte[1] = s & 0xff;
        // db->byte[2] = s >> 8 & 0xff;
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        memcpy(&db->byte[3], b, s);

    }else {
#ifdef __LITTLE_ENDIAN__
        db->byte[0] = BLOCK_TYPE_32BIT | BLOCK_TYPE_OBJECT;
#else
        db->byte[0] = BLOCK_TYPE_32BIT | BLOCK_TYPE_OBJECT | BLOCK_TYPE_BIGENDIAN;
#endif            
        // *(uint32_t*)&db->byte[1] = s;
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        db->byte[3] = ((char*)&s)[2];
        db->byte[4] = ((char*)&s)[3];
        memcpy(&db->byte[5], b, s);
    }
    return db;
}

static inline lineardb_t* lineardb_load_string(lineardb_t *db, const char *s)
{
    size_t l = strlen(s);
    lineardb_load_binary(db, (const uint8_t *)s, l + 1);
    db->byte[0] |= BLOCK_TYPE_STRING;
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline lineardb_t* lineardb_build_with_string(const char *s)
{
    // fprintf(stdout, "strlen=%lu\n", strlen("1\0\0")); 
    // output strlen=1
    size_t l = strlen(s);
    lineardb_t *db = (lineardb_t *)malloc(BLOCK_HEAD + l + 1);
    lineardb_load_binary(db, (const uint8_t *)s, l + 1);
    db->byte[0] |= BLOCK_TYPE_STRING;
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline lineardb_t* lineardb_build_with_binary(const void *b, size_t size)
{
    lineardb_t *db = (lineardb_t *)malloc(BLOCK_HEAD + size);
    return lineardb_load_binary(db, b, size);
}

static inline void lineardb_destroy(lineardb_t **pp_ldb)
{
    if (pp_ldb && *pp_ldb){
        lineardb_t *ldb = *pp_ldb;
        *pp_ldb = NULL;
        if (ldb->byte[0] & BLOCK_TYPE_OBJECT){
            free(ldb);
        }
    }
}


#endif //__LINEARDB_H__