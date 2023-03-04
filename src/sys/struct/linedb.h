#ifndef __LINEDB_H__
#define __LINEDB_H__


#include <env/env.h>


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
    LINEDB_OBJECT_ARRAY = 0x80,
    LINEDB_OBJECT_BINARY = 0xc0
};


#define LINEDB_HEAD_MAX_SIZE      9


typedef struct linedb {
    uint8_t byte[LINEDB_HEAD_MAX_SIZE];
}*linedb_ptr;


union real32 {
    float f;
    char byte[4];
};

union real64 {
    double f;
    char byte[8];
};



#define __number_to_byte_8bit(n, flag) \
        (struct linedb){ \
            LINEDB_TYPE_8BIT | (flag), \
            (((char*)&(n))[0]) \
        }

#define __byte_to_number_8bit(b, type) \
        ((type)(b)->byte[1])



#define __number_to_byte_16bit(n, flag) \
        (struct linedb){ \
            LINEDB_TYPE_16BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]) \
        }

#define __byte_to_number_16bit(b, type) \
        ((type)(b)->byte[2] << 8 | (type)(b)->byte[1])  



#define __number_to_byte_32bit(n, flag) \
        (struct linedb){ \
            LINEDB_TYPE_32BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]) \
        }

#define __byte_to_number_32bit(b, type) \
        ((type)(b)->byte[4] << 24 | (type)(b)->byte[3] << 16 | (type)(b)->byte[2] << 8 | (type)(b)->byte[1])



#define __number_to_byte_64bit(n, flag) \
        (struct linedb){ \
            LINEDB_TYPE_64BIT | (flag), \
            (((char*)&(n))[0]), (((char*)&(n))[1]), \
            (((char*)&(n))[2]), (((char*)&(n))[3]), \
            (((char*)&(n))[4]), (((char*)&(n))[5]), \
            (((char*)&(n))[6]), (((char*)&(n))[7]) \
        }

#define __byte_to_number_64bit(b, type) \
        ( (type)(b)->byte[8] << 56 | (type)(b)->byte[7] << 48 | (type)(b)->byte[6] << 40 | (type)(b)->byte[5] << 32 \
        | (type)(b)->byte[4] << 24 | (type)(b)->byte[3] << 16 | (type)(b)->byte[2] << 8 | (type)(b)->byte[1] )



// #define __byte_to_float_32bit(b) \
//         (((union real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f)

// #define __byte_to_float_64bit(b) \
//         (((union real64) \
//             { \
//                 .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
//                 .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
//             } \
//         ).f)

static inline float __byte_to_float_32bit(linedb_ptr db)
{
    union real32 r;
    r.byte[0] = db->byte[1];
    r.byte[1] = db->byte[2];
    r.byte[2] = db->byte[3];
    r.byte[3] = db->byte[4];
    return r.f;
}

static inline float __byte_to_float_64bit(linedb_ptr db)
{
    union real64 r;
    r.byte[0] = db->byte[1];
    r.byte[1] = db->byte[2];
    r.byte[2] = db->byte[3];
    r.byte[3] = db->byte[4];
    r.byte[4] = db->byte[5];
    r.byte[5] = db->byte[6];
    r.byte[6] = db->byte[7];
    r.byte[7] = db->byte[8];
    return r.f;
}

#define __n2b8(n)        __number_to_byte_8bit(n, LINEDB_NUMBER_INTEGER)
#define __n2b16(n)       __number_to_byte_16bit(n, LINEDB_NUMBER_INTEGER)
#define __n2b32(n)       __number_to_byte_32bit(n, LINEDB_NUMBER_INTEGER)
#define __n2b64(n)       __number_to_byte_64bit(n, LINEDB_NUMBER_INTEGER)

#define __b2n8(b)        __byte_to_number_8bit(b, int8_t)
#define __b2n16(b)       __byte_to_number_16bit(b, int16_t)
#define __b2n32(b)       __byte_to_number_32bit(b, int32_t)
#define __b2n64(b)       __byte_to_number_64bit(b, int64_t)

#define __u2b8(n)        __number_to_byte_8bit(n, LINEDB_NUMBER_UNSIGNED)
#define __u2b16(n)       __number_to_byte_16bit(n, LINEDB_NUMBER_UNSIGNED)
#define __u2b32(n)       __number_to_byte_32bit(n, LINEDB_NUMBER_UNSIGNED)
#define __u2b64(n)       __number_to_byte_64bit(n, LINEDB_NUMBER_UNSIGNED)

#define __b2u8(b)        __byte_to_number_8bit(b, uint8_t)
#define __b2u16(b)       __byte_to_number_16bit(b, uint16_t)
#define __b2u32(b)       __byte_to_number_32bit(b, uint32_t)
#define __b2u64(b)       __byte_to_number_64bit(b, uint64_t)

