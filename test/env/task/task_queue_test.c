#include "task_queue.h"
#include <unistd.h>


static void test_task_func(linearkv_parser_t parser)
{
    fprintf(stdout, "func: 0x%x\n", lkv_find_n64(parser, "func"));
    fprintf(stdout, "this: %s\n", (char*)lkv_find_ptr(parser, "this"));
    fprintf(stdout, "int: %d\n", lkv_find_n32(parser, "int"));
    fprintf(stdout, "float: %.3f\n", lkv_find_f32(parser, "float"));
    fprintf(stdout, "double: %.5lf\n", lkv_find_f64(parser, "double"));
    fprintf(stdout, "string: %s\n", lkv_find_str(parser, "string"));
}

static void test_timedtask_func(linearkv_parser_t parser)
{
    fprintf(stdout, "================= time: %lu\n", (char*)lkv_find_ptr(parser, "time"));
    fprintf(stdout, "func: 0x%x\n", lkv_find_n64(parser, "func"));
    fprintf(stdout, "this: %s\n", (char*)lkv_find_ptr(parser, "this"));
    fprintf(stdout, "loop: %lu\n", (char*)lkv_find_ptr(parser, "loop"));
}

static void test_timedtask_func1(linearkv_parser_t parser)
{
    fprintf(stdout, "time: %lu\n", (char*)lkv_find_ptr(parser, "time"));
    fprintf(stdout, "func: 0x%x\n", lkv_find_n64(parser, "func"));
    fprintf(stdout, "this: %s\n", (char*)lkv_find_ptr(parser, "this"));
    fprintf(stdout, "loop: %lu\n", (char*)lkv_find_ptr(parser, "loop"));
}

void task_queue_test()
{
    fprintf(stdout, "task_queue_test enter\n");

    env_taskqueue_t *tq = env_taskqueue_build();

    char buf[256];
    linearkv_t *lkv = lkv_build(10240);
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        // fprintf(stdout, "task_queue_test func %x\n", (uint64_t)test_task_func);
        lkv_add_ptr(lkv, "func", test_task_func);
        lkv_add_ptr(lkv, "this", (void*)test_string);
        lkv_add_n32(lkv, "int", test_number);
        lkv_add_f32(lkv, "float", test_float);
        lkv_add_f64(lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // fprintf(stdout, "task_queue_test push string %s\n", string_buf);
        lkv_add_str(lkv, "string", buf);
        env_taskqueue_push_task(tq, lkv);
        lkv_clear(lkv);
    }

    lkv_clear(lkv);
    lkv_add_ptr(lkv, "func", test_timedtask_func);
    lkv_add_ptr(lkv, "this", (void*)test_string);
    lkv_add_n64(lkv, "time", 3000000000ULL);
    lkv_add_n64(lkv, "loop", 1000000000ULL);
    env_taskqueue_push_timedtask(tq, lkv);

    lkv_clear(lkv);
    lkv_add_ptr(lkv, "func", test_timedtask_func1);
    lkv_add_ptr(lkv, "this", (void*)test_string);
    lkv_add_n64(lkv, "time", 1000000000ULL);
    lkv_add_n64(lkv, "loop", 1000000000ULL);
    env_taskqueue_push_timedtask(tq, lkv);    

    sleep(10*3);

    env_taskqueue_exit(tq);

    env_taskqueue_destroy(&tq);

    lkv_destroy(&lkv);

    fprintf(stdout, "task_queue_test exit\n");
}