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

void linearkv_test()
{
	// test_append_string();
	test_find_from();
}