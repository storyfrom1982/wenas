#include "task_queue.h"


static void test_task_func(lkv_parser_t parser)
{
    // Lineardb *ldb = lkv_find(parser, "func");
    // fprintf(stdout, "func: 0x%x\n", __b2n64(ldb));
    // ldb = lkv_find(parser, "this");
    // fprintf(stdout, "this: %s\n", (char*)__b2n64(ldb));
    // ldb = lkv_after(parser, "int");
    // fprintf(stdout, "int: %d\n", __b2n32(ldb));
    // ldb = lkv_after(parser, "float");
    // fprintf(stdout, "foat: %.3f\n", b2f32(ldb));
    // ldb = lkv_after(parser, "double");
    // fprintf(stdout, "double: %.5lf\n", b2f64(ldb));
    // ldb = lkv_after(parser, "string");
    // fprintf(stdout, "string: %s\n", __dataof_block(ldb));
    fprintf(stdout, "func: 0x%x\n", lkv_find_n64(parser, "func"));
    fprintf(stdout, "this: %s\n", (char*)lkv_find_ptr(parser, "this"));
    fprintf(stdout, "int: %d\n", lkv_find_n32(parser, "int"));
    fprintf(stdout, "float: %.3f\n", lkv_find_f32(parser, "float"));
    fprintf(stdout, "double: %.5lf\n", lkv_find_f64(parser, "double"));
    fprintf(stdout, "string: %s\n", lkv_find_str(parser, "string"));
}

void task_queue_test()
{
    fprintf(stdout, "task_queue_test enter\n");

    EnvTaskQueue *tq = env_taskQueue_create();

    char buf[256];
    lkv_builder_t lkv;
    int test_number = 10240;
    float test_float = 123.456f;
    double test_double = 12345.12345f;
    const char *test_string = "test string";

    for (int i = 0; i < 100; ++i){
        // linearkv_bind_buffer(&lkv, buf, 10240);
        lkv_clear(&lkv);
        // fprintf(stdout, "task_queue_test func %x\n", (uint64_t)test_task_func);
        lkv_add_ptr(&lkv, "func", test_task_func);
        lkv_add_ptr(&lkv, "this", (void*)test_string);
        lkv_add_n32(&lkv, "int", test_number);
        lkv_add_f32(&lkv, "float", test_float);
        lkv_add_f64(&lkv, "double", test_double);
        snprintf(buf, 256, "Hello World %ld %d", rand() * rand(), i);
        // fprintf(stdout, "task_queue_test push string %s\n", string_buf);
        lkv_add_str(&lkv, "string", buf);
        env_taskQueue_push(tq, &lkv);
    }

    env_taskQueue_exit(tq);

    env_taskQueue_destroy(&tq);

    fprintf(stdout, "task_queue_test exit\n");
}