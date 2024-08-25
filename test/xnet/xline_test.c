#include "xnet/xline.h"
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
    __xlogd("bit shift: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, byte[0], byte[1], byte[2], byte[3]);

    __xlogd(">>>>------------>\n");
    
    swap_byte[0] = ((char*)&little_number)[0];
    swap_byte[1] = ((char*)&little_number)[1];
    swap_byte[2] = ((char*)&little_number)[2];
    swap_byte[3] = ((char*)&little_number)[3];
    swap_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __xlogd("swap byte: little to little => little number= %x swap number= %x byte= %x %x %x %x\n",
        little_number, swap_number, swap_byte[0],  swap_byte[1], swap_byte[2], swap_byte[3]);

    __xlogd(">>>>------------>\n");

    // little to big => swap byte
    swap_byte[0] = byte[3];
    swap_byte[1] = byte[2];
    swap_byte[2] = byte[1];
    swap_byte[3] = byte[0];

    // 小端的byte[3]存储的是0x1a //大端的byte[3]存储的是0x1d，
    // 所以同样左移24位，小端的高位地址存储的是数字高位，大端的高位地址存储的是数字的低位
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __xlogd("swap byte: little to big => little number=%x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    __xlogd("swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    __xlogd(">>>>------------>\n");

    // little to big => bit shift
    swap_byte[0] = little_number >> 24 & 0xff;
    swap_byte[1] = little_number >> 16 & 0xff;
    swap_byte[2] = little_number >> 8 & 0xff;
    swap_byte[3] = little_number & 0xff;
    big_number = swap_byte[3] << 24 | swap_byte[2] << 16 | swap_byte[1] << 8 | swap_byte[0];
    __xlogd("bit shift: little to big => little number= %x byte[]= %x %x %x %x\n", little_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    __xlogd("swap byte: little to big => little number=%x big number=%x\n", little_number, big_number);

    __xlogd(">>>>------------>\n");

    // big to byte => swap_byte
    swap_byte[0] = ((char*)&big_number)[0];
    swap_byte[1] = ((char*)&big_number)[1];
    swap_byte[2] = ((char*)&big_number)[2];
    swap_byte[3] = ((char*)&big_number)[3];
    __xlogd("bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    __xlogd("bit shift: big to little => little number=%x big number=%x\n", little_number, big_number);

    __xlogd(">>>>------------>\n");

    swap_byte[0] = big_number & 0xff;
    swap_byte[1] = big_number >> 8 & 0xff;
    swap_byte[2] = big_number >> 16 & 0xff;
    swap_byte[3] = big_number >> 24 & 0xff;
    __xlogd("bit shift: big to byte => big number= %x byte[]= %x %x %x %x\n", big_number, swap_byte[0], swap_byte[1], swap_byte[2], swap_byte[3]);
    little_number = swap_byte[0] << 24 | swap_byte[1] << 16 | swap_byte[2] << 8 | swap_byte[3];
    __xlogd("bit shift: big to little => big number=%x little number=%x\n", big_number, little_number);
    __xlogd(">>>>------------>\n");
}


static void build_msg(xlinekv_ptr lkv)
{
    xl_add_word(lkv, "api", "cmd");
    xl_add_word(lkv, "cmd", "join");
    xl_add_bin(lkv, "bin", "bin", slength("bin"));
    uint64_t ipos = xl_hold_kv(lkv, "int");
    xl_add_int(lkv, "int8", 8);
    xl_add_int(lkv, "int16", 16);
    xl_add_uint(lkv, "uint32", 32);
    xl_add_uint(lkv, "uint64", 64);
    uint64_t fpos = xl_hold_kv(lkv, "float");
    xl_add_float(lkv, "real32", 32.3232);
    xl_add_float(lkv, "real64", 64.6464);
    xl_save_kv(lkv, fpos);
    xl_save_kv(lkv, ipos);
    xl_add_uint(lkv, "uint64", 64);
    xl_add_float(lkv, "real64", 64.6464);

    uint64_t lpos = xl_hold_list(lkv, "list");
    for (int i = 0; i < 10; ++i){
        struct xline line = __xl_i2b(i);
        xl_list_append(lkv, &line);
    }
    xl_save_list(lkv, lpos);

    lpos = xl_hold_list(lkv, "list-tree");
    for (int i = 0; i < 10; ++i){
        ipos = xl_list_hold_kv(lkv);
        xl_add_word(lkv, "key", "tree");
        xl_add_int(lkv, "real32", i);
        xl_add_float(lkv, "real64", 64.6464 * i);
        xl_list_save_kv(lkv, ipos);
    }
    xl_save_list(lkv, lpos);
    
}

int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/xpeer/log", NULL);

    // test_byte_order();

    // __xlogd("strlen=%d\n", strlen("1\n"));
    // __xlogd("strlen=%d\n", strlen("1\n\0"));
    // char *src = "1234567890";
    // char dst[11] = {0};
    // memcpy(dst, src, 11);
    // __xlogd("memcpy dst = %s\n", dst);

    xlinekv_ptr lkv = xl_maker();
    build_msg(lkv);
    xl_update(lkv);

    xlinekv_t parser = xl_parser(&lkv->line);

    xline_ptr int_kv = xl_find(&parser, "int");
    xline_ptr list_kv = xl_find(&parser, "list-tree");

    xl_add_obj(lkv, "int_kv", int_kv);
    xl_add_obj(lkv, "list_kv", list_kv);
    xl_update(lkv);

    xl_printf(&lkv->line);

    xl_free(lkv);

    return 0;
}