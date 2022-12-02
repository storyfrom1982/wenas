#include "sys/struct/linearkv.h"
#include <stdio.h>

static void test_add_string()
{
	linekv_t *lkv = linekv_build(10240);
	linekv_parser_t parser = lkv;
	linedb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		fprintf(stdout, "key = %s value = %s\n", key_buf, value_buf);
		linekv_add_str(parser, key_buf, value_buf);
	}

	for (int k = 0; k < 100; ++k){
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_find(parser, key_buf);
		if (v){
			fprintf(stdout, "key %s -> value %s\n", key_buf, __dataof_linedb(v));
		}
	}

	n = snprintf(key_buf, 1024, "1%d", 2);
    fprintf(stdout, "strlen=%lu n=%u\n", strlen(key_buf), n);
    memcpy(value_buf, key_buf, n);
    value_buf[n] = '\0';
    fprintf(stdout, "memcpy=%s\n", value_buf);

	linekv_destroy(&lkv);
}

static void test_find_after()
{
	linekv_t *lkv = linekv_build(10240);
	linekv_parser_t parser = lkv;
	linedb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		linekv_add_str(parser, key_buf, value_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __dataof_linedb(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %s\n", key_buf, __dataof_linedb(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_destroy(&lkv);
}

static void test_find_number()
{
	linekv_t *lkv = linekv_build(10240);
	linekv_parser_t parser = lkv;
	linedb_t *v, value, *b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	uint32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		fprintf(stdout, "input %d\n", n);
		linekv_add_number(parser, key_buf, __n2b32(n));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_destroy(&lkv);
}

static void test_find_float()
{
	linekv_t *lkv = linekv_build(10240);
	linekv_parser_t parser = lkv;
	linedb_t *v, value, *b = &value;
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
		linekv_add_number(parser, key_buf, value);
		fprintf(stdout, "input ====###>>>>>%d %lf\n", i, __b2f64(&value));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 98; v != NULL; --k){
		fprintf(stdout, "key %s from valude --------> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		fprintf(stdout, "key %s from valude >>>>>>>>-> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_destroy(&lkv);
}


static void print_objcet(linekv_t *kv)
{
	fprintf(stdout, "objcet >>>>---------->\n");

	lineval_t *val = linekv_head(kv);

	do {

		if (__typeis_number(val)){

			if (__numberis_integer(val)){

				if (__numberis_8bit(val)){
					fprintf(stdout, "key=%s value=%hhd\n", linekv_current_key(kv), __b2n8(val));
				}else if (__numberis_16bit(val)){
					fprintf(stdout, "key=%s value=%hd\n", linekv_current_key(kv), __b2n16(val));
				}else if (__numberis_32bit(val)){
					fprintf(stdout, "key=%s value=%d\n", linekv_current_key(kv), __b2n32(val));
				}else if (__numberis_64bit(val)){
					fprintf(stdout, "key=%s value=%ld\n", linekv_current_key(kv), __b2n64(val));
				}

			}else if (__numberis_unsigned(val)){

				if (__numberis_8bit(val)){
					fprintf(stdout, "key=%s value=%u\n", linekv_current_key(kv), __b2u8(val));
				}else if (__numberis_16bit(val)){
					fprintf(stdout, "key=%s value=%u\n", linekv_current_key(kv), __b2u16(val));
				}else if (__numberis_32bit(val)){
					fprintf(stdout, "key=%s value=%u\n", linekv_current_key(kv), __b2u32(val));
				}else if (__numberis_64bit(val)){
					fprintf(stdout, "key=%s value=%lu\n", linekv_current_key(kv), __b2u64(val));
				}

			}else if (__numberis_float(val)){

				if (__numberis_32bit(val)){
					fprintf(stdout, "key=%s value=%f\n", linekv_current_key(kv), __b2f32(val));
				}else {
					fprintf(stdout, "key=%s value=%lf\n", linekv_current_key(kv), __b2f64(val));
				}

			}else if (__numberis_boolean(val)){

				fprintf(stdout, "key=%s value=%u\n", linekv_current_key(kv), __b2u8(val));
			}

		}else if (__typeis_object(val)){

			if (__objectis_string(val)){

				fprintf(stdout, "key=%s value %s\n", linekv_current_key(kv), __dataof_linedb(val));

			}else if (__objectis_linekv(val)){

				linekv_t obj;
				linekv_bind_object(&obj, val);
				print_objcet(&obj);
			}
		}

	}while ((val = linekv_next(kv)) != NULL);
}

static void test_add_object()
{
	linekv_t *lkv = linekv_build(10240);
	linekv_t *obj = linekv_build(10240);
	linekv_t *obj1 = linekv_build(10240);

	linekv_add_int8(lkv, "int8", -8);
	linekv_add_int16(lkv, "int16", -16);
	linekv_add_int32(lkv, "int32", -32);
	linekv_add_int64(lkv, "int32", -64);

	linekv_add_bool(obj, "boolean", 1);
	linekv_add_uint8(obj, "uint8", 8);
	linekv_add_uint16(obj, "uint16", 16);
	linekv_add_uint32(obj, "uint32", 32);
	linekv_add_uint64(obj, "uint64", 64);
	linekv_add_float32(obj, "float32", 32.32f);
	linekv_add_float64(obj, "float64", 64.64f);
	linekv_add_str(obj, "string", "string >>>>------------------------> object");

	linekv_add_obj(lkv, "object", obj);

	linekv_add_bool(obj1, "boolean", 1);
	linekv_add_uint8(obj1, "uint8", 8);
	linekv_add_uint16(obj1, "uint16", 16);
	linekv_add_uint32(obj1, "uint32", 32);
	linekv_add_uint64(obj1, "uint64", 64);
	linekv_add_float32(obj1, "float32", 32.32f);
	linekv_add_float64(obj1, "float64", 64.64f);
	linekv_add_str(obj1, "string", "string >>>>------------------------> object 1");
	linekv_add_obj(obj1, "object", obj);

	linekv_add_obj(lkv, "object", obj1);

	print_objcet(lkv);

	linekv_destroy(&lkv);
	linekv_destroy(&obj);
	linekv_destroy(&obj1);
}

void linearkv_test()
{
	test_add_string();
	test_find_after();
	test_find_float();
	test_find_number();
	test_add_object();
}