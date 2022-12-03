#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "env/malloc.h"
#include "env/logger.h"


static void log_cb(const char *fmt){
    LOGD("MALLOC", "%s\n", fmt);
}


static char *str, *strn;
static void test_strdup()
{
    str = strdup("12345");
    strn = strndup("12345", strlen("12345"));
}

void malloc_test()
{
#ifdef ENV_MALLOC_BACKTRACE
    LOGD("MALLOC", "%s\n", "ENV_MALLOC_BACKTRACE");
#endif

#ifdef ENV_HAVE_STDATOMIC
    LOGD("MALLOC", "%s\n", "ENV_HAVE_STDATOMIC");
#endif

    // test4();

    void *p = malloc(1024);

    void *p1 = malloc(1024);

    void *p2 = malloc(1024);
    
    test_strdup();

    LOGD("MALLOC", ">>>>--------------->\n");
    env_malloc_debug(log_cb);
    

    free(p);
    free(p1);
    free(p2);

    LOGD("MALLOC", ">>>>--------------->\n");
    env_malloc_debug(log_cb);
    

    free(str);
    free(strn);

    LOGD("MALLOC", ">>>>--------------->\n");
    env_malloc_debug(log_cb);
    
}