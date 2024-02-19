//
// Created by liyong kang on 2022/12/2.
//

#include "ex/ex.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "sys/struct/xbuf.h"

#define __log_text_size			4096
#define __log_text_end			( __log_text_size - 2 )
#define __log_file_size         1024 * 1024 * 8
// #define __log_pipe_size			1 << 14
#define __log_pipe_size			1 << 1

#define __path_clear(path) \
        ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[__XLOG_LEVEL_ERROR + 1] = {"D", "I", "E"};


typedef struct xlog_file {
    __atom_bool lock;
    __atom_bool running;
    __atom_bool writing;
    char *log0, *log1;
    __xfile_ptr fp;
    xpipe_ptr pipe;
    __xprocess_ptr pid;
    __xlog_cb print_cb;
}__xlog_file;

static __xlog_file g_log_file = {0};

static void* __xlog_file_write_loop(void *ctx)
{
    __xlogd("__xlog_file_write_loop enter\n");

    int64_t n;
    uint64_t res, buf_size = __log_pipe_size;
    unsigned char *buf = (unsigned char *)malloc(buf_size);
    __xcheck(buf != NULL);

    {
        // 只有在 Release 模式下才能获取到指针的信息
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        __xlogd("buf addr: %llu.%llu\n", *a, *b);
    }

    __set_true(g_log_file.writing);
    
    while (1)
    {
        // printf("xpipe_read enter\n");
        // TODO xpipe 不支持无锁模式下的多写一读
        // TODO 是否要频繁写文件？是否可以一次性写入 4K
        if ((res = xpipe_read(g_log_file.pipe, buf, 1)) == 1){
            // printf("xpipe_read 1\n");
            n = xpipe_readable(g_log_file.pipe);
            // printf("xpipe_readable %lu\n", n);
            res += xpipe_read(g_log_file.pipe, buf + 1, n < buf_size ? n : buf_size - 1);
            // printf("xpipe_read exit\n");
        }

        if (res > 0){
            n = __xapi->fwrite(g_log_file.fp, buf, res);
            // 日志线程本身不能同时读写日志管道
            // __xcheck(n == res);
            __xcheck(n == res);

            if (__xapi->ftell(g_log_file.fp) > __log_file_size){
                __xapi->fclose(g_log_file.fp);
                __xapi->move_path(g_log_file.log0, g_log_file.log1);
                g_log_file.fp = __xapi->fopen(g_log_file.log0, "a+t");
                // 日志线程本身不能同时读写日志管道
                // __xcheck(g_log_file.fp != NULL);
                __xcheck(g_log_file.fp != NULL);
            }
        }else {
            if (__is_false(g_log_file.running)){
                break;
            }
        }
    }

Clean:

    if (buf){
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        __xlogd("buf addr: %llu.%llu\n", *a, *b);
        free(buf);
        __xlogd("buf addr: %llu.%llu\n", *a, *b);
    }

    __xlogd("__xlog_file_write_loop exit\n");

    return NULL;
}

void test3()
{
    void *p = strdup("test");
    // free(p);
    p = NULL;
    // *(int*)p = 0;
}

void test2()
{
    void *p = strndup("test", strlen("test"));
    test3();
}

void test1()
{
    void *p = malloc(256);
    test2();
}

static void test(){
    void *p = malloc(256);
    test1();
}

static void memory_leak_cb(const char *leak_location)
{
    __xloge("%s\n", leak_location);
}

void __xlog_close()
{
    //先设置关闭状态    
    if (__set_false(g_log_file.running)){
        __set_false(g_log_file.writing);
        //再清空管道，确保写入线程退出管道，并且不会再去写日志
        xpipe_break(g_log_file.pipe);
        __xapi->process_free(g_log_file.pid);
        xpipe_free(&g_log_file.pipe);
        free(g_log_file.log0);
        free(g_log_file.log1);

        __xlogi(">>>>-------------->\n");
        __xlogi("Log stop >>>>-------------->\n");
        __xlogi(">>>>-------------->\n");

        __atom_lock(g_log_file.lock);
        __xapi->fclose(g_log_file.fp);
        __atom_unlock(g_log_file.lock);

        mclear(&g_log_file, sizeof(g_log_file));
    }

#if defined(XMALLOC_BACKTRACE)
    xmalloc_leak_trace(memory_leak_cb);
#endif
}

