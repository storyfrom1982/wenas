#ifndef __LINEAR_DATA_BLOCK_H__
#define __LINEAR_DATA_BLOCK_H__

#include <stdint.h>

enum {
    BLOCK_TYPE_NUMBER_8BIT = 1,
    BLOCK_TYPE_NUMBER_16BIT = 2,
    BLOCK_TYPE_NUMBER_32BIT = 4,
    BLOCK_TYPE_NUMBER_64BIT = 8,
    BLOCK_TYPE_BLOCK_SIZE_8BIT,
    BLOCK_TYPE_BLOCK_SIZE_16BIT,
    BLOCK_TYPE_BLOCK_SIZE_32BIT,
    BLOCK_TYPE_BLOCK_SIZE_64BIT,
    BLOCK_TYPE_OBJECT_SIZE_64BIT,
    BLOCK_TYPE_BLOCK_ARRAY_SIZE_64BIT
};

typedef struct linear_data_block {
    char byte[16];
}Lineardb;


#define __block_to_int32(b)   (int32_t)((b)->byte[1] | (b)->byte[2] << 8 | (b)->byte[3] << 16 | (b)->byte[4] << 24)
#define __int32_to_block(i)   (Lineardb){BLOCK_TYPE_NUMBER_32BIT, (char)((i) & 0xff), (char)((i) >> 8 & 0xff), (char)(((i) >> 16) & 0xff), (char)(((i) >> 24) & 0xff)}

#endif
