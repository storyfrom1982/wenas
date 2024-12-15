#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xline.h"


#define XMSG_FLAG_NONE          0x00
#define XMSG_FLAG_CONNECT       0x01
#define XMSG_FLAG_DISCONNECT    0x02


#define XLMSG_DEFAULT_SIZE      (1024 * 16)


typedef struct xmsg {
    __atom_size ref;
    uint32_t flag;
    uint32_t streamid;
    size_t sendpos, recvpos, size, range;
    struct xchannel *channel;
    struct xchannel_ctx *ctx;
    struct xmsg *prev, *next;
    uint8_t *key;
    uint8_t body[1];
}*xmsg_ptr;



static inline xmsg_ptr xmsg_create(size_t size)
{
    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg) + size);
    if (msg){
        msg->ref = 1;
        msg->size = size;
        msg->sendpos = 0;
    }
    return msg;
}

static inline xmsg_ptr xmsg_maker()
{
    return xmsg_create(XLMSG_DEFAULT_SIZE);
}

static inline void xmsg_ref(xmsg_ptr msg)
{
    if (msg){
        __atom_add(msg->ref, 1);
    }
}

static inline void xmsg_free(xmsg_ptr msg)
{
    if (msg && (__atom_sub(msg->ref, 1) == 0)){
        free(msg);
    }
}

static inline uint64_t xmsg_hold_kv(xmsg_ptr msg, const char *key)
{
    msg->key = msg->body + msg->sendpos;
    msg->key[0] = slength(key) + 1;
    mcopy(msg->key + 1, key, msg->key[0]);
    msg->sendpos += 1 + msg->key[0];
    uint64_t pos = msg->sendpos;
    msg->sendpos += XLINE_SIZE;
    return pos;
}

static inline void xmsg_save_kv(xmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->sendpos - pos - XLINE_SIZE;
    *((xdata_ptr)(msg->body + pos)) = __n2xl(len, XLINE_TYPE_XLKV);
}

static inline uint64_t xmsg_hold_list(xmsg_ptr msg, const char *key)
{
    return xmsg_hold_kv(msg, key);
}

static inline void xmsg_save_list(xmsg_ptr msg, uint64_t pos)
{
    uint64_t len = msg->sendpos - pos - XLINE_SIZE;
    *((xdata_ptr)(msg->body + pos)) = __n2xl(len, XLINE_TYPE_LIST);
}

static inline uint64_t xmsg_list_hold_kv(xmsg_ptr msg)
{
    msg->recvpos = msg->sendpos;
    msg->sendpos += XLINE_SIZE;
    return msg->recvpos;
}

static inline void xmsg_list_save_xlkv(xmsg_ptr msg, uint64_t pos)
{
    return xmsg_save_kv(msg, pos);
}

