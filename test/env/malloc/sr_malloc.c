#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdatomic.h>

#define __USE_GNU
#define _GNU_SOURCE
#include <dlfcn.h>

#define BT_BUF_SIZE 100


#define SR_MALLOC_BACKTRACE 1
#include "sr_malloc.h"


// void test1(){
//     int nptrs;
//     void *buffer[BT_BUF_SIZE];
//     char **strings;

//     nptrs = backtrace(buffer, BT_BUF_SIZE);
//     printf("backtrace() returned %d addresses\n", nptrs);

//     for (int j = 0; j < nptrs; j++){
//         const void* addr = buffer[j];
//         Dl_info info= {0};
//         if (dladdr(addr, &info) && info.dli_sname) {
            
//         }
//         printf("===>>>> %s\n", info.dli_sname);
//         // printf("===>>>> %p\n", addr);
//     }

//         /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
//         would produce similar output to the following: */

//     strings = backtrace_symbols(buffer, nptrs);
//     if (strings == NULL) {
//         perror("backtrace_symbols");
//         exit(EXIT_FAILURE);
//     }

//     for (int j = 0; j < nptrs; j++)
//         printf("%s\n", strings[j]);
//     free(strings);
// }

// void test2()
// {
//     test1();
// }

// void test3()
// {
//     test2();
// }

// void test4()
// {
//     test3();
// }

static void log_cb(const char *fmt, ...){
    char text[4096] = {0};
    va_list args;
    va_start (args, fmt);
    size_t n = (size_t) vsnprintf(text, 4096, fmt, args);
    va_end (args);
    printf("%s\n", text);
}

void sr_malloc_test()
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
    


    sr_malloc_debug(log_cb);

    free(p);
}