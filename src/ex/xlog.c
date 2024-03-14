//
// Created by liyong kang on 2022/12/2.
//

#include "ex.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "sys/struct/xmalloc.h"

#define __log_text_size			1024
#define __log_text_end			( __log_text_size - 2 )
#define __log_file_size         1024 * 1024 * 64
#define __log_pipe_size			8192
#define __log_file_path_size    256

#define __path_clear(path) \
        ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[__XLOG_LEVEL_ERROR + 1] = {"D", "I", "E"};


struct xlogrecorder {
    __atom_bool lock;
    __atom_bool running;
    __atom_bool recording;
    __xfile_ptr fp;
    __xprocess_ptr pid;
    __xlog_cb print_cb;
    char log0[__log_file_path_size], log1[__log_file_path_size];
};

static struct xlogrecorder global_logrecorder = {0}, *srecorder = &global_logrecorder;


struct xlogbuf {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint64_t len;
    __atom_size writer;
    __atom_size reader;
    char buf[__log_pipe_size];
}slogbuf = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond  = PTHREAD_COND_INITIALIZER,
    .len = __log_pipe_size,
    .writer = 0,
    .reader = 0
};

typedef struct xlogbuf* xlogbuf_ptr;
static xlogbuf_ptr spipe = &slogbuf;

#define __spipe_readable(sp)    (sp->writer - sp->reader)
#define __spipe_writable(sp)    (sp->len - sp->writer + sp->reader)

static inline uint64_t __spipe_read(void *buf, uint64_t len)
{
    uint64_t readable = spipe->writer - spipe->reader;

    if (readable > len){
        readable = len;
    }

    if (readable > 0){

        uint64_t leftover = spipe->len - ( spipe->reader & ( spipe->len - 1 ) );
        if (leftover >= readable){
            mcopy(((uint8_t*)buf), spipe->buf + (spipe->reader & (spipe->len - 1)), readable);
        }else {
            mcopy(((uint8_t*)buf), spipe->buf + (spipe->reader & (spipe->len - 1)), leftover);
            mcopy(((uint8_t*)buf) + leftover, spipe->buf, readable - leftover);
        }
        __atom_add(spipe->reader, readable);
        pthread_cond_broadcast(&spipe->cond);
    }

    return readable;
}

static inline uint64_t __spipe_write(void *data, uint64_t len)
{
    uint64_t  writable = spipe->len - spipe->writer + spipe->reader;

    if (writable > len){
        writable = len;
    }

    if (writable > 0){

        uint64_t leftover = spipe->len - ( spipe->writer & ( spipe->len - 1 ) );
        if (leftover >= writable){
            mcopy(spipe->buf + (spipe->writer & (spipe->len - 1)), ((uint8_t*)data), writable);
        }else {
            mcopy(spipe->buf + (spipe->writer & (spipe->len - 1)), ((uint8_t*)data), leftover);
            mcopy(spipe->buf, ((uint8_t*)data) + leftover, writable - leftover);
        }
        __atom_add(spipe->writer, writable);
        pthread_cond_broadcast(&spipe->cond);
    }

    return writable;
}


