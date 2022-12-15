#include "env/env.h"

extern void test();

int main(int argc, char *argv[])
{
    // env_backtrace_setup();
	env_logger_start("/tmp/log", NULL);

    __logi("hello world\n");
    __logi("time %llu clock %llu\n", env_time(), env_clock());
    __sym buf[1024] = {0};
    __uint64 n = env_strtime(buf, 1024, env_time() / NANO_SECONDS);
    buf[n] = '\0';
    __logi("time %s\n", buf);

    n = env_strtime(buf, 1024, env_clock() / NANO_SECONDS);
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

    test();

    env_logger_stop();

	return 0;
}
