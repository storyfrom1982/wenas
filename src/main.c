#include "env/env.h"

extern void test();

int main(int argc, char *argv[])
{
    env_backtrace_setup();
#if defined(OS_WINDOWS)
    //env_logger_start("d:/tmp/log", NULL);
    env_logger_start("./tmp/log", NULL);
#else
    env_logger_start("./tmp/log", NULL);
#endif
	

    __logi("hello world\n");
    __logi("time %llu clock %llu\n", env_time(), env_clock());
    char buf[1024] = {0};
    __uint64 n = env_strftime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    __logi("time %s\n", buf);

    n = env_strftime(buf, 1024, env_clock() / NANO_SECONDS);
    buf[n] = '\0';
    __logi("clock %s\n", buf);

#ifdef __PL64__
    // LOGD("TEST", "__PL64__\n");
    __logi("__PL64__\n");
#endif

#ifdef __LITTLE_ENDIAN__
	// LOGD("TEST", "__LITTLE_ENDIAN__\n");
    __logi("__LITTLE_ENDIAN__\n");
#else
	// LOGD("TEST", "__BIG_ENDIAN__\n");
    __logi("__BIG_ENDIAN__\n");
#endif

#ifdef ENV_HAVA_BACKTRACE
    __logi("ENV_HAVA_BACKTRACE\n");
#endif

    __logi("__uint64 size: %d\n", sizeof(__uint64));
    __logi("__sint32 size: %d\n", sizeof(__sint32));
    __logi("__bool size: %d\n", sizeof(__bool));

    test();

	return 0;
}
