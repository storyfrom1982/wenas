#include "env/env.h"


#if defined(OS_WINDOWS)
#include <Windows.h>
#include <pthread.h>
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;
#else //!defined(OS_WINDOWS)
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;
#endif //defined(OS_WINDOWS)

#include <stdlib.h>
#include <string.h>


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


__sint32 env_thread_create(env_thread_ptr *p_ptr, env_thread_cb cb, __ptr ctx)
{
    env_thread_ptr ptr = malloc(sizeof(pthread_t));
    if (ptr == NULL) {
        return -1;
    }
    if (pthread_create((pthread_t*)ptr, NULL, cb, ctx) != 0) {
        free(ptr);
        return -1;
    }
    *p_ptr = ptr;
    return 0;
}

__uint64 env_thread_self()
{
#if defined(OS_WINDOWS)
    return (__uint64)pthread_self().p;
#else
    return (__uint64)pthread_self();
#endif
}

__uint64 env_thread_id(env_thread_ptr thread_ptr)
{
#if defined(OS_WINDOWS)
    return (__uint64)((pthread_t*)thread_ptr)->p;
#else
    return (__uint64)*((pthread_t*)thread_ptr);
#endif
}

void env_thread_destroy(env_thread_ptr *p_ptr)
{
    if (p_ptr && *p_ptr) {
        env_thread_ptr ptr = (pthread_t*)*p_ptr;
        *p_ptr = NULL;
        pthread_join(*(pthread_t*)ptr, NULL);
        free(ptr);
    }
    
}

void env_thread_sleep(__uint64 nano_seconds)
{
#if defined(OS_WINDOWS)
    nano_seconds /= 1000000ULL;
	if (nano_seconds < 1) nano_seconds = 1;
    Sleep(nano_seconds);
#else
    nano_seconds /= 1000ULL;
    if (nano_seconds < 1) nano_seconds = 1;
	usleep(nano_seconds);
#endif
}

__sint32 env_thread_mutex_init(env_thread_mutex_t *mutex)
{
    return pthread_mutex_init(mutex, NULL);
}