#define __f2b32(f)       __number_to_byte_32bit(f, LINEDB_NUMBER_FLOAT)
#define __f2b64(f)       __number_to_byte_64bit(f, LINEDB_NUMBER_FLOAT)

#define __b2f32(b)       __byte_to_float_32bit(b)
#define __b2f64(b)       __byte_to_float_64bit(b)

#define __bool_to_byte(b)       __number_to_byte_8bit(b, LINEDB_NUMBER_BOOLEAN)
#define __byte_to_bool(b)       __byte_to_number_8bit(b, uint8_t)

#define __typeis_object(b)      ((b)->byte[0] & LINEDB_TYPE_OBJECT)
#define __typeis_number(b)      (!((b)->byte[0] & LINEDB_TYPE_OBJECT))
#define __typeof_linedb(b)      ((b)->byte[0] & LINEDB_TYPE_MASK)

#define __objectis_custom(b)    (__typeof_linedb(b) == LINEDB_OBJECT_CUSTOM)
#define __objectis_string(b)    (__typeof_linedb(b) == LINEDB_OBJECT_STRING)
#define __objectis_array(b)     (__typeof_linedb(b) == LINEDB_OBJECT_ARRAY)
#define __objectis_binary(b)    (__typeof_linedb(b) == LINEDB_OBJECT_BINARY)

#define __numberis_integer(b)   (__typeof_linedb(b) == LINEDB_NUMBER_INTEGER)
#define __numberis_unsigned(b)  (__typeof_linedb(b) == LINEDB_NUMBER_UNSIGNED)
#define __numberis_float(b)     (__typeof_linedb(b) == LINEDB_NUMBER_FLOAT)
#define __numberis_boolean(b)   (__typeof_linedb(b) == LINEDB_NUMBER_BOOLEAN)

#define __numberis_8bit(b)      ((b)->byte[0] & LINEDB_TYPE_8BIT)
#define __numberis_16bit(b)     ((b)->byte[0] & LINEDB_TYPE_16BIT)
#define __numberis_32bit(b)     ((b)->byte[0] & LINEDB_TYPE_32BIT)
#define __numberis_64bit(b)     ((b)->byte[0] & LINEDB_TYPE_64BIT)



#define __sizeof_head(b) \
        (uint64_t)( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? 1 + (((b)->byte[0]) & LINEDB_HEAD_MASK) \
        : 1 )

#define __sizeof_data(b) \
        (uint64_t)( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? (((b)->byte[0] & LINEDB_TYPE_8BIT)) ? __b2u8(b) \
        : (((b)->byte[0] & LINEDB_TYPE_16BIT)) ? __b2u16(b) \
        : (((b)->byte[0] & LINEDB_TYPE_32BIT)) ? __b2u32(b) \
        : __b2u64(b) : (((b)->byte[0]) & LINEDB_HEAD_MASK) )

#define __sizeof_linedb(b)        (( __sizeof_head(b) + __sizeof_data(b)))
#define __dataof_linedb(b)        (&((b)->byte[0]) + __sizeof_head(b))

#define __planof_malloc(size) \
        ( (size) < 0x100 ? (2 + (size)) \
        : (size) < 0x10000 ? (3 + (size)) \
        : (size) < 0x100000000 ? (5 + (size)) \
        : (9 + (size)) )

#define __linedb_free(ldb) \
        if (ldb->byte[0] & LINEDB_TYPE_OBJECT) { \
            free(ldb); \
            ldb = NULL; \
        }


static inline linedb_ptr linedb_filled_data(linedb_ptr ldb, const void *data, uint64_t size, uint8_t flag)
{
    if (size < 0x100){
        *ldb = __number_to_byte_8bit(size, flag);
        memcpy(&ldb->byte[2], data, size);
    }else if (size < 0x10000){
        *ldb = __number_to_byte_16bit(size, flag);
        memcpy(&ldb->byte[3], data, size);
    }else if (size < 0x100000000){
        *ldb = __number_to_byte_32bit(size, flag);
        memcpy(&ldb->byte[5], data, size);
    }else {
        *ldb = __number_to_byte_64bit(size, flag);
        memcpy(&ldb->byte[9], data, size);
    }
    return ldb;
}

static inline linedb_ptr linedb_from_string(const char *str)
{
    uint64_t len = strlen(str) + 1;
    linedb_ptr ldb = (linedb_ptr)malloc(__planof_malloc((len)));
    linedb_filled_data(ldb, str, len, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_STRING);
    *(ldb->byte + (__sizeof_head(ldb) + len - 1)) = '\0';
    return ldb;
}

static inline linedb_ptr linedb_from_object(void *obj, uint64_t size, uint8_t flag)
{
    linedb_ptr ldb = (linedb_ptr)malloc(__planof_malloc(size));
    linedb_filled_data(ldb, obj, size, flag);
    return ldb;
}


#endif //__LINEDB_H__