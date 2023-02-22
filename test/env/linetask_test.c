#include "env/env.h"
#include "env/linetask.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


static void test_post_func(linekv_ptr  parser)
{
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    __logd("int: %d\n", linekv_find_int32(parser, "int"));
    __logd("float: %.3f\n", linekv_find_float32(parser, "float"));
    __logd("double: %.5lf\n", linekv_find_float64(parser, "double"));
    __logd("string: %s\n", linekv_find_string(parser, "string"));
    linekv_release(&parser);
}

static uint64_t test_timer_func(linekv_ptr  parser)
{
    __logd("================= delay: %lu\n", (char*)linekv_find_ptr(parser, "delay"));
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 3000000000ULL;
}

static uint64_t test_timer_func1(linekv_ptr  parser)
{
    __logd(">>>>------------> delay: %lu\n", (char*)linekv_find_ptr(parser, "delay"));
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 1000000000ULL;
}

void linetask_test()
{
    linetask_ptr tq = linetask_create();

    char buf[256];
    linekv_ptr lkv;
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";


    lkv = linekv_create(1024);
    linekv_add_ptr(lkv, "func", test_timer_func);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "delay", 3000000000ULL);
    linetask_timer(tq, lkv);

    lkv = linekv_create(1024);
    linekv_add_ptr(lkv, "func", test_timer_func1);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "delay", 1000000000ULL);
    linetask_timer(tq, lkv);

    for (int i = 0; i < 100; ++i){
        lkv = linekv_create(1024);
        linekv_add_ptr(lkv, "func", test_post_func);
        linekv_add_ptr(lkv, "ctx", (void*)test_string);
        linekv_add_int32(lkv, "int", test_number);
        linekv_add_float32(lkv, "float", test_float);
        linekv_add_float64(lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        linekv_add_string(lkv, "string", buf);
        if (i < 50){
            linetask_post(tq, lkv);
        }else {
            linetask_immediately(tq, lkv);
        }
    }

    sleep(10);

    linetask_release(&tq);
}