#include <stdio.h>
#include "linear_data_block.h"

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");


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

	Lineardb block;
	int32_t n, i = 0x1a1b1c1d;
	
	block = __int32_to_block(i);
	n = __block_to_int32(&block);
	
	fprintf(stdout, "%x %x %x %x %x %x\n", i, n, block.byte[1], block.byte[2], block.byte[3], block.byte[4]);

	return 0;
}
