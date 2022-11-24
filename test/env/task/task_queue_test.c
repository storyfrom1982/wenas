#include "task_queue.h"
#include <unistd.h>
#include <stdio.h>


static void test_task_func(linekv_parser_t parser)
{
    fprintf(stdout, "func: 0x%x\n", linekv_find_n64(parser, "func"));
    fprintf(stdout, "ctx: %s\n", (char*)linekv_find_address(parser, "ctx"));
    fprintf(stdout, "int: %d\n", linekv_find_n32(parser, "int"));
    fprintf(stdout, "float: %.3f\n", linekv_find_f32(parser, "float"));
    fprintf(stdout, "double: %.5lf\n", linekv_find_f64(parser, "double"));
    fprintf(stdout, "string: %s\n", linekv_find_string(parser, "string"));
}

static uint64_t test_timedtask_func(linekv_parser_t parser)
{
    fprintf(stdout, "================= time: %lu\n", (char*)linekv_find_address(parser, "time"));
    fprintf(stdout, "func: 0x%x\n", linekv_find_n64(parser, "func"));
    fprintf(stdout, "ctx: %s\n", (char*)linekv_find_address(parser, "ctx"));
    return 3000000000ULL;
}

static uint64_t test_timedtask_func1(linekv_parser_t parser)
{
    fprintf(stdout, "time: %lu\n", (char*)linekv_find_address(parser, "time"));
    fprintf(stdout, "func: 0x%x\n", linekv_find_n64(parser, "func"));
    fprintf(stdout, "ctx: %s\n", (char*)linekv_find_address(parser, "ctx"));
    return 1000000000ULL;
}

void task_queue_test()
{
    fprintf(stdout, "task_queue_test enter\n");

    env_taskqueue_t *tq = env_taskqueue_build();

    char buf[256];
    linekv_t *lkv = linekv_build(10240);
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        // fprintf(stdout, "task_queue_test func %x\n", (uint64_t)test_task_func);
        linekv_add_address(lkv, "func", test_task_func);
        linekv_add_address(lkv, "ctx", (void*)test_string);
        linekv_add_n32(lkv, "int", test_number);
        linekv_add_f32(lkv, "float", test_float);
        linekv_add_f64(lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // fprintf(stdout, "task_queue_test push string %s\n", string_buf);
        linekv_add_string(lkv, "string", buf);
        env_taskqueue_push_task(tq, lkv);
        linekv_clear(lkv);
    }

    linekv_clear(lkv);
    linekv_add_address(lkv, "func", test_timedtask_func);
    linekv_add_address(lkv, "ctx", (void*)test_string);
    linekv_add_n64(lkv, "time", 3000000000ULL);
    env_taskqueue_push_timedtask(tq, lkv);

    linekv_clear(lkv);
    linekv_add_address(lkv, "func", test_timedtask_func1);
    linekv_add_address(lkv, "ctx", (void*)test_string);
    linekv_add_n64(lkv, "time", 1000000000ULL);
    env_taskqueue_push_timedtask(tq, lkv);    

    sleep(10*3);

    env_taskqueue_exit(tq);

    env_taskqueue_destroy(&tq);

    linekv_destroy(&lkv);

    fprintf(stdout, "task_queue_test exit\n");
}