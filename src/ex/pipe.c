#include "ex/ex.h"


#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <stdlib.h>
#include <string.h>

#include "sys/struct/xline.h"


typedef struct xpipe {

    char *buf;
    uint64_t len;
    uint64_t leftover;

    __atom_size writer;
    __atom_size reader;
    __atom_bool breaking;
    __xmutex_ptr mutex;

}*xpipe_ptr;


xpipe_ptr xpipe_create(uint64_t len)
{
    xpipe_ptr pipe = (xpipe_ptr)malloc(sizeof(struct xpipe));
    __xbreak(pipe);

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
    __xbreak(pipe->buf);

    pipe->mutex = __xapi->mutex_create();
    __xbreak(pipe->mutex);

    pipe->reader = pipe->writer = 0;
    pipe->breaking = false;

    return pipe;

Clean:

    xpipe_free(&pipe);

    return NULL;
}

void xpipe_free(xpipe_ptr *pptr)
{
    if (pptr && *pptr){
        xpipe_ptr pipe = *pptr;
        *pptr = NULL;
        if (pipe){
            xpipe_clear(pipe);
            if (pipe->mutex){
                xpipe_break(pipe);
                // 断开两次，确保没有其他线程阻塞在管道上
                xpipe_break(pipe);
                __xapi->mutex_free(pipe->mutex);
            }
        }
        if (pipe->buf){
            free(pipe->buf);
        }
        free(pipe);
    }
}


static inline uint64_t __pipe_write(xpipe_ptr pipe, void *data, uint64_t len)
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
        __atom_add(pipe->writer, writable);
        __xapi->mutex_notify(pipe->mutex);
    }

    return writable;
}

uint64_t xpipe_write(xpipe_ptr pipe, void *data, uint64_t len)
{
    uint64_t pos = 0;

    if (pipe == NULL || data == NULL){
        return pos;
    }

    while (__is_false(pipe->breaking) && pos < len) {

        pos += __pipe_write(pipe, data + pos, len - pos);
        if (pos != len){
            __xapi->mutex_lock(pipe->mutex);
            // __pipe_write 中的唤醒通知没有锁保护，不能确保在有可读空间的同时唤醒读取线程
            // 所以在阻塞之前，唤醒一次读线程
            __xapi->mutex_notify(pipe->mutex);
            // printf("xpipe_write waiting --------------- enter     len: %lu pos: %lu\n", len, pos);
            __xapi->mutex_wait(pipe->mutex);
            // printf("xpipe_write waiting --------------- exit\n");
            __xapi->mutex_unlock(pipe->mutex);
        }
    }

    return pos;
}

static inline uint64_t __pipe_read(xpipe_ptr pipe, void *buf, uint64_t len)
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
        __atom_add(pipe->reader, readable);
        __xapi->mutex_notify(pipe->mutex);
    }

    return readable;
}

uint64_t xpipe_read(xpipe_ptr pipe, void *buf, uint64_t len)
{
    uint64_t pos = 0;

    // 如果日志线程触发了下面的日志写入，会造成原本负责读管道的线程向管道写数据，如果这时管道的空间不够大，就会造成日志线程阻塞
    if (pipe == NULL || buf == NULL){
        return pos;
    }

    // 长度大于 0 才能进入读循环
    while (pos < len) {

        pos += __pipe_read(pipe, buf + pos, len - pos);

        if (pos != len){
            __xapi->mutex_lock(pipe->mutex);
            // __pipe_read 中的唤醒通知没有锁保护，不能确保在有可写空间的同时唤醒写入线程
            // 所以在阻塞之前，唤醒一次写线程
            __xapi->mutex_notify(pipe->mutex);
            if (__is_false(pipe->breaking)){
                // printf("xpipe_read waiting +++++++++++++++++++++++++++++++ enter     len: %lu pos: %lu\n", len, pos);
                __xapi->mutex_wait(pipe->mutex);
                // printf("xpipe_read waiting +++++++++++++++++++++++++++++++ exit\n");
            }
            __xapi->mutex_unlock(pipe->mutex);
            if (__is_true(pipe->breaking)){
                break;
            }
        }
    }

    return pos;
}

uint64_t xpipe_readable(xpipe_ptr pipe){
    return pipe->writer - pipe->reader;
}

