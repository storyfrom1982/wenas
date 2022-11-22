#include "linearkv.h"
#include <stdio.h>

static void test_add_string()
{
	linearkv_t *lkv = lkv_build(10240);
	linearkv_parser_t parser = lkv;
	lineardb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		fprintf(stdout, "key = %s value = %s\n", key_buf, value_buf);
		lkv_add_str(parser, key_buf, value_buf);
	}

	for (int k = 0; k < 100; ++k){
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = lkv_find(parser, key_buf);
		if (v){
			fprintf(stdout, "key %s -> value %s\n", key_buf, __dataof_block(v));
		}
	}

	n = snprintf(key_buf, 1024, "1%d", 2);
    fprintf(stdout, "strlen=%lu n=%u\n", strlen(key_buf), n);
    memcpy(value_buf, key_buf, n);
    value_buf[n] = '\0';
    fprintf(stdout, "memcpy=%s\n", value_buf);

	lkv_destroy(&lkv);
}

static void test_find_after()
{
	linearkv_t *lkv = lkv_build(10240);
	linearkv_parser_t parser = lkv;
	lineardb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		lkv_add_str(parser, key_buf, value_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = lkv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __dataof_block(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = lkv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = lkv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __dataof_block(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = lkv_after(parser, key_buf);
	}

	lkv_destroy(&lkv);
}

static void test_find_number()
{
	linearkv_t *lkv = lkv_build(10240);
	linearkv_parser_t parser = lkv;
	lineardb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	uint32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		fprintf(stdout, "input %d\n", n);
		lkv_add_number(parser, key_buf, __n2b32(n));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = lkv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = lkv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = lkv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = lkv_after(parser, key_buf);
	}

	lkv_destroy(&lkv);
}

static void test_find_float()
{
	linearkv_t *lkv = lkv_build(10240);
	linearkv_parser_t parser = lkv;
	lineardb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	double f64;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		if (n%2){
			f64 = (double)n / 3.3f;
		}else {
			f64 = (double)n / 2.2f;
		}
		fprintf(stdout, "input ====>>>>>%d %lf\n", i, f64);
		// lkv_add_f64(parser, key_buf, f64);
		value = __f2b64(f64);
		lkv_add_number(parser, key_buf, value);
		fprintf(stdout, "input ====###>>>>>%d %lf\n", i, __b2f64(&value));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = lkv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude --------> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = lkv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = lkv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude >>>>>>>>-> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = lkv_after(parser, key_buf);
	}

	lkv_destroy(&lkv);
}


static void print_objcet(linearkv_t *lkv)
{
	fprintf(stdout, "objcet >>>>---------->\n");

	lineardb_t *ldb = lkv_head(lkv);

	do {

		if (__typeis_number(ldb)){

			if (__typeof_block(ldb) == BLOCK_TYPE_INTEGER){

				if (__typeis_n8bit(ldb)){
					fprintf(stdout, "key=%s value=%hhd\n", lkv_current_key(lkv), __b2n8(ldb));
				}else if (__typeis_n16bit(ldb)){
					fprintf(stdout, "key=%s value=%hd\n", lkv_current_key(lkv), __b2n16(ldb));
				}else if (__typeis_n32bit(ldb)){
					fprintf(stdout, "key=%s value=%d\n", lkv_current_key(lkv), __b2n32(ldb));
				}else if (__typeis_n64bit(ldb)){
					fprintf(stdout, "key=%s value=%ld\n", lkv_current_key(lkv), __b2n64(ldb));
				}

			}else if (__typeof_block(ldb) == BLOCK_TYPE_UNSIGNED){

				if (__typeis_n8bit(ldb)){
					fprintf(stdout, "key=%s value=%u\n", lkv_current_key(lkv), __b2u8(ldb));
				}else if (__typeis_n16bit(ldb)){
					fprintf(stdout, "key=%s value=%u\n", lkv_current_key(lkv), __b2u16(ldb));
				}else if (__typeis_n32bit(ldb)){
					fprintf(stdout, "key=%s value=%u\n", lkv_current_key(lkv), __b2u32(ldb));
				}else if (__typeis_n64bit(ldb)){
					fprintf(stdout, "key=%s value=%lu\n", lkv_current_key(lkv), __b2u64(ldb));
				}

			}else if (__typeof_block(ldb) == BLOCK_TYPE_FLOAT){

				if (__typeis_n32bit(ldb)){
					fprintf(stdout, "key=%s value=%f\n", lkv_current_key(lkv), __b2f32(ldb));
				}else {
					fprintf(stdout, "key=%s value=%lf\n", lkv_current_key(lkv), __b2f64(ldb));
				}

			}else if (__typeof_block(ldb) == BLOCK_TYPE_BOOLEAN){

				fprintf(stdout, "key=%s value=%u\n", lkv_current_key(lkv), __b2u8(ldb));
			}

		}else if (__typeis_object(ldb)){

			if (__typeof_block(ldb) == BLOCK_TYPE_STRING){

				fprintf(stdout, "key=%s value %s\n", lkv_current_key(lkv), __dataof_block(ldb));

			}else if (__typeof_block(ldb) == BLOCK_TYPE_BLOCK){

				linearkv_t obj;
				lkv_bind_block(&obj, ldb);
				print_objcet(&obj);
			}
		}

	}while ((ldb = lkv_next(lkv)) != NULL);
}

static void test_add_object()
{
	linearkv_t *lkv = lkv_build(10240);
	linearkv_t *obj = lkv_build(10240);
	linearkv_t *obj1 = lkv_build(10240);

	lkv_add_n8(lkv, "int8", -8);
	lkv_add_n16(lkv, "int16", -16);
	lkv_add_n32(lkv, "int32", -32);
	lkv_add_n64(lkv, "int32", -64);

	lkv_add_bool(obj, "boolean", 1);
	lkv_add_u8(obj, "uint8", 8);
	lkv_add_u16(obj, "uint16", 16);
	lkv_add_u32(obj, "uint32", 32);
	lkv_add_u64(obj, "uint64", 64);
	lkv_add_f32(obj, "float32", 32.32f);
	lkv_add_f64(obj, "float64", 64.64f);
	lkv_add_str(obj, "string", "string >>>>------------------------> object");

	lkv_add_obj(lkv, "object", obj);

	lkv_add_bool(obj1, "boolean", 1);
	lkv_add_u8(obj1, "uint8", 8);
	lkv_add_u16(obj1, "uint16", 16);
	lkv_add_u32(obj1, "uint32", 32);
	lkv_add_u64(obj1, "uint64", 64);
	lkv_add_f32(obj1, "float32", 32.32f);
	lkv_add_f64(obj1, "float64", 64.64f);
	lkv_add_str(obj1, "string", "string >>>>------------------------> object 1");
	lkv_add_obj(obj1, "object", obj);

	lkv_add_obj(lkv, "object", obj1);

	print_objcet(lkv);

	lkv_destroy(&lkv);
	lkv_destroy(&obj);
	lkv_destroy(&obj1);
}

void linearkv_test()
{
	test_add_string();
	test_find_after();
	test_find_float();
	test_find_number();
	test_add_object();
}