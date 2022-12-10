#include "env/env.h"
// #include <stdio.h>


// extern void linearkv_test();
// extern void lineardb_test();
// extern void lineardb_pipe_test();
// extern void thread_test();
// extern void task_queue_test();
// extern void heap_test();
// extern void malloc_test();
// extern void crash_backtrace_test();
// extern void file_system_test();
// extern void logger_test();
// extern int test_error(const char *test);

static void log_print(int level, const char *tag, const char *debug, const char *log)
{
	fprintf(stdout, "%s", debug);
}

int main(int argc, char *argv[])
{
    // env_backtrace_setup();
	// env_logger_start("/tmp/log", log_print);

	// LOGD("TEST", "hello world\n");
    fprintf(stdout, "hello world\n");

#ifdef __PL64__
    // LOGD("TEST", "__PL64__\n");
    fprintf(stdout, "__PL64__\n");
#endif

#ifdef __LITTLE_ENDIAN__
	// LOGD("TEST", "__LITTLE_ENDIAN__\n");
    fprintf(stdout, "__LITTLE_ENDIAN__\n");
#else
	// LOGD("TEST", "__BIG_ENDIAN__\n");
    fprintf(stdout, "__BIG_ENDIAN__\n");
#endif



    // lineardb_test();
    // linearkv_test();
    // lineardb_pipe_test();
    // thread_test();
    // task_queue_test();
    // heap_test();
    // file_system_test();


    // logger_test();
    // malloc_test();


	return 0;
}
