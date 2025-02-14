#ifndef __XPIPE_H__
#define __XPIPE_H__


#include "xline.h"


typedef struct xpipe {
    uint64_t len;
    // uint64_t leftover; 曾经为了避免使用一个临时变量处理折行，在这里添加了一个长周期的变量，却忽略了读写线程共用这个变量，导致 xcopy 越界的 BUG
    __atom_size writer;
    __atom_size reader;
    __atom_bool breaker;
    __atom_bool rlock;
    __atom_bool wlock;
    __atom_bool rblock;
    __atom_bool wblock;
    __xmutex_ptr mutex;
    char name[16];
    uint8_t *buf;
}*xpipe_ptr;

static inline void __xpipe_notify(xpipe_ptr pipe){
    __xapi->mutex_notify(pipe->mutex);
}

static inline uint64_t __xpipe_write(xpipe_ptr pipe, void *data, uint64_t len)
{
    if(!__atom_try_lock(pipe->wlock)){
        return 0;
    }

    uint64_t writable = pipe->len - pipe->writer + pipe->reader;

    if (writable > len){
        writable = len;
    }

    if (writable > 0){

        uint64_t leftover = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );
        if (leftover >= writable){
            xcopy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((uint8_t*)data), writable);
        }else {
            xcopy(pipe->buf + (pipe->writer & (pipe->len - 1)), ((uint8_t*)data), leftover);
            xcopy(pipe->buf, ((uint8_t*)data) + leftover, writable - leftover);
        }
        __atom_add(pipe->writer, writable);
    }

    __atom_unlock(pipe->wlock);

    return writable;
}

static inline uint64_t xpipe_write(xpipe_ptr pipe, void *data, uint64_t len)
{
    uint64_t pos = 0;

    if (pipe == NULL || data == NULL){
        return pos;
    }

    // __xlogd("%s xpipe_write >>>>>--------------> enter\n", pipe->name);

    while (!pipe->breaker && pos < len) {
        pos += __xpipe_write(pipe, (uint8_t*)data + pos, len - pos);
        if (pos != len){
            __xapi->mutex_lock(pipe->mutex);
            if (!pipe->breaker){
                __xapi->mutex_notify(pipe->mutex);
                __atom_lock(pipe->wblock);
                if ((pipe->len - pipe->writer + pipe->reader) == 0){
                    __xapi->mutex_wait(pipe->mutex);
                }
                __atom_unlock(pipe->wblock);
            }
            __xapi->mutex_unlock(pipe->mutex);
        }else {
            if (__atom_try_lock(pipe->rblock)){
                __atom_unlock(pipe->rblock);
            }else {
                __xapi->mutex_lock(pipe->mutex);
                __xapi->mutex_notify(pipe->mutex);
                __xapi->mutex_unlock(pipe->mutex);
            }
            break;
        }
    }

    // __xlogd("%s xpipe_write >>>>>--------------> exit\n", pipe->name);

    return pos;
}

static inline uint64_t __xpipe_read(xpipe_ptr pipe, void *buf, uint64_t len)
{
    if(!__atom_try_lock(pipe->rlock)){
        return 0;
    }

    uint64_t readable = pipe->writer - pipe->reader;

    if (readable > len){
        readable = len;
    }

    if (readable > 0){

        uint64_t leftover = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );
        if (leftover >= readable){
            xcopy(((uint8_t*)buf), pipe->buf + (pipe->reader & (pipe->len - 1)), readable);
        }else {
            xcopy(((uint8_t*)buf), pipe->buf + (pipe->reader & (pipe->len - 1)), leftover);
            xcopy(((uint8_t*)buf) + leftover, pipe->buf, readable - leftover);
        }
        __atom_add(pipe->reader, readable);
    }

    __atom_unlock(pipe->rlock);

    return readable;
}

