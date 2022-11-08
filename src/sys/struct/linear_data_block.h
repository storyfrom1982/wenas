#ifndef __LINEAR_DATA_BLOCK_H__
#define __LINEAR_DATA_BLOCK_H__


#include <stdint.h>

enum {
    BLOCK_TYPE_NUMBER_8BIT = 0x01,
    BLOCK_TYPE_NUMBER_16BIT = 0x02,
    BLOCK_TYPE_NUMBER_32BIT = 0x04,
    BLOCK_TYPE_NUMBER_64BIT = 0x08,
    BLOCK_TYPE_STRING_8BIT = 0x10,
    BLOCK_TYPE_STRING_16BIT = 0x20,
    BLOCK_TYPE_STRING_32BIT = 0x40,
    BLOCK_BYTES_ORDER_BIG_ENDIAN = 0x80
};

typedef struct linear_data_block {
    char byte[16];
}Lineardb;

typedef union number_16bit{
    int16_t i;
    uint16_t u;
}Number16;

typedef union number_32bit{
    float f;
    int32_t i;
    uint32_t u;
}Number32;

typedef union number_64bit{
    double f;
    int64_t i;
    uint64_t u;
}Number64;



#ifdef __LITTLE_ENDIAN__

#   define __number16_to_block(n)  (Lineardb){BLOCK_TYPE_NUMBER_16BIT, (char)((n) & 0xff), (char)((n) >> 8 & 0xff)}
#   define __block_to_number16(b) \
            (((b)->byte[0] & 0x80) ? (Number32)((b)->byte[2] | (b)->byte[1] << 8) : (Number32)((b)->byte[1] | (b)->byte[2] << 8))

#   define __number32_to_block(n)  (Lineardb){BLOCK_TYPE_NUMBER_32BIT, (char)((n) & 0xff), (char)((n) >> 8 & 0xff), (char)(((n) >> 16) & 0xff), (char)(((n) >> 24) & 0xff)}
#   define __block_to_number32(b) \
            (((b)->byte[0] & 0x80) ? (Number32)((b)->byte[4] | (b)->byte[3] << 8 | (b)->byte[2] << 16 | (b)->byte[1] << 24) : (Number32)((b)->byte[1] | (b)->byte[2] << 8 | (b)->byte[3] << 16 | (b)->byte[4] << 24))

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

#   define __number16_to_block(n)  (Lineardb){BLOCK_TYPE_NUMBER_16BIT | BLOCK_BYTES_ORDER_BIG_ENDIAN, (char)((n) & 0xff), (char)((n) >> 8 & 0xff)}
#   define __block_to_number16(b) \
            (((b)->byte[0] & 0x80) ? (Number32)((b)->byte[1] | (b)->byte[2] << 8) : (Number32)((b)->byte[2] | (b)->byte[1] << 8))

#   define __number32_to_block(n)  (Lineardb){BLOCK_TYPE_NUMBER_32BIT | BLOCK_BYTES_ORDER_BIG_ENDIAN, \
            (char)((n) & 0xff), (char)((n) >> 8 & 0xff), (char)(((n) >> 16) & 0xff), (char)(((n) >> 24) & 0xff)}
#   define __block_to_number32(b) \
            (((b)->byte[0] & 0x80) ? (Number64)((b)->byte[1] | (b)->byte[2] << 8 | (b)->byte[3] << 16 | (b)->byte[4] << 24) : (Number64)((b)->byte[4] | (b)->byte[3] << 8 | (b)->byte[2] << 16 | (b)->byte[1] << 24))

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

#endif


#define __block_size(b)     \
            ((b)->byte[0] & (~0x80)) > 8 ? (((b)->byte[0] & (~0x80)) == BLOCK_TYPE_STRING_8BIT) \
			: (((b)->byte[0] & (~0x80)) == BLOCK_TYPE_STRING_8BIT) ? ((b)->byte[1] + 1) \
            : (((b)->byte[0] & (~0x80)) == BLOCK_TYPE_STRING_16BIT) ? ((__block_to_number16(b)).u + 1) \
            : (((b)->byte[0] & (~0x80)) == BLOCK_TYPE_STRING_32BIT) ? ((__block_to_number32(b)).u + 1) \
            : ((b)->byte[0] + 1)


#endif