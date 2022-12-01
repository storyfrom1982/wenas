#include <stdio.h>

extern void linearkv_test();
extern void lineardb_test();
extern void lineardb_pipe_test();
extern void thread_test();
extern void task_queue_test();
extern void heap_test();
extern void malloc_test();
extern void crash_backtrace_test();
extern void file_system_test();
extern void logger_test();

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");
#ifdef __LITTLE_ENDIAN__
	fprintf(stdout, "__LITTLE_ENDIAN__\n");
#else
	fprintf(stdout, "__BIG_ENDIAN__\n");
#endif

	crash_backtrace_test();
	
	// lineardb_test();
	// linearkv_test();
	// lineardb_pipe_test();
	// thread_test();
	// task_queue_test();
	// heap_test();
	// file_system_test();

	// malloc_test();
	logger_test();

	return 0;
}