uint64_t xpipe_writable(xpipe_ptr pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

void xpipe_clear(xpipe_ptr pipe){
    __atom_sub(pipe->reader, pipe->reader);
    __atom_sub(pipe->writer, pipe->writer);
}

void xpipe_break(xpipe_ptr pipe){
    __xapi->mutex_lock(pipe->mutex);
    __set_true(pipe->breaking);
    __xapi->mutex_broadcast(pipe->mutex);
    __xapi->mutex_unlock(pipe->mutex);
}



struct xbuf {

    uint64_t len;
    __atom_size writer;
    __atom_size reader;
    // __atom_size holder;
    __atom_bool breaking;
    __xmutex_ptr mutex;
    xmaker_ptr buf;
};

xbuf_ptr xbuf_create(uint64_t len)
{
    xbuf_ptr pipe = (xbuf_ptr)malloc(sizeof(struct xbuf));
    __xbreak(pipe);

   if ((len & (len - 1)) == 0){
        pipe->len = len;
    }else{
        pipe->len = 1U;
        do {
            pipe->len <<= 1;
        } while(len >>= 1);
    }

    __xlogi("pipe len %lu\n", pipe->len);

    // 一维指针 (struct xmaker*) 可以像使用 xmaker 类型的数组那样，进行下标操作，每个下标指向的是一个 xmaker 类型的结构体
    // 二维指针 (struct xmaker**) 也可以像使用数组那样，进行下标操作，但是每个下标指向的是 xmaker 类型的结构体指针 (xmaker*)
    pipe->buf = (xmaker_ptr)calloc(pipe->len, sizeof(struct xmaker));
    // pipe->buf = (xline_maker_ptr*)calloc(pipe->len, sizeof(xline_maker_ptr*));
    __xbreak(pipe->buf);

    for (int i = 0; i < pipe->len; ++i){
        // 如果使用二级指针，需要给每个指针分配实际的内存地址
        // pipe->buf[i] = calloc(1, sizeof(struct xmaker));
        xline_maker_create(&pipe->buf[i], NULL, 2);
        __xlogi("xbuf_create ============================== xmaker.addr: 0x%X\n", pipe->buf[i].addr);
    }

    pipe->mutex = __xapi->mutex_create();
    __xbreak(pipe->mutex);

    pipe->reader = pipe->writer = 0;
    pipe->breaking = false;

    return pipe;

Clean:

    xbuf_free(&pipe);

    return NULL;
}

void xbuf_free(xbuf_ptr *pptr)
{
    if (pptr && *pptr){
        xbuf_ptr pipe = *pptr;
        *pptr = NULL;
        if (pipe){
            xbuf_clear(pipe);
            if (pipe->mutex){
                xbuf_break(pipe);
                __xlogd("xbuf_free ============================== 1\n");
                __xapi->mutex_free(pipe->mutex);
            }
        }
        if (pipe->buf){
            __xlogd("xbuf_free ==============================\n");
            for (int i = 0; i < pipe->len; ++i){
                // 如果使用二级指针，这里需要释放内存
                __xlogd("xbuf_free ============================== xmaker.addr: 0x%X\n", pipe->buf[i].addr);
                xline_maker_free(&pipe->buf[i]);
                // free(pipe->buf[i]);
            }
            free(pipe->buf);
        }
        free(pipe);
    }
}

uint64_t xbuf_readable(xbuf_ptr buf)
{
    return buf->writer - buf->reader;
}

uint64_t xbuf_writable(xbuf_ptr buf)
{
    return buf->len - buf->writer + buf->reader;
}

void xbuf_clear(xbuf_ptr buf)
{
    __atom_sub(buf->reader, buf->reader);
    __atom_sub(buf->writer, buf->writer);
}

void xbuf_break(xbuf_ptr buf)
{
    __xapi->mutex_lock(buf->mutex);
    __set_true(buf->breaking);
    __xapi->mutex_broadcast(buf->mutex);
    __xapi->mutex_unlock(buf->mutex);
}

xmaker_ptr xbuf_hold_writer(xbuf_ptr buf)
{
    while ((buf->len - buf->writer + buf->reader) == 0){
        // 只有需要阻塞时才检查 breaking 状态
        if (__is_true(buf->breaking)){
            return NULL;
        }
        __xapi->mutex_lock(buf->mutex);
        // 写入线程不能确保被 xbuf_update_writer 唤醒
        // 因为 xbuf_update_writer 的唤醒没有加锁
        __xapi->mutex_notify(buf->mutex);
        __xapi->mutex_wait(buf->mutex);
        __xapi->mutex_unlock(buf->mutex);
        if (__is_true(buf->breaking)){
            return NULL;
        }        
    }
    // 重置 xline maker，用户才能重新写入数据
    // update reader 时重置一次就可以了，所以这里不需要重置了
    // xline_maker_reset(pipe->buf[(pipe->writer & (pipe->len - 1))]);
    // 返回给用户一个可写缓冲区，用户持有这个缓冲区，直接写入数据
    return &buf->buf[(buf->writer & (buf->len - 1))];
}

void xbuf_update_writer(xbuf_ptr buf)
{
    // 用户完成一次写入，增加一个可读区域
    __atom_add(buf->writer, 1);
    __xapi->mutex_notify(buf->mutex);
}

xmaker_ptr xbuf_hold_reader(xbuf_ptr buf)
{
    while ((buf->writer - buf->reader) == 0){
        // 只有需要阻塞时才检查 breaking 状态
        if (__is_true(buf->breaking)){
            return NULL;
        }
        __xapi->mutex_lock(buf->mutex);
        // 写入线程不能确保被 xbuf_update_reader 唤醒
        // 因为 xbuf_update_reader 的唤醒没有加锁
        __xapi->mutex_notify(buf->mutex);
        __xapi->mutex_wait(buf->mutex);
        __xapi->mutex_unlock(buf->mutex);
        if (__is_true(buf->breaking)){
            return NULL;
        }        
    }
    // 用户持有一个可读缓冲区，直接读取数据
    return &buf->buf[(buf->reader & (buf->len - 1))];
}

void xbuf_update_reader(xbuf_ptr buf)
{
    // 重置 xline maker，用户才能重新写入数据
    xline_maker_reset(&buf->buf[(buf->reader & (buf->len - 1))]);
    // 完成一次读取，增加一个可写区域
    __atom_add(buf->reader, 1);
    // 执行一次唤醒
    __xapi->mutex_notify(buf->mutex);
}