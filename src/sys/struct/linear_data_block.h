#ifndef __LINEAR_DATA_BLOCK_H__
#define __LINEAR_DATA_BLOCK_H__


#include <stdint.h>

enum {
    BLOCK_TYPE_OBJECT = 0x00,
    BLOCK_TYPE_NUMBER_8BIT = 0x01,
    BLOCK_TYPE_NUMBER_16BIT = 0x02,
    BLOCK_TYPE_NUMBER_32BIT = 0x04,
    BLOCK_TYPE_NUMBER_64BIT = 0x08,
    BLOCK_TYPE_STRING_8BIT = 0x10,
    BLOCK_TYPE_STRING_16BIT = 0x20,
    BLOCK_TYPE_STRING_32BIT = 0x40,
    BLOCK_BYTES_ORDER_BIG_ENDIAN = 0x80
};

#define BLOCK_HEAD      16

typedef struct linear_data_block {
    char byte[BLOCK_HEAD];
}Lineardb;

typedef union number_32bit{
    int32_t i;
    uint32_t u;
}Number32;

typedef union number_64bit{
    int64_t i;
    uint64_t u;
}Number64;



#ifdef __LITTLE_ENDIAN__

#   define __number16_to_block(n) \
            (Lineardb){ \
                BLOCK_TYPE_NUMBER_16BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block_to_number16(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number32)( \
                    ((int16_t)((char*)(b))[2]) \
                    | ((int16_t)((char*)(b))[1] << 8) \
                ) \
                : (Number32)( \
                    ((int16_t)((char*)(b))[1]) \
                    | ((int16_t)((char*)(b))[2] << 8) \
                ) \
			)

#   define __number32_to_block(n) \
            (Lineardb){ \
                BLOCK_TYPE_NUMBER_32BIT, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block_to_number32(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number32)( \
                    ((int32_t)((char*)(b))[4]) \
                    | ((int32_t)((char*)(b))[3] << 8) \
                    | ((int32_t)((char*)(b))[2] << 16) \
                    | ((int32_t)((char*)(b))[1] << 24) \
                ) \
                : (Number32)( \
                    ((int32_t)((char*)(b))[1]) \
                    | ((int32_t)((char*)(b))[2] << 8) \
                    | ((int32_t)((char*)(b))[3] << 16) \
                    | ((int32_t)((char*)(b))[4] << 24) \
                ) \
			)

#   define __number64_to_block(n) \
            (Lineardb){ \
				BLOCK_TYPE_NUMBER_64BIT, \
				(((char*)&(n))[0]), (((char*)&(n))[1]), (((char*)&(n))[2]), (((char*)&(n))[3]), \
				(((char*)&(n))[4]), (((char*)&(n))[5]), (((char*)&(n))[6]), (((char*)&(n))[7]) \
			}

#   define __block_to_number64(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number64)( \
                    ((int64_t)((char*)(b))[8]) \
                    | ((int64_t)((char*)(b))[7] << 8) \
                    | ((int64_t)((char*)(b))[6] << 16) \
                    | ((int64_t)((char*)(b))[5] << 24) \
                    | ((int64_t)((char*)(b))[4] << 32) \
                    | ((int64_t)((char*)(b))[3] << 40) \
                    | ((int64_t)((char*)(b))[2] << 48) \
                    | ((int64_t)((char*)(b))[1] << 56) \
                ) \
                : (Number64)( \
                    ((int64_t)((char*)(b))[1]) \
                    | ((int64_t)((char*)(b))[2] << 8) \
                    | ((int64_t)((char*)(b))[3] << 16) \
                    | ((int64_t)((char*)(b))[4] << 24) \
                    | ((int64_t)((char*)(b))[5] << 32) \
                    | ((int64_t)((char*)(b))[6] << 40) \
                    | ((int64_t)((char*)(b))[7] << 48) \
                    | ((int64_t)((char*)(b))[8] << 56) \
                ) \
			)

#else //__BIG_ENDIAN__

#   define __number16_to_block(n) \
            (Lineardb){ \
                BLOCK_TYPE_NUMBER_16BIT | BLOCK_BYTES_ORDER_BIG_ENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]) \
            }

#   define __block_to_number16(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number32)( \
                    ((int16_t)((char*)(b))[1]) \
                    | ((int16_t)((char*)(b))[2] << 8) \
                ) \
                : (Number32)( \
                    ((int16_t)((char*)(b))[2]) \
                    | ((int16_t)((char*)(b))[1] << 8) \
                ) \
			)

#   define __number32_to_block(n) \
            (Lineardb){ \
                BLOCK_TYPE_NUMBER_32BIT | BLOCK_BYTES_ORDER_BIG_ENDIAN, \
                (((char*)&(n))[0]), (((char*)&(n))[1]), (((char*)&(n))[2]), (((char*)&(n))[3]) \
            }

#   define __block_to_number32(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number32)( \
                    ((int32_t)((char*)(b))[1]) \
                    | ((int32_t)((char*)(b))[2] << 8) \
                    | ((int32_t)((char*)(b))[3] << 16) \
                    | ((int32_t)((char*)(b))[4] << 24) \
                ) \
                : (Number32)( \
                    ((int32_t)((char*)(b))[4]) \
                    | ((int32_t)((char*)(b))[3] << 8) \
                    | ((int32_t)((char*)(b))[2] << 16) \
                    | ((int32_t)((char*)(b))[1] << 24) \
                ) \
			)

