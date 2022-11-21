#ifndef __LINEARDB_H__
#define __LINEARDB_H__


#include <stdint.h>

#define BLOCK_HEAD_MASK     0x0f

enum {
    BLOCK_TYPE_8BIT = 0x01,
    BLOCK_TYPE_16BIT = 0x02,
    BLOCK_TYPE_32BIT = 0x04,
    BLOCK_TYPE_64BIT = 0x08,
    BLOCK_TYPE_OBJECT = 0x10,
    BLOCK_TYPE_BIGENDIAN = 0x20,
    BLOCK_TYPE_INTEGER,
    BLOCK_TYPE_UNSIGNED,
    BLOCK_TYPE_FLOAT,
    BLOCK_TYPE_ARRAY
};

#define BLOCK_HEAD      16

typedef struct linear_data_block {
    uint8_t byte[BLOCK_HEAD];
}lineardb_t;

#ifdef __LITTLE_ENDIAN__

#define __n2b8(n) \
            (lineardb_t){ \
                BLOCK_TYPE_8BIT, \
                (((char*)&(n))[0]) \
            }
#define __b2n8(b)    ((b)->byte[1])

#   define __n2b16(n) \
            (lineardb_t){ \
                BLOCK_TYPE_16BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }
#   define __b2n16(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 8 | (b)->byte[2]) \
                : ((b)->byte[2] << 8 | (b)->byte[1]) \
			)       

#   define __n2b32(n) \
            (lineardb_t){ \
                BLOCK_TYPE_32BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __b2n32(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
                : ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
			)

#   define __n2b64(n) \
            (lineardb_t){ \
                BLOCK_TYPE_64BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __b2n64(b) \
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
    
#else //__BIG_ENDIAN__

#define __n2b8(n) \
            (lineardb_t){ \
                BLOCK_TYPE_8BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]) \
            }
#define __b2n8(b)    ((b)->byte[1])

#   define __n2b16(n) \
            (lineardb_t){ \
                BLOCK_TYPE_16BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }
#   define __b2n16(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 8 | (b)->byte[2]) \
			)       

