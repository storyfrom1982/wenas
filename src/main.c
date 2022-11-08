#include <stdio.h>
#include "linear_data_block.h"

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");

#ifdef __LITTLE_ENDIAN__
	fprintf(stdout, "__LITTLE_ENDIAN__\n");
#endif


	float fa = 1234.5678;
	float fb;
	char bb[4];
	char *pa = (char*)&fa;
	char *pb = (char*)&fb;

	//
	//低位存储到低位是小端，高位存储到低位是大端
	//
	bb[0] = *pa++;
	bb[1] = *pa++;
	bb[2] = *pa++;
	bb[3] = *pa++;

	*pb++ = bb[0];
	*pb++ = bb[1];
	*pb++ = bb[2];
	*pb++ = bb[3];

	//
	// https://www.zhihu.com/question/364159510
	// %.3f 是输出小数点后 3 位小数
	// 输出 1234.568 四舍五入了，了解详情请参考上方连接
	//
	fprintf(stdout, "float to bytes %.3f -> %.3f\n", fa, fb);

	int i1 = 0x1a1b1c1d;
	int i2;
	pa = (char*)&i1;

	fprintf(stdout, "%x %x %x %x\n", pa[0], pa[1], pa[2], pa[3]);

	bb[0] = i1 & 0xff;
	bb[1] = i1 >> 8 & 0xff;
	bb[2] = i1 >> 16 & 0xff;
	bb[3] = i1 >> 24 & 0xff;

	fprintf(stdout, "%x %x %x %x\n", bb[0], bb[1], bb[2], bb[3]);

	//
	// 内存地址	bb[0]	bb[1]	bb[2]	bb[3]
	// 输出 	1d 	1c	1b 	1a
	// 数字的低位在内存的低位，小映射到小为小端。
	//

	Lineardb block, *b = &block;

	int16_t n16, i16 = 0x1a1b;
	block = __number16_to_block(i16);
	n16 = __block_to_number16(b).i;

	uint32_t size =	__block_size(b);

	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "%x %x %x %x %x %x\n", i16, n16, b->byte[1], b->byte[2], b->byte[3], b->byte[4]);


	int32_t n32, i32 = 0x1a1b1c1d;
	block = __number32_to_block(i32);
	n32 = __block_to_number32(b).i;

	size =	__block_size(b);

	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "%x %x %x %x %x %x\n", i32, n32, b->byte[1], b->byte[2], b->byte[3], b->byte[4]);

	int64_t n64, n = 0x1a1b1c1d1a1b1c1d;
	block = __number64_to_block(n);
	n64 = __block_to_number64(b).i;
	// n64 = ((Number64)(((int64_t)(b)->byte[8]) | ((int64_t)(b)->byte[7] << 8) | ((int64_t)(b)->byte[6] << 16) | ((int64_t)(b)->byte[5] << 24) 
	// 	| ((int64_t)(b)->byte[4] << 32) | ((int64_t)(b)->byte[3] << 40) | ((int64_t)(b)->byte[2] << 48) | ((int64_t)(b)->byte[1] << 56))).i;
	// n64 = (((b)->byte[0] & 0x80) ? (Number64)((b)->byte[8] | (b)->byte[7] << 8 | (b)->byte[6] << 16 | (b)->byte[5] << 24 | (b)->byte[4] << 32 | (b)->byte[3] << 40 | (b)->byte[2] << 48 | (b)->byte[1] << 56)
    //         : (Number64)((b)->byte[1] | (b)->byte[2] << 8 | (b)->byte[3] << 16 | (b)->byte[4] << 24 | (b)->byte[5] << 32 | (b)->byte[6] << 40 | (b)->byte[7] << 48 | (b)->byte[8] << 56));

	size =	__block_size(b);

	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "%llx %llx %x %x %x %x %x %x %x %x\n", n, n64, b->byte[1], b->byte[2], b->byte[3], b->byte[4], b->byte[5], b->byte[6], b->byte[7], b->byte[8]);

	return 0;
}
