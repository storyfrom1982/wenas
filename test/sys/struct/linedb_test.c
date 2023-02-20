#include "sys/struct/linedb.h"
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
    __logd("bit shift: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, byte[0], byte[1], byte[2], byte[3]);

    __logd(">>>>------------>\n");
    
    swap_byte[0] = ((char*)&little_number)[0];
    swap_byte[1] = ((char*)&little_number)[1];
    swap_byte[2] = ((char*)&little_number)[2];
    swap_byte[3] = ((char*)&little_number)[3];
    swap_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __logd("swap byte: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, swap_byte[0],  swap_byte[1], swap_byte[2], swap_byte[3]);

    __logd(">>>>------------>\n");

    // little to big => swap byte
    swap_byte[0] = byte[3];
    swap_byte[1] = byte[2];
    swap_byte[2] = byte[1];
    swap_byte[3] = byte[0];

    // 小端的byte[3]存储的是0x1a //大端的byte[3]存储的是0x1d，
    // 所以同样左移24位，小端的高位地址存储的是数字高位，大端的高位地址存储的是数字的低位
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __logd("swap byte: little to big => little number=%x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    __logd("swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    __logd(">>>>------------>\n");

    // little to big => bit shift
    swap_byte[0] = little_number >> 24 & 0xff;
    swap_byte[1] = little_number >> 16 & 0xff;
    swap_byte[2] = little_number >> 8 & 0xff;
    swap_byte[3] = little_number & 0xff;
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __logd("bit shift: little to big => little number= %x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    __logd("swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    __logd(">>>>------------>\n");

    // big to byte => swap_byte
    swap_byte[0] = ((char*)&big_number)[0];
    swap_byte[1] = ((char*)&big_number)[1];
    swap_byte[2] = ((char*)&big_number)[2];
    swap_byte[3] = ((char*)&big_number)[3];
    __logd("bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    __logd("bit shift: big to little => little number=%x big number=%x\n", little_number, big_number);

    __logd(">>>>------------>\n");

    swap_byte[0] = big_number & 0xff;
    swap_byte[1] = big_number >> 8 & 0xff;
    swap_byte[2] = big_number >> 16 & 0xff;
    swap_byte[3] = big_number >> 24 & 0xff;
    __logd("bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    __logd("bit shift: big to little => big number=%x little number=%x\n", big_number, little_number);
    __logd(">>>>------------>\n");
}

static void number16_to_ldb()
{
    int16_t n16, i16 = -12345; // big endian 14640 <=> little endian 12345, -14385 <=> -12345;
    struct linedb ldb = __n2b16(i16);
    n16 = __b2n16(&ldb);
    __logd("block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (char)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_linedb(&ldb);
    __logd("int16_t %d to linear block %d sizeof(block)=%u\n", i16, n16, size);

    uint16_t u16, iu16 = 12345;
    ldb = __n2b16(iu16);
    u16 = __b2n16(&ldb);
    size = __sizeof_linedb(&ldb);
    __logd("uint16_t %u to linear block %u sizeof(block)=%u\n", iu16, u16, size);

    __logd(">>>>------------>\n");
}

static void number32_to_ldb()
{
    int32_t n32, i32 = -12345;
    struct linedb ldb = __n2b32(i32);
    __logd("block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (char)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_linedb(&ldb);
    n32 = __b2n32(&ldb);
    __logd("__sint32 %d to linear block %d sizeof(block)=%u\n", i32, n32, size);

    uint32_t u32, iu32 = 12345;
    ldb = __n2b32(iu32);
    size = __sizeof_linedb(&ldb);
    u32 = __b2n32(&ldb);
    __logd("uint32_t %u to linear block %u sizeof(block)=%u\n", iu32, u32, size);

    __logd(">>>>------------>\n");
}

static void number64_to_ldb()
{
    int64_t n64, i64 = -12345;
    struct linedb ldb = __n2b64(i64);
    __logd("block head = 0x%x ldb.byte[0] >> 7 = 0x%x ldb.byte[0] & 0x80 = 0x%x\n", 
        ldb.byte[0], (char)(ldb.byte[0] >> 7), ldb.byte[0] & 0x80);

    uint32_t size = __sizeof_linedb(&ldb);
    n64 = __b2n64(&ldb);
    __logd("int64_t %ld to linear block %ld sizeof(block)=%u\n", i64, n64, size);

    uint64_t u64, iu64 = 12345;
    ldb = __n2b64(iu64);
    size = __sizeof_linedb(&ldb);
    u64 = __b2n64(&ldb);
    __logd("__uint64 %lu to linear block %lu sizeof(block)=%u\n", iu64, u64, size);

    __logd(">>>>------------>\n");
}


static void string_to_ldb()
{
    const char *string8bit = "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
    linedb_ptr ldb = linedb_from_string(string8bit);
    __logd("ldb type = %u\n", __typeof_linedb(ldb));
    __logd("string %s\n>>>>------------> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string8bit, __dataof_linedb(ldb), __sizeof_head(ldb), __sizeof_linedb(ldb));
    __linedb_free(ldb);

    __logd(">>>>------------>\n");

    const char *string16bit =   "\nHello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
                                "Hello World !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";

    ldb = linedb_from_string(string16bit);
    __logd("ldb type = %u\n", __typeof_linedb(ldb));
    __logd("string %s\n>>>>------------> to linear byteof(block) %s sizeof(head)=%u sizeof(block)=%u\n", 
        string16bit, __dataof_linedb(ldb), __sizeof_head(ldb), __sizeof_linedb(ldb));
    __linedb_free(ldb);

    __logd(">>>>------------>\n");

    uint32_t len = 0xabcdef;
    void *obj = malloc(len);
    ldb = linedb_from_object(obj, len, LINEDB_TYPE_OBJECT | LINEDB_OBJECT_BINARY);

    __logd("ldb type = %u\n", __typeof_linedb(ldb));
    __logd("data size=%lu + %u sizeof(block)=%lu\n", len, __sizeof_head(ldb), __sizeof_linedb(ldb));
    free(obj);
    __linedb_free(ldb);

    __logd(">>>>------------>\n");
}


static void float_to_ldb()
{
    float f32 = 12345.12345f, fn32;
    struct linedb ldb = __f2b32(f32);
    __logd("ldb type %u\n", __typeof_linedb(&ldb));

    fn32 = __b2f32(&ldb);
    __logd("float=%f ldb=%f\n", f32, fn32);

    __logd(">>>>------------>\n");

    double f64 = 123456.123456f, fn64;
    ldb = __f2b64(f64);
    __logd("ldb type %u\n", __typeof_linedb(&ldb));
    fn64 = __b2f64(&ldb);
    __logd("double=%lf ldb=%lf\n", f64, fn64);

    __logd(">>>>------------>\n");
}

static void test_lineardb_header()
{
    __logd("LINEDB_TYPE_8BIT | LINEDB_NUMBER_INTEGER=0x%x\n", (LINEDB_NUMBER_INTEGER) & LINEDB_TYPE_MASK);
    __logd("LINEDB_TYPE_8BIT | LINEDB_NUMBER_UNSIGNED=0x%x\n", (LINEDB_NUMBER_UNSIGNED) & LINEDB_TYPE_MASK);
    __logd("LINEDB_TYPE_8BIT | LINEDB_NUMBER_FLOAT=0x%x\n", (LINEDB_NUMBER_FLOAT) & LINEDB_TYPE_MASK);
    __logd("LINEDB_TYPE_8BIT | LINEDB_NUMBER_BOOLEAN=0x%x\n", (LINEDB_NUMBER_BOOLEAN) & LINEDB_TYPE_MASK);
    __logd(">>>>------------>\n");

    __logd("LINEDB_TYPE_8BIT | LINEDB_NUMBER_INTEGER=0x%x\n", (LINEDB_TYPE_8BIT | LINEDB_NUMBER_INTEGER) >> 6);
    __logd("LINEDB_TYPE_16BIT | LINEDB_NUMBER_UNSIGNED=0x%x\n", (LINEDB_TYPE_16BIT | LINEDB_NUMBER_UNSIGNED) >> 6);
    __logd("LINEDB_TYPE_32BIT | LINEDB_NUMBER_FLOAT=0x%x\n", (LINEDB_TYPE_32BIT | LINEDB_NUMBER_FLOAT) >> 6);
    __logd("LINEDB_TYPE_64BIT | LINEDB_NUMBER_BOOLEAN=0x%x\n", (LINEDB_TYPE_64BIT | LINEDB_NUMBER_BOOLEAN) >> 6);
    __logd(">>>>------------>\n");
}

void linedb_array_test(){

    linearray_ptr writer = linearray_create_writer();

    linedb_ptr ldb;
    char key_buf[1024];
    size_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d %d\0", i, rand());
		ldb = linedb_from_string(key_buf);
        // __logd("%s\n", __dataof_linedb(ldb));
        linearray_append(writer, ldb);
        __linedb_free(ldb);
	}

    struct linearray reader;
    linearray_load_reader(&reader, writer->head);
    do {
        ldb = linearray_next(&reader);
        if (ldb){
            __logd("%s\n", (char*)__dataof_linedb(ldb));
        }
    }while(ldb);
    
    linearray_free(&writer);
}


void lineardb_test()
{
    test_byte_order();
    number16_to_ldb();
    number32_to_ldb();
    number64_to_ldb();
    string_to_ldb();
    float_to_ldb();
    test_lineardb_header();

    linedb_array_test();
    __logd("strlen=%d\n", strlen("1\n\0"));
    char *src = "1234567890";
    char dst[11] = {0};
    memcpy(dst, src, 10);
    __logd("memcpy dst = %s\n", dst);
}