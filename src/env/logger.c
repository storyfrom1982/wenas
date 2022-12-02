//
// Created by liyong kang on 2022/12/2.
//

#include "env/logger.h"
#include "env/thread.h"
#include "env/file_system.h"
#include "sys/struct/lineardb_pipe.h"

#define __log_text_size			4096
#define __log_file_size         1024 * 1024 * 2

#define __path_clear(path) \
    ( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )

static const char *s_log_level_strings[ENV_LOG_LEVEL_COUNT] = {"NONE", "F", "E", "W", "I", "D", "T"};


typedef struct env_logger {
    uint8_t running;
    char *path;
    env_thread_t tid;
    logger_cb_t printer;
    linedb_pipe_t *lpipe;
    env_mutex_t mutex;
}env_logger_t;

static env_logger_t g_logger = {0};

static void* env_logger_write_loop(void *p)
{
    int fd;
    ssize_t n;

    if (!env_fs_dir_exists(g_logger.path)){
        env_fs_mkpath(g_logger.path);
    }

    {
        char log_path[1024];
        n = snprintf(log_path, 1024, "%s/%s", g_logger.path, "0.log");
        log_path[n] = '\0';
        fd = env_open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
        n = env_write(fd, (void*)"\n\n\nLogger start >>>>-------------->\n\n\n", strlen("\n\n\nLogger start >>>>-------------->\n\n\n"));
    }

    linedb_t *ldb;

    while (1)
    {
        env_mutex_lock(&g_logger.mutex);

        ldb = linedb_pipe_hold_block(g_logger.lpipe);

        if (ldb){
            n = env_write(fd, __dataof_linedb(ldb), __sizeof_data(ldb));
            linedb_pipe_free_block(g_logger.lpipe, ldb);
            env_mutex_signal(&g_logger.mutex);
            env_mutex_unlock(&g_logger.mutex);

            if (env_fs_ltell(fd) > __log_file_size){
                env_close(fd);
                char log_path[1024];
                n = snprintf(log_path, 1024, "%s/0.log", g_logger.path);
                log_path[n] = '\0';
                char tmp_path[1024];
                n = snprintf(tmp_path, 1024, "%s/1.log", g_logger.path);
                tmp_path[n] = '\0';
                env_rename(log_path, tmp_path);
                fd = env_open(log_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if (fd < 0){
                    return NULL;
                }
            }

        }else {

            if (!g_logger.running){
                env_mutex_unlock(&g_logger.mutex);
                break;
            }
            env_mutex_wait(&g_logger.mutex);
            env_mutex_unlock(&g_logger.mutex);
        }
    }

    env_close(fd);

    return NULL;
}

int env_logger_start(const char *path, logger_cb_t cb)
{
    if (!g_logger.running){
        g_logger.running = 1;
        g_logger.printer = cb;
        g_logger.path = strdup(path);
        if (g_logger.path){
            env_mutex_init(&g_logger.mutex);
            g_logger.lpipe = linedb_pipe_build(1 << 15);
            if (!g_logger.lpipe){
                return -1;
            }
            if (env_thread_create(&g_logger.tid, env_logger_write_loop, &g_logger) != 0){
                linedb_pipe_destroy(&g_logger.lpipe);
                return -1;
            }
        }
    }
    return 0;
}

void env_logger_stop()
{
    if (g_logger.running){
        g_logger.running = 0;
        env_mutex_lock(&g_logger.mutex);
        env_mutex_signal(&g_logger.mutex);
        env_mutex_unlock(&g_logger.mutex);
        env_thread_join(g_logger.tid);
        env_mutex_destroy(&g_logger.mutex);
        linedb_pipe_destroy(&g_logger.lpipe);
        if (g_logger.path){
            free(g_logger.path);
        }
        memset(&g_logger, 0, sizeof(g_logger));
    }
}

void env_logger_printf(enum env_log_level level, const char *tag, const char *file, int line, const char *func, const char *fmt, ...)
{
    size_t n = 0;
    char text[__log_text_size];

    uint64_t millisecond = env_time() / MICROSEC;
    time_t sec = millisecond / MILLISEC;
    n = strftime(text, __log_text_size, "%Y-%m-%d-%H:%M:%S", localtime(&sec));

    n += snprintf(text + n, __log_text_size - n, ".%03u [0x%X] [%s] [%s] ", (unsigned int)(millisecond % 1000),
                  env_thread_self(), s_log_level_strings[level], tag);

    const char *log = text + n;
    n += snprintf(text + n, __log_text_size - n, "[%s %d %s] ", file != NULL ? __path_clear(file) : "<*>", line, func);

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

    if (g_logger.lpipe != NULL){
        env_mutex_lock(&g_logger.mutex);
        while (linedb_pipe_writable(g_logger.lpipe) < n){
            env_mutex_wait(&g_logger.mutex);
        }
        linedb_pipe_write(g_logger.lpipe, text, n - 1); //去掉'\0'
        env_mutex_signal(&g_logger.mutex);
        env_mutex_unlock(&g_logger.mutex);
    }

    if (g_logger.printer != NULL){
        g_logger.printer(level, tag, text, log);
    }
}
