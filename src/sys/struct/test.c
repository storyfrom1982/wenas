#include "linear_data_block.h"
#include "linear_key_value.h"

#include <stdio.h>

int test_linearkv()
{
	Linearkv *lkv = linearkv_create(1024 * 1024);
	Lineardb *key, *v, value, *b = &value;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(buf, 1024, "hello world %d", i);
		key = bytes2block(buf, n);
		n = (int32_t)rand();
		// fprintf(stdout, "%d n= %d\n", i, n);
		value = __number32_to_block(n);
		// fprintf(stdout, "%d b= %d\n", i, __block_to_number32(b).i);
		linearkv_append(lkv, key, &value);
		free(key);
	}

	for (int k = 0; k < 100; ++k){
		n = snprintf(buf, 1024, "hello world %d", k);
		key = bytes2block(buf, n);
		v = linearkv_find(lkv, key);
		free(key);
		if (v){
			fprintf(stdout, "key %s -> value %d\n", __block_byte(key), __block_to_number32(v).i);
		}
	}

	fprintf(stdout, "strlen(1) = %lu\n", strlen("1\0\0"));

	return 0;
}

int test_lineardb()
{
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

	int64_t n64, i64 = 0x1a1b1c1d1a1b1c1d;
	block = __number64_to_block(i64);
	n64 = __block_to_number64(b).i;

	size =	__block_size(b);

	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "%lx %lx %x %x %x %x %x %x %x %x\n", i64, n64, b->byte[1], b->byte[2], b->byte[3], b->byte[4], b->byte[5], b->byte[6], b->byte[7], b->byte[8]);

	i64 = 123456789;
	block = __number64_to_block(i64);
	n64 = __block_to_number64(b).i;

	size =	__block_size(b);

	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "%ld %ld %x %x %x %x %x %x %x %x\n", i64, n64, b->byte[1], b->byte[2], b->byte[3], b->byte[4], b->byte[5], b->byte[6], b->byte[7], b->byte[8]);

	float fn32, fi32 = 1234.5678f;
	block = __number32_to_block(fi32);
	fn32 = __block_to_float32(b);

	size =	__block_size(b);

	if (fn32 == fi32){
		fprintf(stdout, "size ===== %u\n", size);
	}
	fprintf(stdout, "%f %f %x %x %x %x\n", fi32, fn32, b->byte[1], b->byte[2], b->byte[3], b->byte[4]);

	double fn64, fi64 = 7654321.1234567f;
	block = __number64_to_block(fi64);
	fn64 = __block_to_float64(b);

	size =	__block_size(b);

	if (fn64 == fi64){
		fprintf(stdout, "size ===== %u\n", size);
	}
	fprintf(stdout, "%.5llf %.5llf %x %x %x %x %x %x %x %x\n", fi64, fn64, b->byte[1], b->byte[2], b->byte[3], b->byte[4], b->byte[5], b->byte[6], b->byte[7], b->byte[8]);

	b = string2block("Hello World");

	size =	__block_size(b);
	fprintf(stdout, "size ===== %u\n", size);
	fprintf(stdout, "byte ===== %s\n", __block_byte(b));
	fprintf(stdout, "byte ===== %s\n", __block_byte(b));

	return 0;
}