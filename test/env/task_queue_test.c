#include "env/env.h"
#include "sys/struct/linekv.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>


static void test_task_func(linekv_t* parser)
{
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    __logd("int: %d\n", linekv_find_int32(parser, "int"));
    __logd("float: %.3f\n", linekv_find_float32(parser, "float"));
    __logd("double: %.5lf\n", linekv_find_float64(parser, "double"));
    __logd("string: %s\n", linekv_find_str(parser, "string"));
}

static uint64_t test_timedtask_func(linekv_t* parser)
{
    __logd("================= time: %lu\n", (char*)linekv_find_ptr(parser, "time"));
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 3000000000ULL;
}

static uint64_t test_timedtask_func1(linekv_t* parser)
{
    __logd(">>>>------------> time: %lu\n", (char*)linekv_find_ptr(parser, "time"));
    __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    __logd("ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 1000000000ULL;
}

static linekv_t* test_join_task_func(linekv_t* parser)
{
    linekv_t *result = linekv_create(10240);
    linekv_add_float64(result, "result", 12345.12345f);
    for (int i = 0; i < 3; ++i){
        __logd("join task result=====================%d\n", i);
        __logd("func: 0x%x\n", linekv_find_int64(parser, "func"));
    }
    return result;
}

void task_queue_test()
{
    __logd("task_queue_test enter\n");

    env_taskqueue_t *tq = env_taskqueue_create();

    char buf[256];
    linekv_t *lkv = linekv_create(10240);
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    __logd("task_queue_test 1\n");

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        // __logd("task_queue_test func %x\n", (uint64_t)test_task_func);
        linekv_add_ptr(lkv, "func", test_task_func);
        linekv_add_ptr(lkv, "ctx", (void*)test_string);
        linekv_add_int32(lkv, "int", test_number);
        linekv_add_float32(lkv, "float", test_float);
        linekv_add_float64(lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // __logd("task_queue_test push string %s\n", string_buf);
        linekv_add_str(lkv, "string", buf);
        env_taskqueue_post_task(tq, lkv);
        linekv_clear(lkv);
    }

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_timedtask_func);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "time", 3000000000ULL);
    env_taskqueue_insert_timed_task(tq, lkv);

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_timedtask_func1);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "time", 1000000000ULL);
    env_taskqueue_insert_timed_task(tq, lkv);

    sleep(10*3);

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_join_task_func);
    linekv_t *result = env_taskqueue_run_sync_task(tq, lkv);
    __logd("result >>>>------------>  >>>>------------>  >>>>------------> %lf\n", linekv_find_float64(result, "result"));
    linekv_destroy(&result);

    env_taskqueue_exit(tq);

    env_taskqueue_destroy(&tq);

    linekv_destroy(&lkv);

    __logd("task_queue_test exit\n");
}