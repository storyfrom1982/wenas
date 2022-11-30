#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>

#include "env_malloc.h"


static void log_cb(const char *fmt, ...){
    char text[4096] = {0};
    va_list args;
    va_start (args, fmt);
    size_t n = (size_t) vsnprintf(text, 4096, fmt, args);
    va_end (args);
    printf("%s\n", text);
}


static char *str, *strn;
static void test_strdup()
{
    str = strdup("12345");
    strn = strndup("12345", strlen("12345"));
}

void malloc_test()
{
#ifdef ENV_HAVE_EXECINFO
    printf("%s\n", "ENV_HAVE_EXECINFO");
#endif

#ifdef ENV_HAVE_STDATOMIC
    printf("%s\n", "ENV_HAVE_STDATOMIC");
#endif

    // test4();

    void *p = malloc(1024);

    void *p1 = malloc(1024);

    void *p2 = malloc(1024);
    
    test_strdup();

    printf(">>>>--------------->\n");
    // env_malloc_debug(log_cb);
    

    free(p);
    free(p1);
    free(p2);

    printf(">>>>--------------->\n");
    // env_malloc_debug(log_cb);
    

    free(str);
    free(strn);

    printf(">>>>--------------->\n");
    env_malloc_debug(log_cb);
    
}