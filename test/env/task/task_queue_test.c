#include "env/task_queue.h"
#include <unistd.h>
#include <stdio.h>


static void test_task_func(linekv_parser_t parser)
{
    LOGD("TASKQUEUE", "func: 0x%x\n", linekv_find_int64(parser, "func"));
    LOGD("TASKQUEUE", "ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    LOGD("TASKQUEUE", "int: %d\n", linekv_find_int32(parser, "int"));
    LOGD("TASKQUEUE", "float: %.3f\n", linekv_find_float32(parser, "float"));
    LOGD("TASKQUEUE", "double: %.5lf\n", linekv_find_float64(parser, "double"));
    LOGD("TASKQUEUE", "string: %s\n", linekv_find_str(parser, "string"));
}

static uint64_t test_timedtask_func(linekv_parser_t parser)
{
    LOGD("TASKQUEUE", "================= time: %lu\n", (char*)linekv_find_ptr(parser, "time"));
    LOGD("TASKQUEUE", "func: 0x%x\n", linekv_find_int64(parser, "func"));
    LOGD("TASKQUEUE", "ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 3000000000ULL;
}

static uint64_t test_timedtask_func1(linekv_parser_t parser)
{
    LOGD("TASKQUEUE", ">>>>------------> time: %lu\n", (char*)linekv_find_ptr(parser, "time"));
    LOGD("TASKQUEUE", "func: 0x%x\n", linekv_find_int64(parser, "func"));
    LOGD("TASKQUEUE", "ctx: %s\n", (char*)linekv_find_ptr(parser, "ctx"));
    return 1000000000ULL;
}

static linekv_t* test_join_task_func(linekv_parser_t parser)
{
    linekv_t *result = linekv_build(10240);
    linekv_add_float64(result, "result", 12345.12345f);
    for (int i = 0; i < 3; ++i){
        LOGD("TASKQUEUE", "join task result=====================%d\n", i);
        LOGD("TASKQUEUE", "func: 0x%x\n", linekv_find_int64(parser, "func"));
    }
    return result;
}

void task_queue_test()
{
    LOGD("TASKQUEUE", "task_queue_test enter\n");

    env_taskqueue_t *tq = env_taskqueue_build();

    char buf[256];
    linekv_t *lkv = linekv_build(10240);
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        // LOGD("TASKQUEUE", "task_queue_test func %x\n", (uint64_t)test_task_func);
        linekv_add_ptr(lkv, "func", test_task_func);
        linekv_add_ptr(lkv, "ctx", (void*)test_string);
        linekv_add_int32(lkv, "int", test_number);
        linekv_add_float32(lkv, "float", test_float);
        linekv_add_float64(lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // LOGD("TASKQUEUE", "task_queue_test push string %s\n", string_buf);
        linekv_add_str(lkv, "string", buf);
        env_taskqueue_post_task(tq, lkv);
        linekv_clear(lkv);
    }

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_join_task_func);
    linekv_t *result = env_taskqueue_join_task(tq, lkv);
    LOGD("TEST JOIN TASK", "result >>>>------------>  >>>>------------>  >>>>------------> %lf\n", linekv_find_float64(result, "result"));
    linekv_destroy(&result);

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_timedtask_func);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "time", 3000000000ULL);
    env_taskqueue_insert_timedtask(tq, lkv);

    linekv_clear(lkv);
    linekv_add_ptr(lkv, "func", test_timedtask_func1);
    linekv_add_ptr(lkv, "ctx", (void*)test_string);
    linekv_add_uint64(lkv, "time", 1000000000ULL);
    env_taskqueue_insert_timedtask(tq, lkv);

    sleep(10*3);

    env_taskqueue_exit(tq);

    env_taskqueue_destroy(&tq);

    linekv_destroy(&lkv);

    LOGD("TASKQUEUE", "task_queue_test exit\n");
}