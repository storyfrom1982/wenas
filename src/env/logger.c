//
// Created by liyong kang on 2022/12/2.
//

#include "env/env.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define __log_text_size			4096
#define __log_file_size         1024 * 1024 * 8

#define __path_clear(path) \
    ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[ENV_LOG_LEVEL_COUNT] = {"NONE", "F", "E", "W", "I", "D", "T"};


typedef struct env_logger {
    __atombool running;
    char *path;
    env_thread_t tid;
    env_logger_cb printer;
    env_pipe_t *pipe;
}env_logger_t;

static env_logger_t g_logger = {0};

static __result env_logger_write_loop(__ptr ctx)
{
    __fp fp = NULL;
    __sint64 n;
    __uint64 res;
    __sym *buf = (__sym *)malloc(env_pipe_writable(g_logger.pipe));
    __pass(buf != NULL);

    if (!env_find_path(g_logger.path)){
        __pass(env_make_path(g_logger.path));
    }

    {
        __sym filename[1024];
        n = snprintf(filename, 1024, "%s/%s", g_logger.path, "0.log");
        filename[n] = '\0';
        fp = env_fopen(filename, "a+t");
        __pass(fp != NULL);
        n = env_fwrite(fp, (void*)"\n\n\nLogger start >>>>-------------->\n\n\n", strlen("\n\n\nLogger start >>>>-------------->\n\n\n"));
        __pass(n > 0);
    }

    while (1)
    {
        if ((res = env_pipe_read(g_logger.pipe, buf, 1)) == 1){
            res += env_pipe_read(g_logger.pipe, buf + 1, env_pipe_readable(g_logger.pipe));
        }

        if (res > 0){
            n = env_fwrite(fp, buf, res);
            __pass(n==res);
            if (env_ftell(fp) > __log_file_size){
                env_fclose(fp);
                __sym log0[1024];
                n = snprintf(log0, 1024, "%s/0.log", g_logger.path);
                log0[n] = '\0';
                __sym log1[1024];
                n = snprintf(log1, 1024, "%s/1.log", g_logger.path);
                log1[n] = '\0';
                __pass(env_move_path(log0, log1));
                fp = env_fopen(log0, "a+t");
                __pass(fp != NULL);
            }
        }else {
            if (__is_false(g_logger.running)){
                break;
            }
        }
    }

Reset:
    if (fp){
        env_fclose(fp);
    }
    if (buf){
        free(buf);
    }
    return 0;
}

int env_logger_start(const __sym *path, env_logger_cb cb)
{
    if (__set_true(g_logger.running)){
        g_logger.printer = cb;
        g_logger.path = strdup(path);
        if (g_logger.path){
            g_logger.pipe = env_pipe_create(1 << 15);
            __pass(g_logger.pipe != NULL);
            __pass(env_thread_create(&g_logger.tid, env_logger_write_loop, &g_logger) == 0);
        }
    }
    return 0;
Reset:
    env_pipe_destroy(&g_logger.pipe);
    return -1;
}

void env_logger_stop()
{
    if (__set_false(g_logger.running)){
        env_pipe_stop(g_logger.pipe);
        env_thread_destroy(g_logger.tid);
        env_pipe_destroy(&g_logger.pipe);
        if (g_logger.path){
            free(g_logger.path);
        }
        memset(&g_logger, 0, sizeof(g_logger));
    }
}

void env_logger_printf(enum env_log_level level, const __sym *file, __sint32 line, const __sym *fmt, ...)
{
    __uint64 n = 0;
    __sym text[__log_text_size];

    __uint64 millisecond = env_time() / MICRO_SECONDS;
    n = env_strtime(text, __log_text_size, millisecond / MILLI_SECONDS);

    n += snprintf(text + n, __log_text_size - n, ".%03u [0x%lX] %4d %-21s [%s] ", (unsigned int)(millisecond % 1000),
                  env_thread_self(), line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);

    if (n < __log_text_size - 1){
        text[n++] = '\n';
        text[n++] = '\0';
    }else{
        text[__log_text_size-2] = '\n';
        text[__log_text_size-1] = '\0';
    }

    if (g_logger.pipe != NULL){
        env_pipe_write(g_logger.pipe, text, n - 1);
    }

    if (g_logger.printer != NULL){
        g_logger.printer(level, text);
    }else {
        fprintf(stdout, "%s", text);
    }
}
