#include "linearkv.h"
#include <stdio.h>

static void test_append_string()
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

static void test_find_from()
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

void linearkv_test()
{
	test_append_string();
	test_find_from();
	test_find_float();
	test_find_number();
}