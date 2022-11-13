#include "linearkv.h"



static void test_append_string()
{
	Linearkv *lkv = linearkv_create(1024 * 1024);
	Lineardb *key, *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		fprintf(stdout, "key = %s value = %s\n", key_buf, value_buf);
		linearkv_append_string(lkv, key_buf, value_buf);
	}

	for (int k = 0; k < 100; ++k){
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linearkv_find(lkv, key_buf);
		if (v){
			fprintf(stdout, "key %s -> value %s\n", key_buf, __byteof_block(v));
		}
	}

	n = snprintf(key_buf, 1024, "1%d", 2);
    fprintf(stdout, "strlen=%lu n=%u\n", strlen(key_buf), n);
    memcpy(value_buf, key_buf, n);
    value_buf[n] = '\0';
    fprintf(stdout, "memcpy=%s\n", value_buf);

	linearkv_release(&lkv);
}

static void test_find_from()
{
	Linearkv *lkv = linearkv_create(1024 * 1024);
	Lineardb *key, *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		linearkv_append_string(lkv, key_buf, value_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linearkv_find(lkv, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __byteof_block(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linearkv_find(lkv, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __byteof_block(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	linearkv_release(&lkv);
}

static void test_find_number()
{
	Linearkv *lkv = linearkv_create(1024 * 1024);
	Lineardb *key, *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	uint32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		fprintf(stdout, "input %d\n", n);
		linearkv_append_number(lkv, key_buf, n2b32(n));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linearkv_find(lkv, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linearkv_find(lkv, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	linearkv_release(&lkv);
}

static void test_find_float()
{
	Linearkv *lkv = linearkv_create(1024 * 1024);
	Lineardb *key, *v, value, *b = &value;
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
		fprintf(stdout, "input %lf\n", f64);
		linearkv_append_number(lkv, key_buf, f2b64(f64));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linearkv_find(lkv, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %lf\n", key_buf, b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linearkv_find(lkv, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %lf\n", key_buf, b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linearkv_find_from(lkv, v, key_buf);
	}

	linearkv_release(&lkv);
}

void linearkv_test()
{
	// test_append_string();
	// test_find_from();
	test_find_number();
	// test_find_float();
}