#include "lineardb.h"
#include <stdio.h>

static void test_byte_order()
{
    //  大端字节序
    //  地址增长方向  →
    //	0x0A	0x0B	0x0C	0x0D
    //  示例中，最高位字节是0x0A 存储在最低的内存地址处。下一个字节0x0B存在后面的地址处。正类似于十六进制字节从左到右的阅读顺序。

    //  小端字节序
    //  地址增长方向  →
    //  0x0D	0x0C	0x0B	0x0A
    //  最低位字节是0x0D 存储在最低的内存地址处。后面字节依次存在后面的地址处。

    uint32_t little_number=0x1a1b1c1d, big_number, swap_number;
    char byte[4], swap_byte[4];

    byte[0] = little_number & 0xff;
    byte[1] = little_number >> 8 & 0xff;
    byte[2] = little_number >> 16 & 0xff;
    byte[3] = little_number >> 24 & 0xff;
    swap_number = byte[3] << 24 | byte[2] << 16 | byte[1] << 8 | byte[0];
    fprintf(stdout, "bit shift: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, byte[0], byte[1], byte[2], byte[3]);

    fprintf(stdout, "==================================\n");
    
    swap_byte[0] = ((char*)&little_number)[0];
    swap_byte[1] = ((char*)&little_number)[1];
    swap_byte[2] = ((char*)&little_number)[2];
    swap_byte[3] = ((char*)&little_number)[3];
    swap_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    fprintf(stdout, "swap byte: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, swap_byte[0],  swap_byte[1], swap_byte[2], swap_byte[3]);

    fprintf(stdout, "==================================\n");

    // little to big => swap byte
    swap_byte[0] = byte[3];
    swap_byte[1] = byte[2];
    swap_byte[2] = byte[1];
    swap_byte[3] = byte[0];

    // 小端的byte[3]存储的是0x1a //大端的byte[3]存储的是0x1d，
    // 所以同样左移24位，小端的高位地址存储的是数字高位，大端的高位地址存储的是数字的低位
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    fprintf(stdout, "swap byte: little to big => little number=%x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    fprintf(stdout, "swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    fprintf(stdout, "==================================\n");

    // little to big => bit shift
    swap_byte[0] = little_number >> 24 & 0xff;
    swap_byte[1] = little_number >> 16 & 0xff;
    swap_byte[2] = little_number >> 8 & 0xff;
    swap_byte[3] = little_number & 0xff;
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    fprintf(stdout, "bit shift: little to big => little number= %x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    fprintf(stdout, "swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    fprintf(stdout, "==================================\n");

    // big to byte => swap_byte
    swap_byte[0] = ((char*)&big_number)[0];
    swap_byte[1] = ((char*)&big_number)[1];
    swap_byte[2] = ((char*)&big_number)[2];
    swap_byte[3] = ((char*)&big_number)[3];
    fprintf(stdout, "bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    fprintf(stdout, "bit shift: big to little => little number=%x big number=%x\n", little_number, big_number);

    fprintf(stdout, "==================================\n");

    swap_byte[0] = big_number & 0xff;
    swap_byte[1] = big_number >> 8 & 0xff;
    swap_byte[2] = big_number >> 16 & 0xff;
    swap_byte[3] = big_number >> 24 & 0xff;
    fprintf(stdout, "bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    fprintf(stdout, "bit shift: big to little => big number=%x little number=%x\n", big_number, little_number);
}

static void number16_to_ldb()
{
    fprintf(stdout, "==================================\n");

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
    fprintf(stdout, "==================================\n");

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
    fprintf(stdout, "==================================\n");

    int64_t n64, i64 = -12345;
    Lineardb ldb = __n2b64(i64);
    fprintf(stdout, "block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (uint8_t)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_block(&ldb);
    n64 = __b2n64(&ldb);
    fprintf(stdout, "int64_t %ld to linear block %ld sizeof(block)=%u\n", i64, n64, size);

    uint64_t u64, iu64 = 12345;
    ldb = __n2b64(iu64);
    size = __sizeof_block(&ldb);
    u64 = __b2n64(&ldb);
    fprintf(stdout, "uint64_t %lu to linear block %lu sizeof(block)=%u\n", iu64, u64, size);
}


static void string_to_ldb()
{
    fprintf(stdout, "==================================\n");

    const char *string8bit = "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    Lineardb *ldb = lineardb_load_string(string8bit);
    fprintf(stdout, "string %s\n===> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string8bit, __byteof_block(ldb), __sizeof_head(ldb), __sizeof_block(ldb));
    free(ldb);

    fprintf(stdout, "==================================\n");

    const char *string16bit =   "\nHello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

    ldb = lineardb_load_string(string16bit);
    fprintf(stdout, "string %s\n===> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string16bit, __byteof_block(ldb), __sizeof_head(ldb), __sizeof_block(ldb));
    free(ldb);

    uint32_t len = 0xabcdef;
    ldb = malloc(BLOCK_HEAD + len);
    char *data = malloc(len);
    lineardb_bind_bytes(ldb, data, len);
    fprintf(stdout, "data size=%u + %u sizeof(block)=%u\n", len, __sizeof_head(ldb), __sizeof_block(ldb));
    free(data);
    free(ldb);
}


static void float_to_ldb()
{
    fprintf(stdout, "==================================\n");

    float f32 = 12345.12345f, fn32;
    Lineardb ldb = __n2b32(f32);
    fn32 = b2f32(&ldb);
    fprintf(stdout, "float=%f ldb=%f\n", f32, fn32);

    double f64 = 123456.123456f, fn64;
    ldb = __n2b64(f64);
    fn64 = b2f64(&ldb);
    fprintf(stdout, "double=%lf ldb=%lf\n", f64, fn64);
}


void lineardb_test()
{
    test_byte_order();
    number16_to_ldb();
    number32_to_ldb();
    number64_to_ldb();
    string_to_ldb();
    float_to_ldb();
}