static void* __xlog_recorder_loop(void *ctx)
{
    __xlogd("__xlog_recorder_loop enter\n");

    int64_t n;
    uint64_t readlen;
    char buf[__log_pipe_size];

    __set_true(srecorder->recording);
    
    while (__is_true(srecorder->running))
    {
        while ((readlen = __spipe_readable(spipe)) == 0 && __is_true(srecorder->running)){
            pthread_mutex_lock(&spipe->mutex);
            pthread_cond_broadcast(&spipe->cond);
            pthread_cond_wait(&spipe->cond, &spipe->mutex);
            pthread_mutex_unlock(&spipe->mutex);
        }

        if (readlen == 0){
            break;
        }

        __spipe_read(buf, readlen);

        n = __xapi->fwrite(srecorder->fp, buf, readlen);
        if (n != readlen){
            break;
        }
        
        fflush(srecorder->fp);

        if (__xapi->ftell(srecorder->fp) > __log_file_size){
            __xapi->fclose(srecorder->fp);
            __xapi->move_path(srecorder->log0, srecorder->log1);
            srecorder->fp = __xapi->fopen(srecorder->log0, "a+t");
            if (srecorder->fp == NULL){
                break;
            }
        }
    }

    __xlogd("__xlog_recorder_loop exit\n");

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

static void memory_leak_cb(const char *leak_location, uint64_t pid)
{
    __xloge("[0x%X] %s\n", pid, leak_location);
}

void xlog_recorder_close()
{
    //先设置关闭状态    
    if (__set_false(srecorder->running)){
        __set_false(srecorder->recording);

        //再清空管道，确保写入线程退出管道，并且不会再去写日志
        pthread_mutex_lock(&spipe->mutex);
        pthread_cond_broadcast(&spipe->cond);
        pthread_mutex_unlock(&spipe->mutex);
        pthread_mutex_lock(&spipe->mutex);
        pthread_cond_broadcast(&spipe->cond);
        pthread_mutex_unlock(&spipe->mutex);

        __xapi->process_free(srecorder->pid);

        __xlogi(">>>>-------------->\n");
        __xlogi("Log stop >>>>-------------->\n");
        __xlogi(">>>>-------------->\n");

        __atom_lock(srecorder->lock);
        __xapi->fclose(srecorder->fp);
        __atom_unlock(srecorder->lock);

        mclear(srecorder, sizeof(global_logrecorder));
    }

#ifdef XMALLOC_BACKTRACE
    xmalloc_leak_trace(memory_leak_cb);
#endif
}

extern void env_backtrace_setup();

int xlog_recorder_open(const char *path, __xlog_cb cb)
{
#ifdef XMALLOC_BACKTRACE
    env_backtrace_setup();
#endif
    // test();

    static char buf[BUFSIZ];
    setvbuf(stdout, buf, _IONBF, BUFSIZ);

    xlog_recorder_close();

    srecorder->print_cb = cb;

    if (!__xapi->check_path(path)){
        __xbreak(!__xapi->make_path(path));
    }

    int n = snprintf(srecorder->log0, __log_file_path_size - 1, "%s/0.log", path);
    srecorder->log0[n] = '\0';
    n = snprintf(srecorder->log1, __log_file_path_size - 1, "%s/1.log", path);
    srecorder->log0[n] = '\0';

    __atom_lock(srecorder->lock);
    srecorder->fp = __xapi->fopen(srecorder->log0, "a+t");
    __atom_unlock(srecorder->lock);

    __xbreak(srecorder->fp == NULL);
    
    __xlogi(">>>>-------------->\n");
    __xlogi("Log start >>>>--------------> %s\n", srecorder->log0);
    __xlogi(">>>>-------------->\n");

    srecorder->pid = __xapi->process_create(__xlog_recorder_loop, &global_logrecorder);
    __xbreak(srecorder->pid == NULL);

    __set_true(srecorder->running);

    return 0;

Clean:

    if (srecorder->pid != 0){
        __xapi->process_free(srecorder->pid);
        srecorder->pid = 0;
    }
    if (srecorder->fp){
        __xapi->fclose(srecorder->fp);
        srecorder->fp = NULL;
    }
    __set_false(srecorder->running);
    __set_false(srecorder->recording);
    __set_false(srecorder->lock);

    return -1;
}

void __xlog_debug(const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size];    
    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_end - n, fmt, args);
    va_end (args);

    fprintf(stderr, "%s", text);
}

void __xlog_printf(enum __xlog_level level, const char *file, int line, const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size];

    uint64_t millisecond = __xapi->time() / MICRO_SECONDS;
    // memory leak
    n = __xapi->strftime(text, __log_text_end, millisecond / MILLI_SECONDS);
    n += snprintf(text + n, __log_text_end - n, ".%03u [0x%08X] %4d %-21s [%s] ", 
                    (unsigned int)(millisecond % 1000), __xapi->process_self(), 
                    line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    // n += snprintf(text + n, __log_text_end - n, "[0x%08X] [%lu.%03u] %4d %-21s [%s] ", 
    //                 __xapi->process_self(), (millisecond / 1000), (unsigned int)(millisecond % 1000), 
    //                 line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_end - n, fmt, args);
    va_end (args);

    __atom_lock(srecorder->lock);

    // 这里只向文件写入实际的输入的内容
    // 最大输入内容 0-4094=4095
    if (__is_true(srecorder->recording)){
        // printf("XLOG: __xlog_printf >>>>>---------------------> enter\n");
        // 写入管道
        uint64_t pos = 0;
        while (pos < n){
            pthread_mutex_lock(&spipe->mutex);
            if (__spipe_writable(spipe) == 0){
                pthread_cond_wait(&spipe->cond, &spipe->mutex);
            }
            pos += __spipe_write(text + pos, n - pos);                
            pthread_mutex_unlock(&spipe->mutex);
        }
        // printf("XLOG: __xlog_printf >>>>>---------------------> exit\n");
    }else if (srecorder->fp){
        // 直接写文件
        __xapi->fwrite(srecorder->fp, text, n);
    }

    __atom_unlock(srecorder->lock);

    // 长度 n 不会越界，因为 snprintf 限制了写入长度
    if (text[n] != '\0'){
        // 为字符串添加 \0，打印函数可能需要一个结束符
        text[n+1] = '\0';
    }

    if (srecorder->print_cb != NULL){
        srecorder->print_cb(level, text);
    }else {
        fprintf(stdout, "%s", text);
        fflush(stdout);
    }

}
