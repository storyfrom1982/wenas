#include "env/logger.h"

static void log_print(int level, const char *tag, const char *debug, const char *log)
{
    printf("%s", debug);
}

void logger_test()
{
    env_logger_start("/tmp/log", log_print);
    env_logger_printf(5, "TEST", __FILE__, __LINE__, __FUNCTION__, ">>>>---------------->");

    for (int i = 0; i < 10; ++i){
        LOGD("info", ">>>>----------------> %d\n", i);
        LOGT("Debug", ">>>>----------------> %d", i);
        LOGI("TEST", ">>>>----------------> %d", i);
        LOGW("ERROR", ">>>>----------------> %d", i);
        LOGE("Test", ">>>>----------------> %d", i);
        LOGF("test", ">>>>----------------> %d", i);
    }

    env_logger_stop();
}