#   define __number64_to_block(n) \
            (Lineardb){ \
				BLOCK_TYPE_NUMBER_64BIT | BLOCK_BYTES_ORDER_BIG_ENDIAN, \
				(((char*)&(n))[0]), (((char*)&(n))[1]), (((char*)&(n))[2]), (((char*)&(n))[3]), \
				(((char*)&(n))[4]), (((char*)&(n))[5]), (((char*)&(n))[6]), (((char*)&(n))[7]) \
			}

#   define __block_to_number64(b) \
            (((((char*)(b))[0]) & 0x80) \
                ? (Number64)( \
                    ((int64_t)((char*)(b))[1]) \
                    | ((int64_t)((char*)(b))[2] << 8) \
                    | ((int64_t)((char*)(b))[3] << 16) \
                    | ((int64_t)((char*)(b))[4] << 24) \
                    | ((int64_t)((char*)(b))[5] << 32) \
                    | ((int64_t)((char*)(b))[6] << 40) \
                    | ((int64_t)((char*)(b))[7] << 48) \
                    | ((int64_t)((char*)(b))[8] << 56) \
                ) \
                : (Number64)( \
                    ((int64_t)((char*)(b))[8]) \
                    | ((int64_t)((char*)(b))[7] << 8) \
                    | ((int64_t)((char*)(b))[6] << 16) \
                    | ((int64_t)((char*)(b))[5] << 24) \
                    | ((int64_t)((char*)(b))[4] << 32) \
                    | ((int64_t)((char*)(b))[3] << 40) \
                    | ((int64_t)((char*)(b))[2] << 48) \
                    | ((int64_t)((char*)(b))[1] << 56) \
                ) \
			)

#endif //__LITTLE_ENDIAN__

static inline float __block_to_float32(Lineardb *b)
{
    float f;
#ifdef __LITTLE_ENDIAN__
    if ((((char*)(b))[0]) & 0x80) {
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
    if ((((char*)(b))[0]) & 0x80) {
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

static inline float __block_to_float64(Lineardb *b)
{
    double f;
#ifdef __LITTLE_ENDIAN__
    if ((((char*)(b))[0]) & 0x80) {
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
    if ((((char*)(b))[0]) & 0x80) {
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

static inline Lineardb* bytes2block(const char *b, uint32_t s)
{
    Lineardb *db = (Lineardb *)malloc(BLOCK_HEAD + s);
    if (s < 0x100){
        db->byte[0] = BLOCK_TYPE_STRING_8BIT;
#ifdef __BIG_ENDIAN__
        db->byte[0] |= BLOCK_BYTES_ORDER_BIG_ENDIAN;
#endif
        db->byte[1] = s;
        memcpy(&db->byte[2], b, s);
        db->byte[2 + s] = '\0';
    }else if (s < 0x10000){
        db->byte[0] = BLOCK_TYPE_STRING_16BIT;
#ifdef __BIG_ENDIAN__
        db->byte[0] |= BLOCK_BYTES_ORDER_BIG_ENDIAN;
#endif
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        memcpy(&db->byte[3], b, s);
        db->byte[3 + s] = '\0';
    }else {
        db->byte[0] = BLOCK_TYPE_STRING_32BIT;
#ifdef __BIG_ENDIAN__
        db->byte[0] |= BLOCK_BYTES_ORDER_BIG_ENDIAN;
#endif        
        db->byte[1] = ((char*)&s)[0];
        db->byte[2] = ((char*)&s)[1];
        db->byte[3] = ((char*)&s)[2];
        db->byte[4] = ((char*)&s)[3];
        memcpy(&db->byte[5], b, s);
        db->byte[5 + s] = '\0';
    }
    return db;
}

static inline Lineardb* string2block(const char *s)
{
    return bytes2block(s, strlen(s));
}

#define __block_size(b) \
            (((char*)(b))[0] & (~0x80)) > 8 \
            ? ((((char*)(b))[0] & (~0x80)) == BLOCK_TYPE_STRING_8BIT) ? (((char*)(b))[1] + 2) \
            : ((((char*)(b))[0] & (~0x80)) == BLOCK_TYPE_STRING_16BIT) ? ((__block_to_number16(b)).u + 3) \
            : ((__block_to_number32(b)).u + 5) \
            : (((char*)(b))[0] & (~0x80)) == 0 \
            ? ((__block_to_number32(b)).u + 5) \
            : (((char*)(b))[0] + 1)

#define __block_byte(b) \
            (((char*)(b))[0] & (~0x80)) > 8 \
            ? ((((char*)(b))[0] & (~0x80)) == BLOCK_TYPE_STRING_8BIT) ? &(((char*)(b))[2]) \
            : ((((char*)(b))[0] & (~0x80)) == BLOCK_TYPE_STRING_16BIT) ? &(((char*)(b))[3]) \
            : &(((char*)(b))[5]) \
            : (((char*)(b))[0] & (~0x80)) == 0 \
            ? &(((char*)(b))[5]) \
            : &(((char*)(b))[1])





#endif //__LINEAR_DATA_BLOCK_H__