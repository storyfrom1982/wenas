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


#define LINEDB_STATIC_SIZE      9


typedef struct linedb {
    unsigned char byte[LINEDB_STATIC_SIZE];
}*linedb_ptr;


union real32 {
    float f;
    unsigned char byte[4];
};

union real64 {
    double f;
    unsigned char byte[8];
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



#define __byte_to_float_32bit(b) \
        (((union real32){.byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4]}).f)

#define __byte_to_float_64bit(b) \
        (((union real64) \
            { \
                .byte[0] = (b)->byte[1], .byte[1] = (b)->byte[2], .byte[2] = (b)->byte[3], .byte[3] = (b)->byte[4], \
                .byte[4] = (b)->byte[5], .byte[5] = (b)->byte[6], .byte[6] = (b)->byte[7], .byte[7] = (b)->byte[8], \
            } \
        ).f)



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



#define __linedb_head_size(b) \
        (uint64_t)( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? 1 + (((b)->byte[0]) & LINEDB_HEAD_MASK) \
        : 1 )

#define __linedb_data_size(b) \
        (uint64_t)( (((b)->byte[0] & LINEDB_TYPE_OBJECT)) \
        ? (((b)->byte[0] & LINEDB_TYPE_8BIT)) ? __b2u8(b) \
        : (((b)->byte[0] & LINEDB_TYPE_16BIT)) ? __b2u16(b) \
        : (((b)->byte[0] & LINEDB_TYPE_32BIT)) ? __b2u32(b) \
        : __b2u64(b) : (((b)->byte[0]) & LINEDB_HEAD_MASK) )

#define __linedb_data(b)        (&((b)->byte[0]) + __linedb_head_size(b))

#define __linedb_size(b)        (( __linedb_head_size(b) + __linedb_data_size(b)))

#define __linedb_sizeof(data_size) \
        ( (data_size) < 0x100 ? (2 + (data_size)) \
        : (data_size) < 0x10000 ? (3 + (data_size)) \
        : (data_size) < 0x100000000 ? (5 + (data_size)) \
        : (9 + (data_size)) )

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
    linedb_ptr ldb = (linedb_ptr)malloc(__linedb_sizeof((len)));
    linedb_filled_data(ldb, str, len, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_STRING);
    *(ldb->byte + (__linedb_head_size(ldb) + len - 1)) = '\0';
    return ldb;
}

static inline linedb_ptr linedb_from_object(void *obj, uint64_t size, uint8_t flag)
{
    linedb_ptr ldb = (linedb_ptr)malloc(__linedb_sizeof(size));
    linedb_filled_data(ldb, obj, size, flag);
    return ldb;
}




#define LINEARRAY_INIT_SIZE    65536 //64K


typedef struct linearray_writer {
    uint64_t pos, len;
    linedb_ptr head;
    linedb_ptr next;
}*linearray_writer_ptr;


static inline linearray_writer_ptr linearray_writer_create()
{
    linearray_writer_ptr la = (linearray_writer_ptr)malloc(sizeof(struct linearray_writer));
    la->head = (linedb_ptr) malloc(LINEARRAY_INIT_SIZE);
    // __logd("linearray_writer_create malloc addr = %X\n", la->head);
    la->len = LINEARRAY_INIT_SIZE;
    la->pos = 0;
    *la->head = __number_to_byte_64bit(la->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_ARRAY);
    // __logd("linearray_writer_create malloc addr = %X\n", la->head);
    la->next = (linedb_ptr)__linedb_data(la->head);
    // __logd("linearray_writer_create next addr = %X\n", la->next->byte);
    return la;
}

static inline void linearray_writer_free(linearray_writer_ptr *p_ptr)
{
    linearray_writer_ptr la = *p_ptr;
    *p_ptr = NULL;
    free(la->head);
    free(la);
}

static inline void linearray_writer_append(linearray_writer_ptr la, linedb_ptr ldb)
{
    // __logd("linearray_writer_append enter = %s\n", __linedb_data(ldb));
    uint64_t ldb_size = __linedb_size(ldb);
    while ((la->len - (__linedb_head_size(la->head) + la->pos)) < ldb_size){
        // __logd("linearray_writer_append next addr = %X\n", la->next);
        la->len += LINEARRAY_INIT_SIZE;
        void *p = malloc(la->len);
        // __logd("linearray_writer_append malloc addr = %X\n", p);
        memcpy(p, la->head->byte, __linedb_size(la->head));
        // __logd("linearray_writer_append free addr = %X\n", la->head);
        free(la->head);
        la->head = (linedb_ptr)p;
        la->next = (linedb_ptr)__linedb_data(la->head);
        // __logd("linearray_writer_append next 1 addr = %X\n", la->next);
    }
    
    memcpy(la->next->byte + la->pos, ldb->byte, ldb_size);
    // __logd("linearray_writer_append ldb len = %lu\n", __linedb_size((linedb_ptr)(la->next->byte + la->pos)));
    // __logd("linearray_writer_append ldb = %s\n", (char*)(__linedb_data((linedb_ptr)(la->next->byte + la->pos))));
    la->pos += ldb_size;
    // struct linedb tmp = __number_to_byte_64bit(la->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_ARRAY);
    // memcpy(la->head->byte, tmp.byte, __linedb_head_size(&tmp));
    *la->head = __number_to_byte_64bit(la->pos, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_ARRAY);
    // __logd("linearray_writer_append data = %s\n", (char*)__linedb_data((linedb_ptr)__linedb_data(la->head)));
}



typedef struct linearray_reader {
    uint64_t pos, len;
    linedb_ptr next;
}*linearray_reader_ptr;


static inline linearray_reader_ptr linearray_reader_load(linedb_ptr ldb)
{
    linearray_reader_ptr la = (linearray_reader_ptr)malloc(sizeof(struct linearray_reader));
    la->len = __linedb_data_size(ldb);
    // __logd("linearray_reader_load len = %u\n", la->len);
    // __logd("linearray_reader_load head size = %u\n", __linedb_head_size(ldb));
    la->pos = 0;
    la->next = (linedb_ptr)__linedb_data(ldb);
    // __logd("linearray_reader_load next addr = %X\n", la->next->byte);
    // __logd("linearray_reader_load ldb = %s\n", (char*)&la->next->byte[1]);
    return la;
}

static inline void linearray_reader_free(linearray_reader_ptr *p_ptr)
{
    linearray_reader_ptr la = *p_ptr;
    *p_ptr = NULL;
    free(la);
}

static inline linedb_ptr linearray_reader_next(linearray_reader_ptr la)
{
    if (la->len - la->pos > 0){
        linedb_ptr ldb = (linedb_ptr)(la->next->byte + la->pos);
        // __logd("linearray_reader_next ldb len = %lu\n", __linedb_size(ldb));
        // __logd("linearray_reader_next ldb = %s\n", __linedb_data(ldb));
        la->pos += __linedb_size(ldb);
        return ldb;
    }
    return NULL;
}


#endif //__LINEDB_H__