static inline uint64_t xlmsg_add_obj(xmsg_ptr msg, const char *key, uint8_t keylen, const void *val, size_t size, uint8_t flag)
{
    // 检查是否有足够的空间写入
    if ((int64_t)(msg->size - msg->sendpos) < (keylen + XLINE_SIZE + size)){
        return EENDED;
    }
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    msg->key = msg->body + msg->sendpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的结尾
    msg->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(msg->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(msg->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(msg->key + msg->key[0]) = '\0';
    // 因为我们的 key 除了字符串头部的一个字节的长度，还有一个字节的结束符'\0'，所以这里再加 1
    msg->sendpos += (msg->key[0] + 1);
    // 把 val 的长度和类型写入头部的 9 个字节
    *((xdata_ptr)(msg->body + msg->sendpos)) = __n2xl(size, flag);
    msg->sendpos += XLINE_SIZE;
    // 复制 val 到接下来的内存地址
    mcopy(msg->body + msg->sendpos, val, size);
    // 先把 val 的 size 加入到 wpos
    msg->sendpos += size;
    // 返回当前写入长度
    return msg->sendpos;
}

static inline uint64_t xlmsg_add_word(xmsg_ptr msg, const char *key, const char *word)
{
    return xlmsg_add_obj(msg, key, slength(key), word, slength(word) + 1, XLINE_TYPE_STR);
}

static inline uint64_t xlmsg_add_str(xmsg_ptr msg, const char *key, const char *str, uint64_t size)
{
    return xlmsg_add_obj(msg, key, slength(key), str, size, XLINE_TYPE_STR);
}

static inline uint64_t xlmsg_add_bin(xmsg_ptr msg, const char *key, const void *val, uint64_t size)
{
    return xlmsg_add_obj(msg, key, slength(key), val, size, XLINE_TYPE_BIN);
}

static inline uint64_t xlmsg_add_kv(xmsg_ptr msg, const char *key, xdata_ptr xl, size_t size)
{
    return xlmsg_add_obj(msg, key, slength(key), xl->b, size, XLINE_TYPE_XLKV);
}

static inline uint64_t xlmsg_add_list(xmsg_ptr msg, const char *key, xdata_ptr xl, size_t size)
{
    return xlmsg_add_obj(msg, key, slength(key), xl->b, size, XLINE_TYPE_LIST);
}

static inline uint64_t xlmsg_append_number(xmsg_ptr msg, const char *key, uint8_t keylen, struct xl val)
{
    // 检查是否有足够的空间写入
    if ((int64_t)(msg->size - msg->sendpos) < (keylen + XLINE_SIZE)){
        return EENDED;
    }
    // key 本身的长度不能超过 253，因为 uint8_t 只能存储 0-255 之间的数字
    // 253 + 一个字节的头 + 一个字节的尾，正好等于 255
    if (keylen > 253){
        keylen = 253;
    }
    msg->key = msg->body + msg->sendpos;
    // 这里加上了一个字节的长度，因为我们要在最后补上一个‘\0’作为字符串的结尾
    msg->key[0] = keylen + 1;
    // 这里从 key[1] 开始复制
    mcopy(msg->key + 1, key, keylen);
    // 因为 key 有可能被截断过，所以复制的最后一个字节有可能不是‘\0’
    // 本来应该是 *(msg->key + 1 + keylen) = '\0';
    // 但是 key[0] = keylen + 1; 所以这里没问题
    *(msg->key + msg->key[0]) = '\0';
    // 因为我们的 key 除了字符串头部的一个字节的长度，还有一个字节的结束符'\0'，所以这里再加 1
    msg->sendpos += (msg->key[0] + 1);
    *((xdata_ptr)(msg->body + msg->sendpos)) = val;
    msg->sendpos += XLINE_SIZE;
    // 返回当前写入长度
    return msg->sendpos;
}

static inline uint64_t xlmsg_add_integer(xmsg_ptr msg, const char *key, int64_t i64)
{
    return xlmsg_append_number(msg, key, slength(key), __i2xl(i64));
}

static inline uint64_t xl_add_number(xmsg_ptr msg, const char *key, uint64_t u64)
{
    return xlmsg_append_number(msg, key, slength(key), __u2xl(u64));
}

static inline uint64_t xl_add_float(xmsg_ptr msg, const char *key, double f64)
{
    return xlmsg_append_number(msg, key, slength(key), __f2xl(f64));
}

static inline uint64_t xl_add_ptr(xmsg_ptr msg, const char *key, void *p)
{
    uint64_t n = (uint64_t)(p);
    return xlmsg_append_number(msg, key, slength(key), __u2xl(n));
}

static inline void xlmsg_parser(xmsg_ptr msg)
{
    msg->recvpos = 0;
}

static inline xdata_ptr xlmsg_find(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = NULL;
    uint64_t rpos = msg->recvpos;

    while (msg->recvpos < msg->sendpos) {
        msg->key = msg->body + msg->recvpos;
        msg->recvpos += (msg->key[0] + 1);
        val = (xdata_ptr)(msg->body + msg->recvpos);
        msg->recvpos += __sizeof_line(val);
        if (slength(key) + 1 == msg->key[0]
            && mcompare(key, msg->key + 1, msg->key[0]) == 0){
            msg->key++;
            return val;
        }
    }

    msg->recvpos = 0;

    while (msg->recvpos < rpos) {
        msg->key = msg->body + msg->recvpos;
        msg->recvpos += (msg->key[0] + 1);
        val = (xdata_ptr)(msg->body + msg->recvpos);
        msg->recvpos += __sizeof_line(val);
        if (slength(key) + 1 == msg->key[0]
            && mcompare(key, msg->key + 1, msg->key[0]) == 0){
            msg->key++;
            return val;
        }
    }

    return val;
}

static inline int64_t xlmsg_find_integer(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = xlmsg_find(msg, key);
    if (val){
        return __xl2i(val);
    }
    return EENDED;
}

static inline uint64_t xlmsg_find_number(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = xlmsg_find(msg, key);
    if (val){
        return __xl2u(val);
    }
    return EENDED;
}

static inline double xlmsg_find_float(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = xlmsg_find(msg, key);
    if (val){
        return __xl2f(val);
    }
    return (double)EENDED;
}

static inline const char* xlmsg_find_word(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = xlmsg_find(msg, key);
    if (val){
        return (const char*)__xl2o(val);
    }
    return NULL;
}

static inline void* xlmsg_find_ptr(xmsg_ptr msg, const char *key)
{
    xdata_ptr val = xlmsg_find(msg, key);
    if (val){
        return (void *)(__xl2u(val));
    }
    return NULL;
}

static inline uint64_t xlmsg_list_append(xmsg_ptr msg, xdata_ptr ptr)
{
    msg->recvpos = __sizeof_line(ptr);
    if ((int64_t)(msg->size - msg->sendpos) < msg->recvpos){
        return EENDED;
    }
    mcopy(msg->body + msg->sendpos, ptr->b, msg->recvpos);
    msg->sendpos += msg->recvpos;
    return msg->sendpos;
}

static inline xdata_ptr xlmsg_list_next(xmsg_ptr msg)
{
    if (msg->recvpos < msg->sendpos){
        xdata_ptr ptr = (xdata_ptr)(msg->body + msg->recvpos);
        msg->recvpos += __sizeof_line(ptr);
        return ptr;
    }
    return NULL;
}



#endif