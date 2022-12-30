#include <env/env.h>


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}

extern void storage_test();
extern void thread_test();
extern void logger_test();
extern void socket_test();
extern void heap_test();
extern void lineardb_test();


void test()
{
    // storage_test();
    // thread_test();
    // logger_test();
    // socket_test();

    // heap_test();
    lineardb_test();

    env_logger_stop();
    
#if !defined(OS_WINDOWS)
    env_malloc_debug(malloc_debug_cb);
#endif
}