void env_thread_mutex_destroy(env_thread_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

void env_thread_mutex_lock(env_thread_mutex_t *mutex)
{
    pthread_mutex_lock(mutex);
}

void env_thread_mutex_unlock(env_thread_mutex_t *mutex)
{
    pthread_mutex_unlock(mutex);
}

__sint32 env_thread_cond_init(env_thread_cond_t *cond)
{
    return pthread_cond_init(cond, NULL);
}

void env_thread_cond_destroy(env_thread_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

void env_thread_cond_signal(env_thread_cond_t *cond)
{
    pthread_cond_signal(cond);
}

void env_thread_cond_broadcast(env_thread_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

void env_thread_cond_wait(env_thread_cond_t *cond, env_thread_mutex_t *mutex)
{
    pthread_cond_wait(cond, mutex);
}

__sint32 env_thread_cond_timedwait(env_thread_cond_t *cond, env_thread_mutex_t *mutex, __uint64 timeout)
{
    __sint32 r = 0;
    struct timespec ts;
    timeout += env_time();
    ts.tv_sec = timeout / NANO_SECONDS;
    ts.tv_nsec = timeout % NANO_SECONDS;
    r = pthread_cond_timedwait(cond, mutex, &ts);
    errno = r;
    return r == 0 ? 0 : r == ETIMEDOUT ? ENV_TIMEDOUT : -1;
}

static inline __sint32 env_mutex_init(env_mutex_t *mutex)
{
    __sint32 r;
    r = env_thread_mutex_init(mutex->mutex);
    if (r == 0){
        r = env_thread_cond_init(mutex->cond);
        if (r != 0){
            env_thread_mutex_destroy(mutex->mutex);
        }
    }
    return r;
}

static inline void env_mutex_uninit(env_mutex_t *mutex)
{
    if (NULL != (void*)mutex->cond){
        env_thread_cond_destroy(mutex->cond);
    }
    if (NULL != (void*)mutex->mutex){
        env_thread_mutex_destroy(mutex->mutex);
    }
}

env_mutex_t* env_mutex_create(void)
{
    env_mutex_t *mutex = (env_mutex_t*)malloc(sizeof(env_mutex_t));
    if (mutex == NULL){
        return NULL;
    }
    if (env_mutex_init(mutex) != 0){
        free(mutex);
        mutex = NULL;
    }
    return mutex;
}

void env_mutex_destroy(env_mutex_t **pp_mutex)
{
    if (pp_mutex && *pp_mutex){
        env_mutex_t *mutex = *pp_mutex;
        *pp_mutex = NULL;
        env_mutex_uninit(mutex);
        free(mutex);
    }
}

void env_mutex_lock(env_mutex_t *mutex)
{
    env_thread_mutex_lock(mutex->mutex);
}

void env_mutex_unlock(env_mutex_t *mutex)
{
    env_thread_mutex_unlock(mutex->mutex);
}

void env_mutex_signal(env_mutex_t *mutex)
{
    env_thread_cond_signal(mutex->cond);
}

void env_mutex_broadcast(env_mutex_t *mutex)
{
    env_thread_cond_broadcast(mutex->cond);
}

void env_mutex_wait(env_mutex_t *mutex)
{
    env_thread_cond_wait(mutex->cond, mutex->mutex);
}

__sint32 env_mutex_timedwait(env_mutex_t *mutex, __uint64 timeout)
{
    return env_thread_cond_timedwait(mutex->cond, mutex->mutex, timeout);
}


typedef struct env_pipe {
    __uint64 len;
    __uint64 leftover;
    __atombool writer;
    __atombool reader;

    char *buf;
    __atombool write_waiting;
    __atombool read_waiting;

    __atombool stopped;
    env_mutex_t mutex;
}env_pipe_t;


env_pipe_t* env_pipe_create(__uint64 len)
{
    env_pipe_t *pipe = (env_pipe_t *)malloc(sizeof(env_pipe_t));
    __pass(pipe != NULL);

   if ((len & (len - 1)) == 0){
        pipe->len = len;
    }else{
        pipe->len = 1U;
        do {
            pipe->len <<= 1;
        } while(len >>= 1);
    }

    // if (pipe->len < (1U << 16)){
    //     pipe->len = (1U << 16);
    // }

    pipe->leftover = pipe->len;

    pipe->buf = (char *)malloc(pipe->len);
    __pass(pipe->buf != NULL);

    __pass(env_mutex_init(&pipe->mutex) == 0);

    pipe->read_waiting = pipe->write_waiting = 0;
    pipe->reader = pipe->writer = 0;
    pipe->stopped = __false;

    return pipe;

Reset:
    env_mutex_uninit(&pipe->mutex);
    if (pipe->buf) free(pipe->buf);
    if (pipe) free(pipe);
    return NULL;
}

void env_pipe_destroy(env_pipe_t **pp_pipe)
{
    if (pp_pipe && *pp_pipe){
        env_pipe_t *pipe = *pp_pipe;
        *pp_pipe = NULL;
        env_pipe_clear(pipe);
        env_pipe_stop(pipe);
        env_mutex_uninit(&pipe->mutex);
        free(pipe->buf);
        free(pipe);
    }
}

static inline __uint64 pipe_atomic_write(env_pipe_t *pipe, __ptr data, __uint64 size) {

    if (pipe->buf == NULL || data == NULL || size == 0){
        return 0;
    }

    __uint64 writable = pipe->len - pipe->writer + pipe->reader;
    pipe->leftover = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );

    if ( writable == 0 ){
        return 0;
    }

    size = writable < size ? writable : size;

    if ( pipe->leftover >= size ){
        memcpy( pipe->buf + ( pipe->writer & ( pipe->len - 1 ) ), data, size);
    }else{
        memcpy( pipe->buf + ( pipe->writer & ( pipe->len - 1 ) ), data, pipe->leftover);
        memcpy( pipe->buf, (char*)data + pipe->leftover, size - pipe->leftover);
    }

    __atom_add(pipe->writer, size);

    return size;
}


static inline __uint64 pipe_atomic_read(env_pipe_t *pipe, __ptr buf, __uint64 size) {

    if (pipe->buf == NULL || buf == NULL || size == 0){
        return 0;
    }

    __uint64 readable = pipe->writer - pipe->reader;
    pipe->leftover = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );

    if ( readable == 0 ){
        return 0;
    }

    size = readable < size ? readable : size;

    if ( pipe->leftover >= size ){
        memcpy( buf, pipe->buf + ( pipe->reader & ( pipe->len - 1 ) ), size);
    }else{
        memcpy( buf, pipe->buf + ( pipe->reader & ( pipe->len - 1 ) ), pipe->leftover);
        memcpy( (char*)buf + pipe->leftover, pipe->buf, size - pipe->leftover);
    }

    __atom_add(pipe->reader, size);

    return size;
}

#include <stdio.h>
__uint64 env_pipe_write(env_pipe_t *pipe, __ptr data, __uint64 len)
{
    printf("env_pipe_write =========== enter\n");

    if (pipe->buf == NULL || data == NULL || len == 0){
        return 0;
    }

    if (__is_true(pipe->stopped)){
        return 0;
    }

    env_mutex_lock(&pipe->mutex);

    __uint64 ret = 0, pos = 0;
    while (pos < len ) {
        ret = pipe_atomic_write(pipe, (char*)data + pos, len - pos);
        pos += ret;
        if (pos != len){
            if (ret > 0 && pipe->read_waiting > 0){
                env_mutex_signal(&pipe->mutex);
            }
            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            }
            __atom_add(pipe->write_waiting, 1);
            printf("env_pipe_write wait %lu ==============------------- enter\n", pipe->read_waiting);
            env_mutex_wait(&pipe->mutex);
            printf("env_pipe_write wait %lu ==============------------- exit\n", pipe->read_waiting);
            __atom_sub(pipe->write_waiting, 1);
        }
    }

    if (pipe->read_waiting > 0){
        env_mutex_signal(&pipe->mutex);
    }
    env_mutex_unlock(&pipe->mutex);

    //__logd("env_pipe_write =========== exit\n");

    return pos;
}

