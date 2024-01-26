#include "ex/ex.h"


#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <stdlib.h>
#include <string.h>

#include "task.h"


typedef struct ex_pipe {

    char *buf;
    uint64_t len;
    uint64_t leftover;

    ___atom_bool writer;
    ___atom_bool reader;
    ___atom_bool breaking;
    __ex_lock_ptr mutex;

}__ex_pipe;


__ex_pipe* __ex_pipe_create(uint64_t len)
{
    __ex_pipe *pipe = (__ex_pipe *)malloc(sizeof(__ex_pipe));
    __ex_check(pipe);

   if ((len & (len - 1)) == 0){
        pipe->len = len;
    }else{
        pipe->len = 1U;
        do {
            pipe->len <<= 1;
        } while(len >>= 1);
    }

    pipe->leftover = pipe->len;

    pipe->buf = (char *)malloc(pipe->len);
    __ex_check(pipe->buf);

    pipe->mutex = __ex_lock_create();
    __ex_check(pipe->mutex);

    pipe->reader = pipe->writer = 0;
    pipe->breaking = false;

    return pipe;

Clean:

    __ex_pipe_destroy(&pipe);

    return NULL;
}

void __ex_pipe_destroy(__ex_pipe **pptr)
{
    if (pptr && *pptr){
        __ex_pipe *pipe = *pptr;
        *pptr = NULL;
        if (pipe){
            __ex_pipe_clear(pipe);
            if (pipe->mutex){
                __ex_pipe_break(pipe);
                __ex_lock_destroy(pipe->mutex);
            }
        }
        if (pipe->buf){
            free(pipe->buf);
        }
        free(pipe);
    }
}


static inline uint64_t pipe_write(__ex_pipe *pipe, __ptr data, uint64_t len)
{
    uint64_t  writable = pipe->len - pipe->writer + pipe->reader;

    if (writable > len){
        writable = len;
    }

    if (writable > 0){

        pipe->leftover = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );
        if (pipe->leftover >= writable){
            memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data), writable);
        }else {
            memcpy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((char*)data), pipe->leftover);
            memcpy(pipe->buf, ((char*)data) + pipe->leftover, writable - pipe->leftover);
        }
        ___atom_add(&pipe->writer, writable);
        __ex_notify(pipe->mutex);
    }

    return writable;
}

uint64_t __ex_pipe_write(__ex_pipe *pipe, __ptr data, uint64_t len)
{
    uint64_t pos = 0;

    // __ex_check(pipe->buf && data);
    if (pipe == NULL || data == NULL){
        return pos;
    }

    while (___is_false(&pipe->breaking) && pos < len) {

        pos += pipe_write(pipe, data + pos, len - pos);
        if (pos != len){
            ___lock lk = __ex_lock(pipe->mutex);
            // pipe_write 中的唤醒通知没有锁保护，不能确保在有可读空间的同时唤醒读取线程
            // 所以在阻塞之前，唤醒一次读线程
            __ex_notify(pipe->mutex);
            // printf("__ex_pipe_write waiting --------------- enter     len: %lu pos: %lu\n", len, pos);
            __ex_wait(pipe->mutex, lk);
            // printf("__ex_pipe_write waiting --------------- exit\n");
            __ex_unlock(pipe->mutex, lk);
        }
    }

// Clean:    

    return pos;
}

static inline uint64_t pipe_read(__ex_pipe *pipe, __ptr buf, uint64_t len)
{
    uint64_t readable = pipe->writer - pipe->reader;

    if (readable > len){
        readable = len;
    }

    if (readable > 0){

        pipe->leftover = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );
        if (pipe->leftover >= readable){
            memcpy(((char*)buf), pipe->buf + (pipe->reader & (pipe->len - 1)), readable);
        }else {
            memcpy(((char*)buf), pipe->buf + (pipe->reader & (pipe->len - 1)), pipe->leftover);
            memcpy(((char*)buf) + pipe->leftover, pipe->buf, readable - pipe->leftover);
        }
        ___atom_add(&pipe->reader, readable);
        __ex_notify(pipe->mutex);
    }

    return readable;
}

uint64_t __ex_pipe_read(__ex_pipe *pipe, __ptr buf, uint64_t len)
{
    uint64_t pos = 0;

    // 如果日志线程触发了下面的日志写入，会造成原本负责读管道的线程向管道写数据，如果这时管道的空间不够大，就会造成日志线程阻塞
    // __ex_check(pipe->buf && buf);
    if (pipe == NULL || buf == NULL){
        return pos;
    }

    // 长度大于 0 才能进入读循环
    while (pos < len) {

        pos += pipe_read(pipe, buf + pos, len - pos);

        if (pos != len){
            ___lock lk = __ex_lock(pipe->mutex);
            // pipe_read 中的唤醒通知没有锁保护，不能确保在有可写空间的同时唤醒写入线程
            // 所以在阻塞之前，唤醒一次写线程
            __ex_notify(pipe->mutex);
            if (___is_false(&pipe->breaking)){
                // printf("__ex_pipe_read waiting +++++++++++++++++++++++++++++++ enter     len: %lu pos: %lu\n", len, pos);
                __ex_wait(pipe->mutex, lk);
                // printf("__ex_pipe_read waiting +++++++++++++++++++++++++++++++ exit\n");
            }
            __ex_unlock(pipe->mutex, lk);
            if (___is_true(&pipe->breaking)){
                break;
            }            
        }
    }

// Clean:

    return pos;
}

uint64_t __ex_pipe_readable(__ex_pipe *pipe){
    return pipe->writer - pipe->reader;
}

uint64_t __ex_pipe_writable(__ex_pipe *pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

void __ex_pipe_clear(__ex_pipe *pipe){
    ___atom_sub(&pipe->reader, pipe->reader);
    ___atom_sub(&pipe->writer, pipe->writer);
}

void __ex_pipe_break(__ex_pipe *pipe){
    ___lock lk = __ex_lock(pipe->mutex);
    ___set_true(&pipe->breaking);
    __ex_broadcast(pipe->mutex);
    __ex_unlock(pipe->mutex, lk);
}
