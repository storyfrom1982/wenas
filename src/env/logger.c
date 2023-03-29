//
// Created by liyong kang on 2022/12/2.
//

#include "env/env.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "task.h"

#if defined( __ANDROID__ )
# include <android/log.h>
#endif

#define __log_text_size			4096
#define __log_file_size         1024 * 1024 * 8
#define __log_pipe_size			1 << 14

#define __path_clear(path) \
        ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[ENV_LOG_LEVEL_COUNT] = {"NONE", "F", "E", "W", "I", "D", "T"};


typedef struct env_logger {
    ___atom_bool inited;
    ___atom_bool writing;
    char *path;
    task_ptr task;
    env_logger_cb printer;
    env_pipe_t *pipe;
}env_logger_t;

static env_logger_t g_logger = {0};

static void env_logger_write_loop(__ptr ctx)
{
    linekv_release((linekv_ptr*)&ctx);
    __fp fp = NULL;
    int64_t n;
    uint64_t res, buf_size = __log_pipe_size;
    unsigned char *buf = (unsigned char *)malloc(buf_size);
//    __pass(buf != NULL);

    if (!env_find_path(g_logger.path)){
       env_make_path(g_logger.path);
    }

    {
        char filename[1024];
        n = snprintf(filename, 1024, "%s/%s", g_logger.path, "0.log");
        filename[n] = '\0';
        fp = env_fopen(filename, "a+t");
//        __pass(fp != NULL);
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        n = snprintf(filename, 1024, "./build/%llu.%llu", *a, *b);
        env_make_path(filename);
    }

    while (1)
    {
        if ((res = env_pipe_read(g_logger.pipe, buf, 1)) == 1){
            n = env_pipe_readable(g_logger.pipe);
            res += env_pipe_read(g_logger.pipe, buf + 1, n < buf_size ? n : buf_size - 1);
        }

        if (res > 0){
            n = env_fwrite(fp, buf, res);
//            __pass(n==res);
            if (env_ftell(fp) > __log_file_size){
                env_fclose(fp);
                char log0[1024];
                n = snprintf(log0, 1024, "%s/0.log", g_logger.path);
                log0[n] = '\0';
                char log1[1024];
                n = snprintf(log1, 1024, "%s/1.log", g_logger.path);
                log1[n] = '\0';
//                __pass(env_move_path(log0, log1));
                fp = env_fopen(log0, "a+t");
//                __pass(fp != NULL);
            }
        }else {
            if (___is_false(&g_logger.inited)){
                break;
            }
        }
    }

Reset:
    ___set_false(&g_logger.writing);

    if (fp){
        env_fclose(fp);
    }
    if (buf){
        uint64_t *a = (uint64_t*)(buf - 16);
        uint64_t *b = (uint64_t*)(buf - 8);
        printf("buf addr: %llu.%llu\n", *a, *b);
        free(buf);
        printf("buf addr: %llu.%llu\n", *a, *b);
    }
}

int env_logger_start(const char *path, env_logger_cb cb)
{
    if (___set_true(&g_logger.inited)){
        g_logger.printer = cb;
        if (path){
            g_logger.path = strdup(path);
            g_logger.pipe = env_pipe_create(__log_pipe_size);
            assert(g_logger.pipe);
            ___set_true(&g_logger.writing);
            g_logger.task = task_create();
            linekv_ptr ctx = linekv_create(1024);
            linekv_add_ptr(ctx, "ctx", &g_logger);
            linekv_add_ptr(ctx, "func", env_logger_write_loop);
            task_post(g_logger.task, ctx);
            __logi(">>>>-------------->\n");
            __logi("Logger start >>>>--------------> %s\n", g_logger.path);
            __logi(">>>>-------------->\n");
        }
    }
    return 0;
Reset:
    if (g_logger.path) free(g_logger.path);
    env_pipe_destroy(&g_logger.pipe);
    return -1;
}

void env_logger_stop()
{
    if (___set_false(&g_logger.inited)
        && ___set_false(&g_logger.writing)){
        env_pipe_stop(g_logger.pipe);
        task_release(&g_logger.task);
        env_pipe_destroy(&g_logger.pipe);
        if (g_logger.path){
            free(g_logger.path);
        }
        memset(&g_logger, 0, sizeof(g_logger));
    }
}

void env_logger_printf(enum env_log_level level, const char *file, int line, const char *fmt, ...)
{
    uint64_t n = 0;
    char text[__log_text_size];

    uint64_t millisecond = ___sys_time() / MICRO_SECONDS;
    n = ___sys_strftime(text, __log_text_size, millisecond / MILLI_SECONDS);

    n += snprintf(text + n, __log_text_size - n, ".%03u [0x%X] %4d %-21s [%s] ", (unsigned int)(millisecond % 1000),
                    ___thread_id(), line, file != NULL ? __path_clear(file) : "<*>", s_log_level_strings[level]);

    va_list args;
    va_start (args, fmt);
    n += vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);

    if (text[n-1] != '\n'){
        if (n < __log_text_size - 1){
            text[n++] = '\n';
            text[n] = '\0';
        }else{
            text[__log_text_size-2] = '\n';
            text[__log_text_size-1] = '\0';
        }
    }

    if (___is_true(&g_logger.writing) && g_logger.pipe != NULL){
        env_pipe_write(g_logger.pipe, text, n);
    }

    if (g_logger.printer != NULL){
        g_logger.printer(level, text);
    }else {
#if defined( __ANDROID__ )
        if (level == ENV_LOG_LEVEL_INFO)
            __android_log_print(ANDROID_LOG_INFO, "srk", "%s", text);
        else if (level == ENV_LOG_LEVEL_WARN)
            __android_log_print(ANDROID_LOG_WARN, "srk", "%s", text);
        else if (level == ENV_LOG_LEVEL_ERROR)
            __android_log_print(ANDROID_LOG_ERROR, "srk", "%s", text);
        else if (level == ENV_LOG_LEVEL_FATAL)
            __android_log_print(ANDROID_LOG_FATAL, "srk", "%s", text);
        else
            __android_log_print(ANDROID_LOG_DEBUG, "srk", "%s", text);
#else
        fprintf(stdout, "%s", text);
        fflush(stdout);
#endif
    }

}