__uint64 env_pipe_read(env_pipe_t *pipe, __ptr buf, __uint64 len)
{
    printf("env_pipe_read =========== enter\n");
    if (pipe->buf == NULL || buf == NULL || len == 0){
        return 0;
    }

    env_mutex_lock(&pipe->mutex);

    __uint64 pos = 0;
    for(__uint64 ret = 0; pos < len; ){
        ret = pipe_atomic_read(pipe, (char*)buf + pos, len - pos);
        pos += ret;
        if (pos != len){
            if (ret > 0 && pipe->write_waiting > 0){
                env_mutex_signal(&pipe->mutex);
            }
            if (__is_true(pipe->stopped)){
                env_mutex_unlock(&pipe->mutex);
                return pos;
            }
            __atom_add(pipe->read_waiting, 1);
            printf("env_pipe_read wait %lu  ============== enter\n", pipe->write_waiting);
            env_mutex_wait(&pipe->mutex);
            printf("env_pipe_read wait %lu ============== exit\n", pipe->write_waiting);
            __atom_sub(pipe->read_waiting, 1);
        }
    }

    if (pipe->write_waiting > 0){
        env_mutex_signal(&pipe->mutex);
    }
    env_mutex_unlock(&pipe->mutex);

    printf("env_pipe_read =========== exit\n");

    return pos;
}

__uint64 env_pipe_readable(env_pipe_t *pipe){
    return pipe->writer - pipe->reader;
}

__uint64 env_pipe_writable(env_pipe_t *pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

void env_pipe_stop(env_pipe_t *pipe){
    if (__set_true(pipe->stopped)){
        while (pipe->write_waiting > 0 || pipe->read_waiting > 0) {
            __logd("env_pipe_stop lock %lu    %lu ============== 1\n", pipe->write_waiting, pipe->read_waiting);
            env_mutex_lock(&pipe->mutex);
            __logd("env_pipe_stop lock %lu    %lu ============== 2\n", pipe->write_waiting, pipe->read_waiting);
            env_mutex_broadcast(&pipe->mutex);
            __logd("env_pipe_stop lock %lu    %lu ============== 3\n", pipe->write_waiting, pipe->read_waiting);
            env_mutex_timedwait(&pipe->mutex, 1000);
            __logd("env_pipe_stop lock %lu    %lu ============== 4\n", pipe->write_waiting, pipe->read_waiting);
            env_mutex_unlock(&pipe->mutex);
            __logd("env_pipe_stop lock %lu    %lu ============== 5\n", pipe->write_waiting, pipe->read_waiting);
            //env_thread_sleep(1000);
        }
    }
}

void env_pipe_clear(env_pipe_t *pipe){
    // env_atomic_store(&pipe->reader, 0);
    // env_atomic_store(&pipe->writer, 0);
    __atom_sub(pipe->reader, pipe->reader);
    __atom_sub(pipe->writer, pipe->writer);
}