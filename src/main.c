#include "env/env.h"

extern void test();

int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/log", NULL);
	
    __logi("hello world\n");
    __logi("time %llu clock %llu\n", env_time(), env_clock());
    char buf[1024] = {0};
    uint64_t n = env_strftime(buf, 1024, env_time() / NANO_SECONDS);
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

    __logi("__uint64 size: %d\n", sizeof(uint64_t));
    __logi("int32_t size: %d\n", sizeof(int32_t));
    __logi("bool size: %d\n", sizeof(bool));

    test();

	return 0;
}
