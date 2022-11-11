#include "lineardb.h"
#include <stdio.h>

static void test_byte_order()
{
    uint16_t little16=0x1a1b, big16=0x1b1a;
    uint32_t little32=0x1a1b1c1d, big32=0x1d1c1b1a, *pnumber;
    uint32_t little_number;
    char byte[4];

    pnumber = (uint32_t*)byte;
    byte[0] = little32 & 0xff;
    byte[1] = little32 >> 8 & 0xff;
    byte[2] = little32 >> 16 & 0xff;
    byte[3] = little32 >> 24 & 0xff;
    // byte[3] = little32 & 0xff;
    // byte[2] = little32 >> 8 & 0xff;
    // byte[1] = little32 >> 16 & 0xff;
    // byte[0] = little32 >> 24 & 0xff;
    little_number = byte[3] << 24 | byte[2] << 16 | byte[1] << 8 | byte[0];
    fprintf(stdout, "little32 byte[0]= %x %x %x  %x %x %x %x\n", little32, *pnumber, little_number, byte[0], byte[1], byte[2], byte[3]);

    *pnumber = big32;
    // fprintf(stdout, "big32 byte[0]= %x      %x %x %x %x\n", big32, byte[0], byte[1], byte[2], byte[3]);

    little32 = little16;
    byte[0] = little32 & 0xff;
    byte[1] = little32 >> 8 & 0xff;
    byte[2] = little32 >> 16 & 0xff;
    byte[3] = little32 >> 24 & 0xff;
    // *pnumber &= 0x0000ffff;
    fprintf(stdout, "little16 byte[0]= %x %x        %x %x %x %x\n", little16, *pnumber, byte[0], byte[1], byte[2], byte[3]);

    // big32 = big16;
    // *pnumber = big32;
    // fprintf(stdout, "big16 byte[0]= %x      %x %x %x %x\n", big16, byte[0], byte[1], byte[2], byte[3]);

    *pnumber &= 0x0000ffff;
    fprintf(stdout, "big16 byte[0]= %x %x           %x %x %x %x\n", big16, *pnumber, byte[0], byte[1], byte[2], byte[3]);


    // Lineardb *ldb = (Lineardb *)malloc(BLOCK_HEAD + 0x1a1b);
    // char *bytes = (char*)malloc(0x1a1b);
    // bytes2block(ldb, bytes, 0x1a1b);
    // fprintf(stdout, "block size = %x\n",  __sizeof_block(ldb));
}

static void number16_to_ldb()
{
    int16_t n16, i16 = -12345; // big endian 14640 <=> little endian 12345, -14385 <=> -12345;
    Lineardb ldb = __n2b16(i16);
    n16 = __b2n16(&ldb);
    fprintf(stdout, "block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (uint8_t)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_block(&ldb);
    fprintf(stdout, "int16_t %d to linear block %d sizeof(block)=%u\n", i16, n16, size);

    int16_t u16, iu16 = 12345;
    ldb = __n2b16(iu16);
    u16 = __b2n16(&ldb);
    size = __sizeof_block(&ldb);
    fprintf(stdout, "uint16_t %u to linear block %u sizeof(block)=%u\n", iu16, u16, size);
}

static void number32_to_ldb()
{
    int32_t n32, i32 = -12345;
    Lineardb ldb = __n2b32(i32);
    fprintf(stdout, "block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (uint8_t)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_block(&ldb);
    n32 = __b2n32(&ldb);
    fprintf(stdout, "int32_t %d to linear block %d sizeof(block)=%u\n", i32, n32, size);

    uint32_t u32, iu32 = 12345;
    ldb = __n2b32(iu32);
    size = __sizeof_block(&ldb);
    u32 = __b2n32(&ldb);
    fprintf(stdout, "uint32_t %u to linear block %u sizeof(block)=%u\n", iu32, u32, size);
}

static void number64_to_ldb()
{
    int64_t n64, i64 = -12345;
    Lineardb ldb = __n2b64(i64);
    fprintf(stdout, "block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (uint8_t)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_block(&ldb);
    n64 = __b2n64(&ldb);
    fprintf(stdout, "int64_t %lld to linear block %lld sizeof(block)=%u\n", i64, n64, size);

    uint64_t u64, iu64 = 12345;
    ldb = __n2b64(iu64);
    size = __sizeof_block(&ldb);
    u64 = __b2n64(&ldb);
    fprintf(stdout, "uint64_t %llu to linear block %llu sizeof(block)=%u\n", iu64, u64, size);
}


static void string_to_ldb()
{
    const char *string8bit = "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    Lineardb *ldb = string2block(string8bit);
    fprintf(stdout, "string %s\n===> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string8bit, __byteof_block(ldb), __sizeof_head(ldb), __sizeof_block(ldb));
    free(ldb);

    const char *string16bit = "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                        "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                        "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                        "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                        "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    ldb = string2block(string16bit);
    fprintf(stdout, "string %s\n===> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string16bit, __byteof_block(ldb), __sizeof_head(ldb), __sizeof_block(ldb));
    free(ldb);

    uint32_t len = 0xabcdef;
    ldb = malloc(BLOCK_HEAD + len);
    char *data = malloc(len);
    bytes2block(ldb, data, len);
    fprintf(stdout, "data size=%u + %u sizeof(block)=%u\n", len, __sizeof_head(ldb), __sizeof_block(ldb));
    free(ldb);
}


void lineardb_test()
{
    // test_byte_order();
    number16_to_ldb();
    number32_to_ldb();
    number64_to_ldb();
    string_to_ldb();
}