#ifndef __LINEARDB_H__
#define __LINEARDB_H__


#include <stdint.h>

#define BLOCK_HEAD_MASK     0x0f

enum {
    BLOCK_TYPE_8BIT = 0x01,
    BLOCK_TYPE_16BIT = 0x02,
    BLOCK_TYPE_32BIT = 0x04,
    BLOCK_TYPE_64BIT = 0x08,
    BLOCK_TYPE_BIGENDIAN = 0x10,
    BLOCK_TYPE_FLOAT = 0x20,
    BLOCK_TYPE_ARRAY = 0x40,
    BLOCK_TYPE_OBJECT = 0x80
};

#define BLOCK_HEAD      16

typedef struct linear_data_block {
    unsigned char byte[BLOCK_HEAD];
}Lineardb;

#ifdef __LITTLE_ENDIAN__

#define __n2b8(n) \
            (Lineardb){ \
                BLOCK_TYPE_8BIT, \
                (((char*)&(n))[0]) \
            }
#define __b2n8(b)    ((b)->byte[1])

#   define __n2b16(n) \
            (Lineardb){ \
                BLOCK_TYPE_16BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }
#   define __b2n16(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[1] << 8 | (b)->byte[2]) \
                : ((b)->byte[2] << 8 | (b)->byte[1]) \
			)       

#   define __n2b32(n) \
            (Lineardb){ \
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
            (Lineardb){ \
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
                | (b)->byte[5] << 24 \
                | (b)->byte[6] << 16 \
                | (b)->byte[7] << 8 \
                | (b)->byte[8]) \
                : ((int64_t)(b)->byte[8] << 56 \
                | (int64_t)(b)->byte[7] << 48 \
                | (int64_t)(b)->byte[6] << 40 \
                | (int64_t)(b)->byte[5] << 32 \
                | (b)->byte[4] << 24 \
                | (b)->byte[3] << 16 \
                | (b)->byte[2] << 8 \
                | (b)->byte[1]) \
			)
#else //__BIG_ENDIAN__

#define __n2b8(n) \
            (Lineardb){ \
                BLOCK_TYPE_8BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]) \
            }
#define __b2n8(b)    ((b)->byte[1])

#   define __n2b16(n) \
            (Lineardb){ \
                BLOCK_TYPE_16BIT | BLOCK_TYPE_BIGENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }
#   define __b2n16(b) \
            (((b)->byte[0] & BLOCK_TYPE_BIGENDIAN) \
                ? ((b)->byte[2] << 8 | (b)->byte[1]) \
                : ((b)->byte[1] << 8 | (b)->byte[2]) \
			)       

#   define __n2b32(n) \
            (Lineardb){ \
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
            (Lineardb){ \
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
                | (b)->byte[4] << 24 \
                | (b)->byte[3] << 16 \
                | (b)->byte[2] << 8 \
                | (b)->byte[1]) \
                : ((int64_t)(b)->byte[1] << 56 \
                | (int64_t)(b)->byte[2] << 48 \
                | (int64_t)(b)->byte[3] << 40 \
                | (int64_t)(b)->byte[4] << 32 \
                | (b)->byte[5] << 24 \
                | (b)->byte[6] << 16 \
                | (b)->byte[7] << 8 \
                | (b)->byte[8]) \
			)

#endif //__LITTLE_ENDIAN__

static inline float __b2f32(Lineardb *b)
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

static inline float __b2f64(Lineardb *b)
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

static inline Lineardb* lineardb_bind_bytes(Lineardb *db, const char *b, uint32_t s)
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

static inline Lineardb* lineardb_bind_string(Lineardb *db, char *s)
{
    size_t l = strlen(s);
    lineardb_bind_bytes(db, s, l + 1);
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
}

static inline Lineardb* lineardb_load_string(const char *s)
{
    // fprintf(stdout, "strlen=%lu\n", strlen("1\0\0")); 
    // output strlen=1
    size_t l = strlen(s);
    Lineardb *db = (Lineardb *)malloc(BLOCK_HEAD + l + 1);
    lineardb_bind_bytes(db, s, l + 1);
    db->byte[(1 + (((db)->byte[0]) & BLOCK_HEAD_MASK)) + l] = '\0';
    return db;
}


#define __sizeof_head(b)   (1 + (((b)->byte[0]) & BLOCK_HEAD_MASK))

#define __sizeof_block(b) \
        ( __sizeof_head(b) + \
        ( (((b)->byte[0]) & BLOCK_TYPE_OBJECT) \
        ? (((b)->byte[0] & BLOCK_TYPE_8BIT)) ? __b2n8(b) \
        : (((b)->byte[0] & BLOCK_TYPE_16BIT)) ? __b2n16(b) : (__b2n32(b)) \
        : 0 ))

#define __byteof_block(b)   (&((b)->byte[0]) + __sizeof_head(b))


#endif //__LINEARDB_H__