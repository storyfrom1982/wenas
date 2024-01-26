//
// Created by liyong kang on 2022/12/2.
//

#include "ex/ex.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "task.h"

#define __log_text_size			4096
#define __log_text_end			( __log_text_size - 2 )
#define __log_file_size         1024 * 1024 * 8
// #define __log_pipe_size			1 << 14
#define __log_pipe_size			1 << 1

#define __path_clear(path) \
        ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[EX_LOG_LEVEL_ERROR + 1] = {"D", "I", "E"};


typedef struct ex_log_file {
    ___atom_bool lock;
    ___atom_bool running;
    ___atom_bool writing;
    char *log0, *log1;
    __ex_fp fp;
    __ex_pipe *pipe;
    __ex_task_ptr task;
    __ex_log_cb print_cb;
}__ex_log_file;

static __ex_log_file g_log_file = {0};

static void ex_log_file_write_loop(__ex_task_ctx_ptr ctx)
{
    __ex_logd("ex_log_file_write_loop enter\n");

    int64_t n;
    uint64_t res, buf_size = __log_pipe_size;
    unsigned char *buf = (unsigned char *)malloc(buf_size);
    __ex_check(buf != NULL);

    {
        // 只有在 Release 模式下才能获取到指针的信息
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        __ex_logd("buf addr: %llu.%llu\n", *a, *b);
    }

    ___set_true(&g_log_file.writing);
    
    while (1)
    {
        // printf("__ex_pipe_read enter\n");
        if ((res = __ex_pipe_read(g_log_file.pipe, buf, 1)) == 1){
            // printf("__ex_pipe_read 1\n");
            n = __ex_pipe_readable(g_log_file.pipe);
            // printf("__ex_pipe_readable %lu\n", n);
            res += __ex_pipe_read(g_log_file.pipe, buf + 1, n < buf_size ? n : buf_size - 1);
            // printf("__ex_pipe_read exit\n");
        }

        if (res > 0){
            n = __ex_fwrite(g_log_file.fp, buf, res);
            // 日志线程本身不能同时读写日志管道
            // __ex_check(n == res);
            __ex_break(n == res);

            if (__ex_ftell(g_log_file.fp) > __log_file_size){
                __ex_fclose(g_log_file.fp);
                __ex_move_path(g_log_file.log0, g_log_file.log1);
                g_log_file.fp = __ex_fopen(g_log_file.log0, "a+t");
                // 日志线程本身不能同时读写日志管道
                // __ex_check(g_log_file.fp != NULL);
                __ex_break(g_log_file.fp != NULL);
            }
        }else {
            if (___is_false(&g_log_file.running)){
                break;
            }
        }
    }

Clean:

    if (buf){
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        __ex_logd("buf addr: %llu.%llu\n", *a, *b);
        free(buf);
        __ex_logd("buf addr: %llu.%llu\n", *a, *b);
    }

    __ex_logd("ex_log_file_write_loop exit\n");
}

void test3()
{
    void *p = malloc(256);
    // free(p);
    p = NULL;
    // *(int*)p = 0;
}

void test2()
{
    void *p = malloc(256);
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
    __ex_loge("%s\n", leak_location);
}

void __ex_log_file_close()
{
    //先设置关闭状态    
    if (___set_false(&g_log_file.running)){
        ___set_false(&g_log_file.writing);
        //再清空管道，确保写入线程退出管道，并且不会再去写日志
        __ex_pipe_break(g_log_file.pipe);
        __ex_task_destroy(&g_log_file.task);
        __ex_pipe_destroy(&g_log_file.pipe);     
        free(g_log_file.log0);
        free(g_log_file.log1);

        __ex_logi(">>>>-------------->\n");
        __ex_logi("Log stop >>>>-------------->\n");
        __ex_logi(">>>>-------------->\n");

        ___atom_lock(&g_log_file.lock);
        __ex_fclose(g_log_file.fp);
        ___atom_unlock(&g_log_file.lock);

        memset(&g_log_file, 0, sizeof(g_log_file));
    }

#if defined(ENV_MALLOC_BACKTRACE)
    __ex_memory_leak_trace(memory_leak_cb);
#endif
}

int __ex_log_file_open(const char *path, __ex_log_cb cb)
{
    // test();

    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IONBF, BUFSIZ);

    __ex_log_file_close();

    g_log_file.print_cb = cb;

    int len = strlen(path) + strlen("/0.log") + 1;
    if (!__ex_find_path(path)){
        __ex_check(__ex_make_path(path));
    }

    g_log_file.log0 = calloc(1, len);
    __ex_check(g_log_file.log0);
    g_log_file.log1 = calloc(1, len);
    __ex_check(g_log_file.log1);

    snprintf(g_log_file.log0, len, "%s/0.log", path);
    snprintf(g_log_file.log1, len, "%s/1.log", path);

    ___atom_lock(&g_log_file.lock);
    g_log_file.fp = __ex_fopen(g_log_file.log0, "a+t");
    ___atom_unlock(&g_log_file.lock);

    __ex_check(g_log_file.fp);
    
    __ex_logi(">>>>-------------->\n");
    __ex_logi("Log start >>>>--------------> %s\n", g_log_file.log0);
    __ex_logi(">>>>-------------->\n");  

    g_log_file.pipe = __ex_pipe_create(__log_pipe_size);
    __ex_check(g_log_file.pipe);

    g_log_file.task = __ex_task_run(ex_log_file_write_loop, &g_log_file);
    __ex_check(g_log_file.task);

    ___set_true(&g_log_file.running);

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
        __ex_pipe_destroy(&g_log_file.pipe);
        g_log_file.pipe = NULL;
    }
    if (g_log_file.task){
        __ex_task_destroy(&g_log_file.task);
        g_log_file.task = NULL;
    }
    if (g_log_file.fp){
        __ex_fclose(g_log_file.fp);
        g_log_file.fp = NULL;
    }
    ___set_false(&g_log_file.running);
    ___set_false(&g_log_file.writing);
    ___set_false(&g_log_file.lock);

    return -1;
}

void __ex_log_printf(enum __ex_log_level level, const char *file, int line, const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size];

    uint64_t millisecond = __ex_time() / MICRO_SECONDS;
    // memory leak
    // n = ___sys_strftime(text, __log_text_end, millisecond / MILLI_SECONDS);

    n += snprintf(text + n, __log_text_end - n, "[0x%08X] [%lu.%03u] %4d %-21s [%s] ", 
                    __ex_thread_id(), (millisecond / 1000) % 1000, (unsigned int)(millisecond % 1000), 
                    line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_end - n, fmt, args);
    va_end (args);

    ___atom_lock(&g_log_file.lock);

    // 这里只向文件写入实际的输入的内容
    // 最大输入内容 0-4094=4095
    if (___is_true(&g_log_file.writing)){
        // 写入管道
        __ex_pipe_write(g_log_file.pipe, text, n);
    }else {
        // 直接写文件
        if (g_log_file.fp){
            __ex_fwrite(g_log_file.fp, text, n);
        }
    }

    ___atom_unlock(&g_log_file.lock);

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