static inline uint64_t xpipe_read(xpipe_ptr pipe, void *buf, uint64_t len)
{
    uint64_t pos = 0;

    // 如果日志线程触发了下面的日志写入，会造成原本负责读管道的线程向管道写数据，如果这时管道的空间不够大，就会造成日志线程阻塞
    if (pipe == NULL || buf == NULL){
        return pos;
    }

    // __xlogd("%s xpipe_read >>>>>--------------> enter\n", pipe->name);

    // 长度大于 0 才能进入读循环
    while (pos < len) {
        pos += __xpipe_read(pipe, (uint8_t*)buf + pos, len - pos);
        if (pos != len){
            __xapi->mutex_lock(pipe->mutex);
            if (!pipe->breaker){
                // 在阻塞之前，唤醒一次写线程，因为这时缓冲区可能已经有写入的空间
                __xapi->mutex_notify(pipe->mutex);
                // 多线程读写时，后到这里的线程会在这里循环尝试，直到获取锁为止
                __atom_lock(pipe->rblock);
                if ((pipe->writer - pipe->reader) == 0){
                    // 缓冲区尚未更新，如果有数据写入之后，写线程一定会尝试获取 rblock，所以确保写线程一定能获知这里已经进入阻塞状态
                    __xapi->mutex_wait(pipe->mutex);
                }
                __atom_unlock(pipe->rblock);
            }else {
                // 只有缓冲区没有数据可读时，breaker 才会导致读线程返回
                __xapi->mutex_unlock(pipe->mutex);
                return pos;
            }
            __xapi->mutex_unlock(pipe->mutex);
        }else {
            if (__atom_try_lock(pipe->wblock)){
                // 拿到 wblock，证明写线程还没有进入阻塞状态
                // 此刻，缓冲区已经更新，写线程在阻塞之前，必然要拿锁之后再检测是否可写，这时缓冲区已经更新，写线程不会进入阻塞状态
                __atom_unlock(pipe->wblock);
            }else {
                __xapi->mutex_lock(pipe->mutex);
                // 拿到互斥锁，证明写线程已经进入阻塞状态，所以此时一定能唤醒写线程
                __xapi->mutex_notify(pipe->mutex);
                __xapi->mutex_unlock(pipe->mutex);
            }
            break;
        }
    }

    // __xlogd("%s xpipe_read >>>>>--------------> exit\n", pipe->name);

    return pos;
}

static inline uint64_t xpipe_readable(xpipe_ptr pipe){
    return pipe->writer - pipe->reader;
}

static inline uint64_t xpipe_writable(xpipe_ptr pipe){
    return pipe->len - pipe->writer + pipe->reader;
}

static inline void xpipe_clear(xpipe_ptr pipe){
    __atom_sub(pipe->reader, pipe->reader);
    __atom_sub(pipe->writer, pipe->writer);
}

static inline void xpipe_break(xpipe_ptr pipe){
    __xapi->mutex_lock(pipe->mutex);
    __set_true(pipe->breaker);
    __xapi->mutex_broadcast(pipe->mutex);
    __xapi->mutex_unlock(pipe->mutex);
}

static inline void xpipe_free(xpipe_ptr *pptr)
{
    if (pptr && *pptr){
        xpipe_ptr pipe = *pptr;
        *pptr = NULL;
        if (pipe){
            xpipe_clear(pipe);
            xpipe_break(pipe);
            // 确保读写线程退出才能释放管道，否则释放互斥锁可能崩溃
            __xapi->mutex_free(pipe->mutex);
            if (pipe->buf){
                xfree(pipe->buf);
            }
            xfree(pipe);
        }
    }
}


static inline xpipe_ptr xpipe_create(uint64_t len, const char *name)
{
    xpipe_ptr pipe = (xpipe_ptr)xalloc(sizeof(struct xpipe));
    __xcheck(pipe == NULL);

    if (xlen(name) > 15){
        xcopy(pipe->name, name, 15);
        pipe->name[15] = '\0';
    }else {
        xcopy(pipe->name, name, xlen(name));
        pipe->name[xlen(name)] = '\0';
    }

   if ((len & (len - 1)) == 0){
        pipe->len = len;
    }else{
        pipe->len = 1U;
        do {
            pipe->len <<= 1;
        } while(len >>= 1);
    }

    pipe->buf = (uint8_t*)xalloc(pipe->len);
    __xcheck(pipe->buf == NULL);

    pipe->mutex = __xapi->mutex_create();
    __xcheck(pipe->mutex == NULL);

    pipe->breaker = 0;
    pipe->reader = pipe->writer = 0;
    pipe->rlock = pipe->wlock = 0;
    pipe->rblock = pipe->wblock = 0;

    return pipe;

XClean:

    xpipe_free(&pipe);

    return NULL;
}


#endif //__XPIPE_H__
