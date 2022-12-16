#include <env/env.h>


void malloc_debug_cb(const __sym *debug)
{
    __logw("%s\n", debug);
}

extern void storage_test();
extern void thread_test();
extern void logger_test();


void test()
{
    storage_test();
    thread_test();
    logger_test();

    env_logger_stop();
    env_malloc_debug(malloc_debug_cb);
}