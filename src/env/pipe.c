#include "env/env.h"


#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "task.h"


typedef struct env_pipe {
    char *buf;
    uint64_t len;
    uint64_t leftover;

    ___atom_bool writer;
    ___atom_bool reader;

    ___atom_bool write_waiting;
    ___atom_bool read_waiting;

    ___atom_bool stopped;

//    pthread_cond_t cond[1];
//    pthread_mutex_t mutex[1];
    ___mutex_ptr mutex;
}env_pipe_t;


env_pipe_t* env_pipe_create(uint64_t len)
{
    int ret;

    env_pipe_t *pipe = (env_pipe_t *)malloc(sizeof(env_pipe_t));
    assert(pipe);

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
    assert(pipe->buf);

    pipe->mutex = ___mutex_create();
    assert(pipe->mutex);
//    ret = pthread_mutex_init(pipe->mutex, NULL);
//    assert(ret == 0);
//    ret = pthread_cond_init(pipe->cond, NULL);
//    assert(ret == 0);

    pipe->read_waiting = pipe->write_waiting = 0;
    pipe->reader = pipe->writer = 0;
    pipe->stopped = false;

    return pipe;
}

void env_pipe_destroy(env_pipe_t **pptr)
{
    if (pptr && *pptr){
        env_pipe_t *pipe = *pptr;
        *pptr = NULL;
        env_pipe_clear(pipe);
        env_pipe_stop(pipe);
        ___mutex_release(pipe->mutex);
//        pthread_cond_destroy(pipe->cond);
//        pthread_mutex_destroy(pipe->mutex);
        free(pipe->buf);
        free(pipe);
    }
}

uint64_t env_pipe_write(env_pipe_t *pipe, __ptr data, uint64_t len)
{
    if (pipe->buf == NULL || data == NULL || len == 0){
        return 0;
    }

    if (___is_true(&pipe->stopped)){
        return 0;
    }

    ___lock lk = ___mutex_lock(pipe->mutex);

    uint64_t writable, pos = 0;

    while (pos < len) {

        writable = pipe->len - pipe->writer + pipe->reader;
        if (writable > len - pos){
            writable = len - pos;
        }

        if (writable > 0){

            pipe->leftover = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );
            if (pipe->leftover >= writable){
                memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data) + pos, writable);
            }else {
                memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data) + pos, pipe->leftover);
                memcpy(pipe->buf, ((char*)data) + (pos + pipe->leftover), writable - pipe->leftover);
            }
            ___atom_add(&pipe->writer, writable);
            pos += writable;

            if (pipe->read_waiting > 0){
                ___mutex_notify(pipe->mutex);
            }

        }else {

            if (___is_true(&pipe->stopped)){
                ___mutex_unlock(pipe->mutex, lk);
                return pos;
            }

            ___atom_add(&pipe->write_waiting, 1);
            ___mutex_wait(pipe->mutex, lk);
            ___atom_sub(&pipe->write_waiting, 1);

            if (___is_true(&pipe->stopped)){
                ___mutex_unlock(pipe->mutex, lk);
                return pos;
            }  
        }
    }

    if (pipe->read_waiting > 0){
        ___mutex_notify(pipe->mutex);
    }
    ___mutex_unlock(pipe->mutex, lk);

    return pos;
}

uint64_t env_pipe_read(env_pipe_t *pipe, __ptr buf, uint64_t len)
{
    if (pipe->buf == NULL || buf == NULL || len == 0){
        return 0;
    }

    ___lock lk = ___mutex_lock(pipe->mutex);

    uint64_t readable, pos = 0;

    while (pos < len) {

        readable = pipe->writer - pipe->reader;
        if (readable > len - pos){
            readable = len - pos;
        }

        if (readable > 0){

            pipe->leftover = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );
            if (pipe->leftover >= readable){
                memcpy(((char*)buf) + pos, pipe->buf + (pipe->reader & (pipe->len - 1)), readable);
            }else {
                memcpy(((char*)buf) + pos, pipe->buf + (pipe->reader & (pipe->len - 1)), pipe->leftover);
                memcpy(((char*)buf) + (pos + pipe->leftover), pipe->buf, readable - pipe->leftover);
            }
            ___atom_add(&pipe->reader, readable);
            pos += readable;

            if (pipe->write_waiting > 0){
                ___mutex_notify(pipe->mutex);
            }

        }else {

            if (___is_true(&pipe->stopped)){
                ___mutex_unlock(pipe->mutex, lk);
                return pos;
            }
            
            ___atom_add(&pipe->read_waiting, 1);
            ___mutex_wait(pipe->mutex, lk);
            ___atom_sub(&pipe->read_waiting, 1);

            if (___is_true(&pipe->stopped)){
                ___mutex_unlock(pipe->mutex, lk);
                return pos;
            } 
        }
    }

    if (pipe->write_waiting > 0){
        ___mutex_notify(pipe->mutex);
    }
    ___mutex_unlock(pipe->mutex, lk);

    return pos;
}

uint64_t env_pipe_readable(env_pipe_t *pipe){
    return pipe->writer - pipe->reader;
}

uint64_t env_pipe_writable(env_pipe_t *pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

void env_pipe_stop(env_pipe_t *pipe){
    if (___set_true(&pipe->stopped)){
        while (pipe->write_waiting > 0 || pipe->read_waiting > 0) {
            ___lock lk = ___mutex_lock(pipe->mutex);
            ___mutex_broadcast(pipe->mutex);
            ___mutex_unlock(pipe->mutex, lk);
        }
    }
}

void env_pipe_clear(env_pipe_t *pipe){
    ___atom_sub(&pipe->reader, pipe->reader);
    ___atom_sub(&pipe->writer, pipe->writer);
}