extern void env_backtrace_setup();

int __xlog_open(const char *path, __xlog_cb cb)
{
    env_backtrace_setup();
    // test();

    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IONBF, BUFSIZ);

    __xlog_close();

    g_log_file.print_cb = cb;

    int len = strlen(path) + strlen("/0.log") + 1;
    if (!__xapi->check_path(path)){
        __xcheck(__xapi->make_path(path));
    }

    g_log_file.log0 = calloc(1, len);
    __xcheck(g_log_file.log0);
    g_log_file.log1 = calloc(1, len);
    __xcheck(g_log_file.log1);

    snprintf(g_log_file.log0, len, "%s/0.log", path);
    snprintf(g_log_file.log1, len, "%s/1.log", path);

    __atom_lock(g_log_file.lock);
    g_log_file.fp = __xapi->fopen(g_log_file.log0, "a+t");
    __atom_unlock(g_log_file.lock);

    __xcheck(g_log_file.fp);
    
    __xlogi(">>>>-------------->\n");
    __xlogi("Log start >>>>--------------> %s\n", g_log_file.log0);
    __xlogi(">>>>-------------->\n");  

    g_log_file.pipe = xpipe_create(__log_pipe_size);
    __xcheck(g_log_file.pipe);

    g_log_file.pid = __xapi->process_create(__xlog_file_write_loop, &g_log_file);
    __xcheck(g_log_file.pid != 0);

    __set_true(g_log_file.running);

    return 0;

Clean:

    if (g_log_file.log0){
        free(g_log_file.log0);
        g_log_file.log0 = NULL;
    }
    if (g_log_file.log1){
        free(g_log_file.log1);
        g_log_file.log1 = NULL;
    }
    if (g_log_file.pipe){
        xpipe_free(&g_log_file.pipe);
        g_log_file.pipe = NULL;
    }
    if (g_log_file.pid != 0){
        __xapi->process_free(g_log_file.pid);
        g_log_file.pid = 0;
    }
    if (g_log_file.fp){
        __xapi->fclose(g_log_file.fp);
        g_log_file.fp = NULL;
    }
    __set_false(g_log_file.running);
    __set_false(g_log_file.writing);
    __set_false(g_log_file.lock);

    return -1;
}

void __xlog_printf(enum __xlog_level level, const char *file, int line, const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size];

    uint64_t millisecond = __xapi->time() / MICRO_SECONDS;
    // memory leak
    // n = ___sys_strftime(text, __log_text_end, millisecond / MILLI_SECONDS);

    n += snprintf(text + n, __log_text_end - n, "[0x%08X] [%lu.%03u] %4d %-21s [%s] ", 
                    __xapi->process_self(), (millisecond / 1000), (unsigned int)(millisecond % 1000), 
                    line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_end - n, fmt, args);
    va_end (args);

    __atom_lock(g_log_file.lock);

    // 这里只向文件写入实际的输入的内容
    // 最大输入内容 0-4094=4095
    if (__is_true(g_log_file.writing)){
        // 写入管道
        xpipe_write(g_log_file.pipe, text, n);
    }else {
        // 直接写文件
        if (g_log_file.fp){
            __xapi->fwrite(g_log_file.fp, text, n);
        }
    }

    __atom_unlock(g_log_file.lock);

    // 长度 n 不会越界，因为 snprintf 限制了写入长度
    if (text[n] != '\0'){
        // 为字符串添加 \0，打印函数可能需要一个结束符
        text[n+1] = '\0';
    }

    if (g_log_file.print_cb != NULL){
        g_log_file.print_cb(level, text);
    }else {
        fprintf(stdout, "%s", text);
        fflush(stdout);
    }

}