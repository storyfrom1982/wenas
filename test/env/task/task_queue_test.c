#include "task_queue.h"


static void test_task_func(lkv_parser_t parser)
{
    Lineardb *ldb = lkv_find(parser, "func");
    fprintf(stdout, "func: 0x%x\n", __b2n64(ldb));
    ldb = lkv_find(parser, "this");
    fprintf(stdout, "this: %s\n", (char*)__b2n64(ldb));
    ldb = lkv_find_after(parser, ldb, "int");
    fprintf(stdout, "int: %d\n", __b2n32(ldb));
    ldb = lkv_find_after(parser, ldb, "float");
    fprintf(stdout, "foat: %.3f\n", b2f32(ldb));
    ldb = lkv_find_after(parser, ldb, "double");
    fprintf(stdout, "double: %.5lf\n", b2f64(ldb));
    ldb = lkv_find_after(parser, ldb, "string");
    fprintf(stdout, "string: %s\n", __dataof_block(ldb));
}

void task_queue_test()
{
    fprintf(stdout, "task_queue_test enter\n");

    EnvTaskQueue *tq = env_taskQueue_create();

    // uint8_t buf[10240];
    char string_buf[256];
    lkv_builder_t lkv;
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        lkv_builder_clear(&lkv);
        // fprintf(stdout, "task_queue_test func %x\n", (uint64_t)test_task_func);
        lkv_add_number(&lkv, "func", n2b64((uint64_t)(test_task_func)));
        lkv_add_number(&lkv, "this", n2b64((uint64_t)(test_string)));
        lkv_add_number(&lkv, "int", __n2b32(test_number));
        lkv_add_number(&lkv, "float", f2b32(test_float));
        lkv_add_number(&lkv, "double", f2b64(test_double));
        snprintf(string_buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // fprintf(stdout, "task_queue_test push string %s\n", string_buf);
        lkv_add_string(&lkv, "string", string_buf);
        env_taskQueue_push(tq, &lkv);
    }

    env_taskQueue_exit(tq);

    env_taskQueue_destroy(&tq);

    fprintf(stdout, "task_queue_test exit\n");
}