#include "env/crash_backtrace.h"
#include <stdio.h>

static void test1(){
    int *p = NULL;
    *p = 0;
}

static void test2()
{
    test1();
}

static void test3()
{
    test2();
}

static void test4()
{
    test3();
}

void crash_backtrace_test()
{
    printf("crash_backtrace_test thread[%x]\n", pthread_self());
    env_crash_backtrace_setup();
    // test4();
}