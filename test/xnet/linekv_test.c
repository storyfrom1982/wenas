#include "xnet/linekv.h"

#include "env/env.h"

#include <stdio.h>
#include <stdlib.h> //rand

static void test_add_string()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> test_add_string\n");
	linekv_ptr lkv = linekv_create(1024);
	linekv_ptr parser = lkv;
	struct linedb value;
	linedb_ptr v, b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 10; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		// __logd("key = %s value = %s\n", key_buf, value_buf);
		linekv_add_string(parser, key_buf, value_buf);
	}

	for (int k = 0; k < 10; ++k){
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_find(parser, key_buf);
		if (v){
			__logd("fond key %s -> value %s\n", key_buf, __dataof_linedb(v));
		}
	}

	n = snprintf(key_buf, 1024, "1%d", 2);
    __logd("strlen=%lu n=%u\n", strlen(key_buf), n);
    memcpy(value_buf, key_buf, n);
    value_buf[n] = '\0';
    __logd("memcpy=%s\n", value_buf);

	linekv_release(&lkv);
}

static void test_find_after()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> test_find_after\n");
	linekv_ptr lkv = linekv_create(1024);
	linekv_ptr parser = lkv;
	struct linedb value;
	linedb_ptr v, b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 10; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = snprintf(value_buf, 1024, "hello world %d", rand());
		linekv_add_string(parser, key_buf, value_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 8; v != NULL; --k){
		__logd("fond key %s from valude -> %s\n", key_buf, __dataof_linedb(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		__logd("fond key %s from valude -> %s\n", key_buf, __dataof_linedb(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_release(&lkv);
}

static void test_find_number()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> test_find_number\n");
	linekv_ptr lkv = linekv_create(1024);
	linekv_ptr parser = lkv;
	struct linedb value;
	linedb_ptr v, b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 10; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		// __logd("input %d\n", n);
		linekv_add_number(parser, key_buf, __n2b32(n));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 8; v != NULL; --k){
		__logd("fond key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		__logd("fond key %s from valude -> %u\n", key_buf, __b2n32(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_release(&lkv);
}

static void test_find_float()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> test_find_float\n");
	linekv_ptr lkv = linekv_create(1024);
	linekv_ptr parser = lkv;
	struct linedb value;
	linedb_ptr v, b = &value;
	char key_buf[1024] = {0};
	char value_buf[1024] = {0};
	int32_t n;
	double f64;
	for (int i = 0; i < 10; ++i){
		n = snprintf(key_buf, 1024, "hello world %d", i);
		n = rand();
		if (n%2){
			f64 = (double)n / 3.3f;
		}else {
			f64 = (double)n / 2.2f;
		}
		// __logd("input ====>>>>>%d %lf\n", i, f64);
		// lkv_add_f64(parser, key_buf, f64);
		value = __f2b64(f64);
		linekv_add_number(parser, key_buf, value);
		// __logd("input ====###>>>>>%d %lf\n", i, __b2f64(&value));
	}

	n = snprintf(key_buf, 1024, "hello world %d", 99);
	v = linekv_find(parser, key_buf);
	for (int k = 8; v != NULL; --k){
		__logd("fond key %s from valude --------> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k-1);
		v = linekv_after(parser, key_buf);
	}

	n = snprintf(key_buf, 1024, "hello world %d", 0);
	v = linekv_find(parser, key_buf);
	for (int k = 1; v != NULL; ++k){
		__logd("fond key %s from valude >>>>>>>>-> %lf\n", key_buf, __b2f64(v));
		n = snprintf(key_buf, 1024, "hello world %d", k);
		v = linekv_after(parser, key_buf);
	}

	linekv_release(&lkv);
}


static void print_objcet(linekv_ptr kv)
{
	__logd(">>>>----------------------------------------------------------------------------------------------> print_objcet\n");

	lineval_ptr val = linekv_first(kv);

	do {

		if (__typeis_number(val)){

			if (__numberis_integer(val)){
				if (__numberis_8bit(val)){
					__logd("key=%s value=%hhd\n", linekey_to_string(kv->key), __b2n8(val));
				}else if (__numberis_16bit(val)){
					__logd("key=%s value=%hd\n", linekey_to_string(kv->key), __b2n16(val));
				}else if (__numberis_32bit(val)){
					__logd("key=%s value=%d\n", linekey_to_string(kv->key), __b2n32(val));
				}else if (__numberis_64bit(val)){
					__logd("key=%s value=%ld\n", linekey_to_string(kv->key), __b2n64(val));
				}

			}else if (__numberis_unsigned(val)){

				if (__numberis_8bit(val)){
					__logd("key=%s value=%u\n", linekey_to_string(kv->key), __b2u8(val));
				}else if (__numberis_16bit(val)){
					__logd("key=%s value=%u\n", linekey_to_string(kv->key), __b2u16(val));
				}else if (__numberis_32bit(val)){
					__logd("key=%s value=%u\n", linekey_to_string(kv->key), __b2u32(val));
				}else if (__numberis_64bit(val)){
					__logd("key=%s value=%lu\n", linekey_to_string(kv->key), __b2u64(val));
				}

			}else if (__numberis_float(val)){

				if (__numberis_32bit(val)){
					__logd("key=%s value=%f\n", linekey_to_string(kv->key), __b2f32(val));
				}else {
					__logd("key=%s value=%lf\n", linekey_to_string(kv->key), __b2f64(val));
				}

			}else if (__numberis_boolean(val)){

				__logd("key=%s value=%u\n", linekey_to_string(kv->key), __b2u8(val));
			}

		}else if (__typeis_object(val)){

			if (__objectis_string(val)){

				__logd("key=%s value %s\n", linekey_to_string(kv->key), __dataof_linedb(val));

			}else if (__objectis_custom(val)){

				struct linekv obj;
				linekv_load(&obj, __dataof_linedb(val), __sizeof_data(val));
				print_objcet(&obj);

			}else if (__objectis_array(val)){

				__logd("__objectis_array size: %lu\n", __sizeof_linedb(val));
				struct linearray reader;
				linedb_ptr ldb;
				linearray_load(&reader, __dataof_linedb(val), __sizeof_data(val));
				do {
					ldb = linearray_next(&reader);
					if (ldb){
						__logd("%s\n", (char*)__dataof_linedb(ldb));
					}
				}while(ldb);
			}
		}

	}while ((val = linekv_next(kv)) != NULL);
}

static void test_add_object()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> test_add_object\n");
	linekv_ptr lkv = linekv_create(1024);
	linekv_ptr obj = linekv_create(1024);
	linekv_ptr obj1 = linekv_create(1024);

	linearray_ptr writer = linearray_create(1024);

    linedb_ptr ldb;
    char key_buf[1024];
    size_t n;
	for (int i = 0; i < 10; ++i){
		n = snprintf(key_buf, 1024, "hello world %d %d\0", i, rand());
		ldb = linedb_from_string(key_buf);
        // __logd("%s\n", __dataof_linedb(ldb));
        linearray_append(writer, ldb);
        __linedb_free(ldb);
	}

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
	linekv_add_string(obj, "string", "string >>>>------------------------> object");

	linekv_add_array(obj, "array", writer);

	linekv_add_object(lkv, "object", obj);

	linekv_add_bool(obj1, "boolean", 1);
	linekv_add_uint8(obj1, "uint8", 8);
	linekv_add_uint16(obj1, "uint16", 16);
	linekv_add_uint32(obj1, "uint32", 32);
	linekv_add_uint64(obj1, "uint64", 64);
	linekv_add_float32(obj1, "float32", 32.32f);
	linekv_add_float64(obj1, "float64", 64.64f);
	linekv_add_string(obj1, "string", "string >>>>------------------------> object 1");
	linekv_add_object(obj1, "object", obj);

	linekv_add_object(lkv, "object", obj1);
	linekv_add_array(lkv, "array", writer);

	print_objcet(lkv);

	linekv_release(&lkv);
	linekv_release(&obj);
	linekv_release(&obj1);

	linearray_release(&writer);
}

void linearkv_test()
{
	__logd(">>>>----------------------------------------------------------------------------------------------> linearkv_test\n");
	test_add_string();
	test_find_after();
	test_find_float();
	test_find_number();
	test_add_object();
}