#   define __n2b32(n) \
            (lineardb_t){ \
                BLOCK_TYPE_32BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __b2n32(b) \
            ((((b)->byte[0]) & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[4] << 24 | (b)->byte[3] << 16 | (b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 24 | (b)->byte[2] << 16 | (b)->byte[3] << 8 | (b)->byte[4]) \
			)

#   define __n2b64(n) \
            (lineardb_t){ \
                BLOCK_TYPE_64BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), \
                (((char*)&(n))[2]), (((char*)&(n))[3]), \
                (((char*)&(n))[4]), (((char*)&(n))[5]), \
                (((char*)&(n))[6]), (((char*)&(n))[7]) \
            }

#   define __b2n64(b) \
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

#endif //__LITTLE_ENDIAN__

static inline lineardb_t n2b8(int8_t n)
{
    return __n2b8(n);
}

static inline lineardb_t n2b16(int16_t n)
{
    return __n2b16(n);
}

static inline lineardb_t n2b32(int32_t n)
{
    return __n2b32(n);
}

static inline lineardb_t n2b64(int64_t n)
{
    return __n2b64(n);
}

static inline lineardb_t f2b32(float f)
{
    return __n2b32(f);
}

static inline lineardb_t f2b64(double f)
{
    return __n2b64(f);
}

static inline int8_t b2n8(lineardb_t *b)
{
    return __b2n8(b);
}

static inline int16_t b2n16(lineardb_t *b)
{
    return __b2n16(b);
}

static inline int32_t b2n32(lineardb_t *b)
{
    return __b2n32(b);
}

static inline int64_t b2n64(lineardb_t *b)
{
    return __b2n64(b);
}

static inline float b2f32(lineardb_t *b)
{
    float f;
#ifdef __LITTLE_ENDIAN__
    if ((((char*)(b))[0]) & BLOCK_TYPE_BIGENDIAN) {
        ((char*)&(f))[0] = ((char*)(b))[4];
        ((char*)&(f))[1] = ((char*)(b))[3];
        ((char*)&(f))[2] = ((char*)(b))[2];
        ((char*)&(f))[3] = ((char*)(b))[1];
    } else {
        ((char*)&(f))[0] = ((char*)(b))[1];
        ((char*)&(f))[1] = ((char*)(b))[2];
        ((char*)&(f))[2] = ((char*)(b))[3];
        ((char*)&(f))[3] = ((char*)(b))[4];
    }
#else    
    if ((((char*)(b))[0]) & BLOCK_TYPE_BIGENDIAN) {
        ((char*)&(f))[0] = ((char*)(b))[1];
        ((char*)&(f))[1] = ((char*)(b))[2];
        ((char*)&(f))[2] = ((char*)(b))[3];
        ((char*)&(f))[3] = ((char*)(b))[4];
    } else {
        ((char*)&(f))[0] = ((char*)(b))[4];
        ((char*)&(f))[1] = ((char*)(b))[3];
        ((char*)&(f))[2] = ((char*)(b))[2];
        ((char*)&(f))[3] = ((char*)(b))[1];
    }
#endif
    return f;
}

static inline double b2f64(lineardb_t *b)
{
    double f;
#ifdef __LITTLE_ENDIAN__
    if ((((char*)(b))[0]) & BLOCK_TYPE_BIGENDIAN) {
        ((char*)&(f))[0] = ((char*)(b))[8];
        ((char*)&(f))[1] = ((char*)(b))[7];
        ((char*)&(f))[2] = ((char*)(b))[6];
        ((char*)&(f))[3] = ((char*)(b))[5];
        ((char*)&(f))[4] = ((char*)(b))[4];
        ((char*)&(f))[5] = ((char*)(b))[3];
        ((char*)&(f))[6] = ((char*)(b))[2];
        ((char*)&(f))[7] = ((char*)(b))[1];
    } else {
        ((char*)&(f))[0] = ((char*)(b))[1];
        ((char*)&(f))[1] = ((char*)(b))[2];
        ((char*)&(f))[2] = ((char*)(b))[3];
        ((char*)&(f))[3] = ((char*)(b))[4];
        ((char*)&(f))[4] = ((char*)(b))[5];
        ((char*)&(f))[5] = ((char*)(b))[6];
        ((char*)&(f))[6] = ((char*)(b))[7];
        ((char*)&(f))[7] = ((char*)(b))[8];
    }
#else    
    if ((((char*)(b))[0]) & BLOCK_TYPE_BIGENDIAN) {
        ((char*)&(f))[0] = ((char*)(b))[1];
        ((char*)&(f))[1] = ((char*)(b))[2];
        ((char*)&(f))[2] = ((char*)(b))[3];
        ((char*)&(f))[3] = ((char*)(b))[4];
        ((char*)&(f))[4] = ((char*)(b))[5];
        ((char*)&(f))[5] = ((char*)(b))[6];
        ((char*)&(f))[6] = ((char*)(b))[7];
        ((char*)&(f))[7] = ((char*)(b))[8];
    } else {
        ((char*)&(f))[0] = ((char*)(b))[8];
        ((char*)&(f))[1] = ((char*)(b))[7];
        ((char*)&(f))[2] = ((char*)(b))[6];
        ((char*)&(f))[3] = ((char*)(b))[5];
        ((char*)&(f))[4] = ((char*)(b))[4];
        ((char*)&(f))[5] = ((char*)(b))[3];
        ((char*)&(f))[6] = ((char*)(b))[2];
        ((char*)&(f))[7] = ((char*)(b))[1];
    }
#endif
    return f;
}


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define __sizeof_head(b)   (1 + (((b)->byte[0]) & BLOCK_HEAD_MASK))
#define __sizeof_data(b) \
        ( (((b)->byte[0]) & BLOCK_TYPE_OBJECT) \
        ? (((b)->byte[0] & BLOCK_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & BLOCK_TYPE_16BIT)) ? __b2n16(b) : (__b2n32(b)) \
        : 0 )

#define __sizeof_block(b) \
        ( __sizeof_head(b) + \
        ( (((b)->byte[0]) & BLOCK_TYPE_OBJECT) \
        ? (((b)->byte[0] & BLOCK_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & BLOCK_TYPE_16BIT)) ? __b2n16(b) : (__b2n32(b)) \
        : 0 ))

#define __dataof_block(b)   (&((b)->byte[0]) + __sizeof_head(b))

#define __plan_sizeof_block(size) \
        (size < 0x100 ? (2 + size) : size < 0x10000 ? (3 + size) : (5 + size));

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

static inline lineardb_t* lineardb_load_data(lineardb_t *db, const void *b, uint32_t s)
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
    lineardb_load_data(db, (const uint8_t *)s, l + 1);
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline lineardb_t* lineardb_build_with_string(const char *s)
{
    // fprintf(stdout, "strlen=%lu\n", strlen("1\0\0")); 
    // output strlen=1
    size_t l = strlen(s);
    lineardb_t *db = (lineardb_t *)malloc(BLOCK_HEAD + l + 1);
    lineardb_load_data(db, (const uint8_t *)s, l + 1);
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
    return db;
}

static inline lineardb_t* lineardb_build_with_data(const void *b, size_t size)
{
    lineardb_t *db = (lineardb_t *)malloc(BLOCK_HEAD + size);
    return lineardb_load_data(db, b, size);
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