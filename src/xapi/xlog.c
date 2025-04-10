//
// Created by liyong kang on 2022/12/2.
//

#include "xapi.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "xnet/xmalloc.h"

#include "uv.h"

#define __log_text_size			1024 * 1024
#define __log_file_size         1024 * 1024 * 64
#define __log_path_max_len      256

#define __path_clear(path) \
        ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[__XLOG_LEVEL_ERROR + 1] = {"D", "I", "E"};


struct xlogrecorder {
    __atom_bool lock;
    __xfile_t fd;
    __xlog_cb cb;
    char log0[__log_path_max_len], log1[__log_path_max_len];
};

static struct xlogrecorder global_logrecorder = {0}, *gloger = &global_logrecorder;

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

static void memory_leak_cb(const char *leak_location, uint64_t pid)
{
    __xlogi("[0x%X] %s\n", pid, leak_location);
}

void xlog_recorder_close()
{
    __xlogi(">>>>-------------->\n");
    __xlogi("Log stop >>>>-------------->\n");
    __xlogi(">>>>-------------->\n");

    if (gloger->fd){
        __xapi->fs_file_close(gloger->fd);
    }

    mclear(gloger, sizeof(global_logrecorder));

#ifdef XMALLOC_ENABLE
    xmalloc_leak_trace(memory_leak_cb);
#endif
}

extern void env_backtrace_setup();

int xlog_recorder_open(const char *path, __xlog_cb cb)
{

#ifdef UNWIND_BACKTRACE
    env_backtrace_setup();
    // test();
#endif

    gloger->cb = cb;

    if (!__xapi->fs_dir_exist(path)){
        __xcheck(__xapi->fs_path_maker(path) == -1);
    }

    int n = snprintf(gloger->log0, __log_path_max_len - 1, "%s/0.log", path);
    gloger->log0[n] = '\0';
    n = snprintf(gloger->log1, __log_path_max_len - 1, "%s/1.log", path);
    gloger->log0[n] = '\0';

    gloger->fd = __xapi->fs_file_open(gloger->log0, XAPI_FS_FLAG_WRITE, 0644);
    __xcheck(gloger->fd == -1);

    __xlogi(">>>>-------------->\n");
    __xlogi("Log start >>>>--------------> %s\n", gloger->log0);
    __xlogi(">>>>-------------->\n");    

    return 0;

XClean:

    if (gloger->fd){
        __xapi->fs_file_close(gloger->fd);
        gloger->fd = 0;
    }

    return -1;
}

void __xlog_debug(const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size + 2];
    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);

    fprintf(stderr, "%s", text);
}

void __xlog_printf(enum __xlog_level level, const char *file, int line, const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size + 2];

    uint64_t millisecond = __xapi->time() / MICRO_SECONDS;
    n = __xapi->strftime(text, __log_text_size, millisecond / MILLI_SECONDS);
    n += snprintf(text + n, __log_text_size - n, ".%03u [0x%08X] %4d %-16s [%s] ", 
                    (unsigned int)(millisecond % 1000), __xapi->thread_self(), 
                    line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    if (__XLOG_LEVEL_ERROR == level){
        n += snprintf(text + n, __log_text_size - n, "<%s> ", strerror(errno));
    }

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);

    if (gloger->fd){
        __atom_lock(gloger->lock);
        __xapi->fs_file_write(gloger->fd, text, n);
        if (__xapi->fs_file_tell(gloger->fd) > __log_file_size){
            __xapi->fs_file_close(gloger->fd);
            if (__xapi->fs_file_exist(gloger->log1)){
                __xapi->fs_file_remove(gloger->log1);
            }
            __xapi->fs_path_rename(gloger->log0, gloger->log1);
            gloger->fd = __xapi->fs_file_open(gloger->log0, XAPI_FS_FLAG_WRITE, 0644);
            // gloger->fd = __xapi->fs_file_open(gloger->log0, UV_FS_O_RDWR | UV_FS_O_CREAT | UV_FS_O_APPEND, 0644);
            // __xcheck(gloger->fd == -1);
        }
        __atom_unlock(gloger->lock);
    }

    // 长度 n 不会越界，因为 snprintf 限制了写入长度
    if (text[n] != '\0'){
        // 为字符串添加 \0，打印函数可能需要一个结束符
        text[n+1] = '\0';
    }

    if (gloger->cb != NULL){
        gloger->cb(level, text);
    }else {
        fprintf(stdout, "%s", text);
        fflush(stdout);
    }

    // if (__XLOG_LEVEL_ERROR == level){
    //     exit(0);
    // }
}
