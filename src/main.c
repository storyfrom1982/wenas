#include "env/env.h"
#include "stdio.h"

extern void test();

static void log_print(int level, const char *tag, const char *debug, const char *log)
{
	printf("%s", debug);
}

int main(int argc, char *argv[])
{
    // env_init();
    // env_backtrace_setup();
	// env_logger_start("/tmp/log", log_print);

	// LOGD("TEST", "hello world\n");
    printf("hello world\n");
    printf("time %llu clock %llu\n", env_time(), env_clock());
    __sym buf[1024] = {0};
    __uint64 n = env_strtime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    printf("time %s\n", buf);

    n = env_strtime(buf, 1024, env_clock() / NANO_SECONDS);
    buf[n] = '\0';
    printf("clock %s\n", buf);

#ifdef __PL64__
    // LOGD("TEST", "__PL64__\n");
    printf("__PL64__\n");
#endif

#ifdef __LITTLE_ENDIAN__
	// LOGD("TEST", "__LITTLE_ENDIAN__\n");
    printf("__LITTLE_ENDIAN__\n");
#else
	// LOGD("TEST", "__BIG_ENDIAN__\n");
    printf("__BIG_ENDIAN__\n");
#endif

#ifdef ENV_HAVA_BACKTRACE
    printf("ENV_HAVA_BACKTRACE\n");
#endif

#ifdef ENV_HAVA_STDATOMIC
    printf("ENV_HAVA_STDATOMIC\n");
#endif

    test();

	return 0;
}
