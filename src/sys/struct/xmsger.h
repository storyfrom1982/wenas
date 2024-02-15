#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"

#include <sys/struct/xheap.h>
#include <sys/struct/xtree.h>
#include <sys/struct/xbuf.h>



enum {
    XMSG_PACK_BYE = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_ACK = 0x02,
    XMSG_PACK_PING = 0x04,
    XMSG_PACK_PONG = 0x08,
    XMSG_PACK_FINAL = 0x10
};


#define PACK_HEAD_SIZE              16
#define PACK_BODY_SIZE              1280 // 1024 + 256
// #define PACK_BODY_SIZE              8
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_WINDOW_RANGE           16

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;
typedef struct xchannellist* xchannellist_ptr;
typedef struct xmsglistener* xmsglistener_ptr;


typedef struct xmsghead {
    uint8_t type;
    uint8_t sn; //serial number 包序列号
    uint8_t ack; //ack 单个包的 ACK
    uint8_t acks; //acks 这个包和它之前的所有包的 ACK
    uint8_t resend, x, y, z; //这个包的重传计数
    uint32_t cid;
    uint16_t pack_range; // message fragment count 消息分包数量
    uint16_t pack_size; // message fragment length 这个分包的长度
}*xmsghead_ptr;

typedef struct xmsgpack {
    bool comfirmed;
    uint8_t resending;
    struct xheapnode ts;
    // uint32_t timestamp; //计算往返耗时
    xchannel_ptr channel;
    struct xmsghead head;
    uint8_t body[PACK_BODY_SIZE];
}*xmsgpack_ptr;

typedef struct xmsgpackbuf {
    uint8_t range;
    __atom_size upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgpackbuf_ptr;


typedef struct xmsg {
    xchannel_ptr channel;
    size_t size;
    char data[1];
}*xmsg_ptr;


typedef struct xmsgbuf {
    xmsg_ptr msg;
    uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgbuf_ptr;


struct xchannel {
    uint8_t key;
    uint32_t cid;
    uint32_t peer_cid;
    uint32_t peer_key;
    xchannel_ptr prev, next;
    int status;
    __xmutex_ptr mtx;
    // __atom_bool is_connected;
    // __atom_bool bye;
    __atom_bool breaker;
    __atom_bool connected;
    // __atom_bool termination;
    __atom_size tasklen;
    uint8_t delay;
    uint64_t timestamp;
    uint64_t update;
    xheap_ptr timer;
    struct xmsghead ack;
    xmsgbuf_ptr msgbuf;
    // xchannellist_ptr queue;
    struct __xipaddr addr;
    xmsgpackbuf_ptr sendbuf;
    xmsger_ptr msger;
};

//channellist
typedef struct xchannellist {
    __atom_size len;
    __atom_bool lock;
    struct xchannel head, end;
}*xchannellist_ptr;


struct xmsglistener {
    void *ctx;
    void (*onConnectionToPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onConnectionFromPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onDisconnection)(struct xmsglistener*, xchannel_ptr channel);
    void (*onReceiveMessage)(struct xmsglistener*, xchannel_ptr channel, xmsg_ptr msg);
    //没有数据接收或发送，通知用户监听网络接口，有数据可读时，通知messenger
    void (*onIdle)(struct xmsglistener*, xchannel_ptr channel);
    void (*onSendable)(struct xmsglistener*, xchannel_ptr channel);
};


typedef struct xmsgsocket {
    void *ctx;
    //不需要内部监听，监听网络接口由用户层的线程处理
    // void (*listening)(struct xmsgsocket *socket);
    size_t (*sendto)(struct xmsgsocket *socket, __xipaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct xmsgsocket *socket, __xipaddr_ptr addr, void *data, size_t size);
}*xmsgsocket_ptr;


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      10000000000ULL

struct xmsger {
    // 每次创建新的channel时加一
    uint32_t cid;
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __xmutex_ptr mtx;
    __atom_bool running;
    //socket可读或者有数据待发送时为true
    __atom_bool working;
    xmsglistener_ptr listener;
    xmsgsocket_ptr msgsock;
    xmaker_ptr mainloop_func;
    xtask_ptr mainloop_task;
    __atom_size tasklen;
    __atom_bool connection_buf_lock;
    xmsgpackbuf_ptr connection_buf;
    xbuf_ptr pipe;
    xchannellist_ptr sendqueue;
};


#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - 1 - (b)->wpos + (b)->rpos))

#define XMSG_KEY    'x'
#define XMSG_VAL    'X'

static inline void xmsger_enqueue_channel(xmsger_ptr msger, xchannel_ptr channel)
{
    channel->next = &msger->sendqueue->end;
    channel->prev = msger->sendqueue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    __atom_add(msger->sendqueue->len, 1);
}

static inline void xmsger_dequeue_channel(xmsger_ptr msger, xchannel_ptr channel)
{
    channel->prev->next = channel->next;
    channel->next->prev = channel->prev;
    __atom_sub(msger->sendqueue->len, 1);
}

static inline void xmsger_clear_channel(xmsger_ptr msger, xchannel_ptr channel)
{
    xmsger_dequeue_channel(msger, channel);
    if (channel->peer_cid != 0){
        xtree_take(msger->peers, &channel->cid, 4);
    }else {
        // 此时，还没有使用 cid 作为索引
        xtree_take(msger->peers, channel->addr.key, channel->addr.keylen);
    }
    
}

static inline void xchannel_push_task(xchannel_ptr channel, uint64_t len)
{
    __atom_add(channel->tasklen, len);
    __atom_add(channel->msger->tasklen, len);
}

static inline void xchannel_push(xchannel_ptr channel, xmsgpack_ptr unit)
{
    unit->channel = channel;
    unit->resending = 0;
    unit->comfirmed = false;

    // if (__is_true(channel->bye)){
    //     return;
    // }

    // while (__transbuf_writable(channel->sendbuf) == 0){
    //     // 不进行阻塞，添加续传逻辑
    //     // TODO 设置当前 channel 的等待写入标志
    //     ___lock lk = __ex_mutex_lock(channel->mtx);
    //     if (__is_true(channel->bye)){
    //         __ex_mutex_unlock(channel->mtx, lk);
    //         return;
    //     }
    //     __ex_mutex_wait(channel->mtx, lk);
    //     if (__is_true(channel->bye)){
    //         __ex_mutex_unlock(channel->mtx, lk);
    //         return;
    //     }
    //     __ex_mutex_unlock(channel->mtx, lk);
    // }

    // 再将 unit 放入缓冲区 
    unit->head.sn = channel->sendbuf->wpos;
    // 设置校验码
    unit->head.x = XMSG_VAL ^ channel->peer_key;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    __atom_add(channel->sendbuf->wpos, 1);

    // // 在主循环中加入 Idle 条件标量，直接检测 Idle 条件。
    // // 是否统计所有 channel 的待发送包数量？
    // if (__atom_add(channel->msger->tasklen, 1) == 1){
    //     ___lock lk = __ex_mutex_lock(channel->msger->mtx);
    //     __ex_mutex_notify(channel->msger->mtx);
    //     __ex_mutex_unlock(channel->msger->mtx, lk);
    // }
}

static inline void xchannel_pull(xchannel_ptr channel, xmsgpack_ptr ack)
{
    __xlogd("xchannel_pull >>>>------------> enter\n");
    __xlogd("xchannel_pull >>>>------------> range: %u sn: %u rpos: %u upos: %u wpos %u\n",
           ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 xmsgpack
    if (__transbuf_inuse(channel->sendbuf) > 0 && ((uint8_t)(ack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        __xlogd("xchannel_pull >>>>------------> in range\n");

        uint8_t index;
        xmsgpack_ptr pack;
        xheapnode_ptr timenode;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        if (ack->head.ack == ack->head.acks){
            __xlogd("xchannel_pull >>>>------------> in serial\n");
            do {
                //TODO 计算往返延时
                //TODO 统计丢包率
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                pack = channel->sendbuf->buf[index];

                // 数据已送达，从待发送数据中减掉这部分长度
                __atom_sub(channel->tasklen, pack->head.pack_size);
                __atom_sub(channel->msger->tasklen, pack->head.pack_size);
                __xlogd("xchannel_pull >>>>------------------------------------> channel len: %lu msger len %lu\n", channel->tasklen - 0, channel->msger->tasklen - 0);

                if (!pack->comfirmed){
                    // 移除定时器，设置确认状态
                    __xlogd("xchannel_pull >>>>------------> remove timer: %u\n", pack->head.sn);
                    assert(channel->timer->array[pack->ts.pos] != NULL);
                    pack->comfirmed = true;
                    timenode = xheap_remove(channel->timer, &pack->ts);
                    assert(timenode->value == pack);
                    __xlogd("xchannel_pull >>>>------------------------------------> timer count: %lu\n", channel->timer->pos);
                }else {
                    // 这里可以统计收到重复 ACK 的次数
                }

                // 释放内存
                free(pack);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                __atom_add(channel->sendbuf->rpos, 1);
                // TODO 通知 channel 可写
                // channel->msger->listener->onSendable();
                __xapi->mutex_notify(channel->mtx);

                // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
            } while (channel->sendbuf->rpos != ack->head.ack);

            channel->delay = 0;

        } else {

            // __logi("xchannel_pull recv interval ack: %u acks: %u", ack->head.ack, ack->head.acks);
            index = ack->head.ack & (PACK_WINDOW_RANGE - 1);
            pack = channel->sendbuf->buf[index];

            __xlogd("xchannel_pull >>>>------------------------> out serial: %u sn: %u rpos: %u upos: %u wpos %u\n",
                   ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

            // 检测此 SN 是否未确认
            if (pack && !pack->comfirmed){
                assert(channel->timer->array[pack->ts.pos] != NULL);
                // 移除定时器，设置确认状态
                pack->comfirmed = true;
                timenode = xheap_remove(channel->timer, &pack->ts);
                assert(timenode->value == pack);
                __xlogd("xchannel_pull >>>>------------------------------------> timer count: %lu\n", channel->timer->pos);
            }

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            if (++channel->delay > 2){
                index = channel->sendbuf->rpos;
                while (index != ack->head.sn) {
                    pack = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                    if (!pack->comfirmed && pack->resending == 0){
                        __xlogd("xchannel_pull >>>>------------------------------------> resend pack: %u\n", pack->head.sn);
                        if (channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, 
                            (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size) == PACK_HEAD_SIZE + pack->head.pack_size){
                            pack->resending++;
                        }
                    }
                    index++;
                }
            }
        }

    }else {

    }

    channel->update = __xapi->clock();
}

static inline int64_t xchannel_send(xchannel_ptr channel, xmsghead_ptr ack)
{
    // __xlogd("xchannel_send >>>>------------> enter\n");

    long result;
    xmsgpack_ptr pack;

    if (__transbuf_usable(channel->sendbuf) > 0){
        pack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
        if (ack != NULL){
            __xlogd("xchannel_send >>>>------------> ACK and MSG\n");
            pack->head.y = 1;
            pack->head.ack = ack->ack;
            pack->head.acks = ack->acks;
        }else {
            __xlogd("xchannel_send >>>>------------> MSG\n");
        }
        
        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            channel->sendbuf->upos++;
            pack->ts.key = __xapi->clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timer, &pack->ts);
            __xlogd("xchannel_send >>>>------------------------> channel timer count %lu\n", channel->timer->pos);
            pack->resending = 0;
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
        }

    }else if (ack != NULL){
        __xlogd("xchannel_send >>>>------------> ACK\n");
        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, (void*)ack, PACK_HEAD_SIZE);
        if (result == PACK_HEAD_SIZE){
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
        }
    }

    while (channel->timer->pos > 0 && (int64_t)(__heap_min(channel->timer)->key - __xapi->clock()) < 0){
        __xlogd("xchannel_send >>>>------------------------------------> pack time out resend %lu %lu\n", __heap_min(channel->timer)->key, __xapi->clock());
        pack = (xmsgpack_ptr)__heap_min(channel->timer)->value;
        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &pack->channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            xheap_pop(channel->timer);
            pack->ts.key = __xapi->clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timer, &pack->ts);
            __xlogd("xchannel_send >>>>------------------------> channel timer count %lu\n", channel->timer->pos);
            pack->resending ++;
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
            break;
        }
    }

    if (channel->timer->pos > 0){
        result = (int64_t)__heap_min(channel->timer)->key - __xapi->clock();
    }else {
        result = 0;
    }

    // __xlogd("xchannel_send >>>>------------> exit\n");

    return result;

}

static inline void xchannel_recv(xchannel_ptr channel, xmsgpack_ptr unit)
{
    __xlogd("xchannel_recv >>>>------------> enter\n");
    __xlogd("xchannel_recv >>>>------------> SN: %u rpos: %u wpos: %u\n",
           unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);

    if (unit->head.y == 1){
        __xlogd("xchannel_recv >>>>------------> MSG and ACK\n");
        // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
        xchannel_pull(channel, unit);
    }else {
        // 只有在接收到对端的消息时，才更新超时时间戳
        channel->update = __xapi->clock();
    }

    channel->ack = unit->head;
    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.pack_size = 0;
    uint16_t index = unit->head.sn & (PACK_WINDOW_RANGE - 1);

    // 如果收到连续的 PACK
    if (unit->head.sn == channel->msgbuf->wpos){

        __xlogd("xchannel_recv >>>>------------> serial\n");

        // 保存 PACK
        channel->msgbuf->buf[index] = unit;
        // 更新最大连续 SN
        channel->msgbuf->wpos++;

        // 收到连续的 ACK 就不会回复的单个 ACK 确认了        
        channel->ack.acks = channel->msgbuf->wpos;
        // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
        channel->ack.ack = channel->ack.acks;

        // 如果之前有为按顺序到达的 PACK 需要更新
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
            //TODO
            assert(__transbuf_writable(channel->msgbuf) > 0);
            channel->msgbuf->wpos++;
            // 这里需要更新将要回复的最大连续 ACK
            channel->ack.acks = channel->msgbuf->wpos;
            // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
            channel->ack.ack = channel->ack.acks;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->msgbuf->wpos - unit->head.sn) > (uint8_t)(unit->head.sn - channel->msgbuf->wpos)){

            __xlogd("xchannel_recv >>>>------------------------> early\n");

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack = unit->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.acks = channel->msgbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->msgbuf->wpos - 1;
            
            if (channel->msgbuf->buf[index] == NULL){
                // 这个 PACK 首次到达，保存 PACK
                channel->msgbuf->buf[index] = unit;
            }else {
                // 这个 PACK 重复到达，释放 PACK
                free(unit);
            }
            
        }else {

            __xlogd("xchannel_recv >>>>------------------------> again\n");
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            channel->ack.ack = channel->msgbuf->wpos - 1;
            channel->ack.acks = channel->ack.ack;
            // 释放 PACK
            free(unit);
        }
    }


    xchannel_send(channel, &channel->ack);


    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->pack_range = channel->msgbuf->buf[index]->head.pack_range;
            assert(channel->msgbuf->pack_range != 0 && channel->msgbuf->pack_range <= XMSG_PACK_RANGE);
            channel->msgbuf->msg = (xmsg_ptr)malloc(sizeof(struct xmsg) + (channel->msgbuf->pack_range * PACK_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }

        mcopy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.pack_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.pack_size;
        channel->msgbuf->pack_range--;
        if (channel->msgbuf->pack_range == 0){
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
            // __xlogd("xchannel_recv >>>>------------> %s\n", channel->msgbuf->msg->data);
            channel->msger->listener->onReceiveMessage(channel->msger->listener, channel, channel->msgbuf->msg);
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }

    __xlogd("xchannel_recv >>>>------------> exit\n");
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    channel->connected = false;
    // channel->bye = false;
    channel->breaker = false;
    channel->update = __xapi->clock();
    channel->delay = 0;
    channel->msger = msger;
    channel->addr = *addr;
    channel->mtx = __xapi->mutex_create();
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    channel->timer = xheap_create(PACK_WINDOW_RANGE);
    channel->peer_cid = 0;
    while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
        if (++msger->cid == 0){
            msger->cid = 1;
        }
    }
    channel->cid = msger->cid;
    channel->key = msger->cid % 255;
    xmsger_enqueue_channel(msger, channel);
    return channel;
}

static inline void xchannel_release(xchannel_ptr channel)
{
    __xlogd("xchannel_release enter\n");
    xmsger_dequeue_channel(channel->msger, channel);
    __xapi->mutex_free(channel->mtx);
    xheap_free(&channel->timer);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_release exit\n");
}

//提供给用户一个从Idle状态唤醒主循环的接口
static inline void xmsger_wake(xmsger_ptr mesger)
{
    __xlogd("xmsger_wake enter %p\n", mesger);
    __xapi->mutex_lock(mesger->mtx);
    __set_true(mesger->working);
    __xapi->mutex_notify(mesger->mtx);
    __xapi->mutex_unlock(mesger->mtx);
    __xlogd("xmsger_wake exit\n");
}

static inline xmsgpack_ptr make_pack(xchannel_ptr channel, uint8_t type)
{
    xmsgpack_ptr pack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    // 设置包类型
    pack->head.type = type;
    // 设置消息封包数量
    pack->head.pack_range = 1;
    // 设置对方 cid
    pack->head.cid = channel->peer_cid;
    // 设置校验码
    pack->head.x = XMSG_VAL ^ channel->peer_key;
    // 设置是否附带 ACK 标志
    pack->head.y = 0;
    return pack;
}

static inline void xmsger_loop(xmaker_ptr ctx)
{
    __xlogd("xmsger_loop enter\n");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000ULL;
    // transack_ptr ack;
    
    xmsgpack_ptr rpack = NULL;
    xheapnode_ptr timenode;
    xmsgpack_ptr sendunit = NULL;
    xchannel_ptr channel = NULL;
    xchannel_ptr next = NULL;

    xmsger_ptr msger = (xmsger_ptr)xline_find_ptr(ctx, "ctx");

    struct __xipaddr addr = __xapi->udp_make_ipaddr(NULL, 1234);

    while (__is_true(msger->running))
    {

        if (rpack == NULL){
            rpack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            __xcheck(rpack);
        }

        rpack->head.type = 0;
        rpack->head.pack_size = 0;
        result = msger->msgsock->recvfrom(msger->msgsock, &addr, &rpack->head, PACK_ONLINE_SIZE);

        if (result == (rpack->head.pack_size + PACK_HEAD_SIZE)){

            __xlogd("xmsger_loop recv ip: %u port: %u cid: %u msg: %d\n", addr.ip, addr.port, rpack->head.cid, rpack->head.type);

            channel = (xchannel_ptr)xtree_find(msger->peers, &rpack->head.cid, 4);

            if (channel){

                __xlogd("xmsger_loop fond channel\n");

                // 协议层验证
                if (rpack->head.x ^ channel->key == XMSG_VAL){

                    if (rpack->head.type == XMSG_PACK_MSG) {
                        __xlogd("xmsger_loop receive MSG\n");
                        rpack->head.cid = channel->peer_cid;
                        rpack->head.x = XMSG_VAL ^ channel->peer_key;
                        xchannel_recv(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_ACK) {
                        __xlogd("xmsger_loop receive ACK\n");
                        xchannel_pull(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_BYE){
                        __xlogd("xmsger_loop receive BYE\n");
                        // 判断是否为主动断开的一方
                        if (__is_true(channel->breaker)){
                            __xlogd("xmsger_loop is breaker\n");
                            // 主动方，回复 FINAL 释放连接
                            xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_FINAL);
                            xchannel_push(channel, spack);
                            xchannel_recv(channel, rpack);
                            // 释放连接，结束超时重传
                            msger->listener->onDisconnection(msger->listener, channel);

                        }else {
                            __xlogd("xmsger_loop not breaker\n");
                            // 被动方，收到 BEY，需要回一个 BEY，并且启动超时重传
                            xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_BYE);
                            // 启动超时重传
                            xchannel_push(channel, spack);
                            xchannel_recv(channel, rpack);

                        }

                    }else if (rpack->head.type == XMSG_PACK_FINAL){
                        __xlogd("xmsger_loop receive FINAL\n");
                        //被动方收到 FINAL，释放连接，结束超时重传。
                        msger->listener->onDisconnection(msger->listener, channel);

                    }else if (rpack->head.type == XMSG_PACK_PING){
                        __xlogd("xmsger_loop receive PING\n");
                        // 回复 PONG
                        xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PONG);
                        xchannel_push(channel, spack);
                        xchannel_recv(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_PONG){
                        __xlogd("xmsger_loop receive PONG\n");
                        rpack->head.cid = channel->peer_cid;
                        rpack->head.x = XMSG_VAL ^ channel->peer_key;
                        xchannel_recv(channel, rpack);
                    }

                }

            } else {

                __xlogd("xmsger_loop cannot fond channel\n");

                if (rpack->head.type == XMSG_PACK_PING){

                    __xlogd("xmsger_loop receive PING\n");

                    if (rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){
                        
                        struct xmaker parser = xline_parse((xline_ptr)rpack->body);
                        uint32_t peer_cid = xline_find_uint32(&parser, "cid");
                        uint64_t timestamp = xline_find_uint64(&parser, "time");

                        channel = (xchannel_ptr)xtree_find(msger->peers, addr.key, addr.keylen);

                        // 检查是否为建立同一次连接的重复的 PING
                        if (channel != NULL && channel->timestamp != timestamp){
                            __xlogd("xmsger_loop receive PING reconnecting\n");
                            // 不是重复的 PING
                            // 同一个地址，在建立第一次连接的过程中，又发起了第二次连接，所以要释放第一次连接的资源
                            xchannel_release(channel);
                            channel = NULL;
                        }

                        if (channel == NULL){
                            // 创建连接
                            channel = xchannel_create(msger, &addr);
                            channel->peer_cid = peer_cid;
                            channel->peer_key = peer_cid % 255;
                            channel->timestamp = timestamp;
                            __xlogd("xmsger_loop new connections channel: 0x%x ip: %u port: %u cid: %u time: %lu\n", channel, addr.ip, addr.port, peer_cid, timestamp);
                            // 上一次的连接已经被释放，xtree 直接覆盖原来的连接，替换当前的连接
                            xtree_save(msger->peers, addr.key, addr.keylen, channel);

                            xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PONG);
                            // // 第一次回复 PONG，cid 必须设置为 0
                            spack->head.cid = 0;
                            spack->head.x = XMSG_VAL ^ XMSG_KEY;
                            spack->head.y = 1;
                            struct xmaker kv;
                            xline_maker_setup(&kv, spack->body, PACK_BODY_SIZE);
                            xline_add_uint32(&kv, "cid", channel->cid);
                            spack->head.pack_size = kv.wpos + 9; //TODO
                            xchannel_push_task(channel, spack->head.pack_size);
                            xchannel_push(channel, spack);

                            xchannel_recv(channel, rpack);
                        }                        
                    }

                }else if (rpack->head.type == XMSG_PACK_PONG){
                    __xlogd("xmsger_loop receive PONG\n");

                    channel = (xchannel_ptr)xtree_take(msger->peers, addr.key, addr.keylen);
                    if (channel && rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){
                        xtree_save(msger->peers, &channel->cid, 4, channel);
                        struct xmaker parser = xline_parse((xline_ptr)rpack->body);
                        uint32_t cid = xline_find_uint32(&parser, "cid");                            
                        // 设置对端 cid 与 key
                        channel->peer_cid = cid;
                        channel->peer_key = cid % 255;
                        rpack->head.type = XMSG_PACK_ACK;
                        rpack->head.x = XMSG_VAL ^ XMSG_KEY;
                        xchannel_recv(channel, rpack);
                        if (__set_true(channel->connected)){
                            //这里是被动建立连接 onConnectionFromPeer
                            channel->msger->listener->onConnectionToPeer(channel->msger->listener, channel);
                        }
                    }else {
                        rpack->head.type = XMSG_PACK_ACK;
                        rpack->head.x = XMSG_VAL ^ XMSG_KEY;
                        msger->msgsock->sendto(msger->msgsock, &addr, (void*)&(rpack->head), PACK_HEAD_SIZE);
                    }

                }else if (rpack->head.type == XMSG_PACK_ACK){
                    __xlogd("xmsger_loop receive ACK\n");

                    channel = (xchannel_ptr)xtree_take(msger->peers, addr.key, addr.keylen);
                    if (channel && rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){
                        xtree_save(msger->peers, &channel->cid, 4, channel);
                        xchannel_pull(channel, rpack);
                        channel->connected = true;
                        //这里是被动建立连接 onConnectionFromPeer
                        channel->msger->listener->onConnectionFromPeer(channel->msger->listener, channel);
                    }

                }else if (rpack->head.type == XMSG_PACK_BYE){
                    __xlogd("xmsger_loop receive BYE\n");
                    // 主动方释放连接后，收到了被动方重传的 BEY
                    // 直接 sendto 回复 FINAL
                    rpack->head.type = XMSG_PACK_FINAL;
                    msger->msgsock->sendto(msger->msgsock, &addr, (void*)&(rpack->head), PACK_HEAD_SIZE);
                }
            }

            rpack = NULL;

        }else {

            //定时select，使用下一个重传定时器到期时间，如果定时器为空，最大10毫秒间隔。
            //主动创建连接，最多需要10毫秒。
            if (msger->tasklen == 0 && xbuf_readable(msger->pipe) == 0){
                if (__set_false(msger->working)){
                    msger->listener->onIdle(msger->listener, channel);
                }
                __xapi->mutex_lock(msger->mtx);
                __xapi->mutex_notify(msger->mtx);
                if (msger->tasklen == 0 && xbuf_readable(msger->pipe) == 0){
                    __xapi->mutex_timedwait(msger->mtx, timer);
                }
                __xapi->mutex_unlock(msger->mtx);
                timer = 1000000000ULL;
            }
        }


        if (xbuf_readable(msger->pipe) > 0){
            __xlogd("xmsger_loop create channel to peer\n");
            xmaker_ptr ctx = xbuf_hold_reader(msger->pipe);
            if (ctx){
                __xipaddr_ptr addr = (__xipaddr_ptr)xline_find_ptr(ctx, "addr");
                // TODO 对方应答后要设置 peer_cid 和 key；
                xchannel_ptr channel = xchannel_create(msger, addr);
                if (channel){
                    // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                    xtree_save(msger->peers, channel->addr.key, channel->addr.keylen, channel);
                    xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PING);
                    // 建立连接时，cid 必须是 0
                    spack->head.cid = 0;
                    // 这里是协议层验证
                    // TODO 需要更换一个密钥
                    spack->head.x = (XMSG_VAL ^ XMSG_KEY);
                    struct xmaker kv;
                    xline_maker_setup(&kv, spack->body, PACK_BODY_SIZE);
                    xline_add_uint32(&kv, "cid", channel->cid);
                    xline_add_uint64(&kv, "time", __xapi->clock());
                    spack->head.pack_size = kv.wpos + 9;
                    xchannel_push_task(channel, spack->head.pack_size);
                    xchannel_push(channel, spack);
                }
                xbuf_update_reader(msger->pipe);
            }
        }

        channel = msger->sendqueue->head.next;

        while (channel != &msger->sendqueue->end)
        {
            next = channel->next;

            timeout = xchannel_send(channel, NULL);
            if (timeout > 0 && timer > timeout){
                timer = timeout;
            }

            if (__xapi->clock() - channel->update > (1000000000ULL * 30)){
                // 十秒内没有收发过数据，释放连建
                xmsger_clear_channel(msger, channel);
                channel->msger->listener->onDisconnection(channel->msger->listener, channel);
            }

            channel = next;
        }
    }


    if (rpack != NULL){
        free(rpack);
    }

    __xapi->udp_clear_ipaddr(addr);

    __xlogd("xmsger_loop exit\n");
}

static inline xmsger_ptr xmsger_create(xmsgsocket_ptr msgsock, xmsglistener_ptr listener)
{
    __xlogd("xmsger_create enter\n");

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));

    __xlogd("xmsger_create 1\n");
    msger->cid = __xapi->clock() % UINT16_MAX;
    msger->running = true;
    msger->msgsock = msgsock;
    msger->listener = listener;
    msger->tasklen = 0;
    __xlogd("xmsger_create 2\n");
    msger->mtx = __xapi->mutex_create();
    __xlogd("xmsger_create 3\n");
    msger->peers = xtree_create();
    
    msger->sendqueue = (xchannellist_ptr)malloc(sizeof(struct xchannellist));
    msger->sendqueue->len = 0;
    msger->sendqueue->lock = false;
    msger->sendqueue->head.prev = NULL;
    msger->sendqueue->end.next = NULL;
    msger->sendqueue->head.next = &msger->sendqueue->end;
    msger->sendqueue->end.prev = &msger->sendqueue->head;

    msger->pipe = xbuf_create(2);
    __xcheck(msger->pipe != NULL);

    __xlogd("xmsger_create exit\n");

    return msger;

Clean:

    return NULL;
}

static inline void xmsger_run(xmsger_ptr messenger)
{
    messenger->mainloop_task = xtask_run(xmsger_loop, messenger);
}

static void free_channel(void *val)
{
    __xlogd(">>>>------------> free channel 0x%x\n", val);
    xchannel_release((xchannel_ptr)val);
}

static inline void xmsger_free(xmsger_ptr *pptr)
{
    __xlogd("xmsger_free enter\n");
    if (pptr && *pptr){        
        xmsger_ptr msger = *pptr;
        *pptr = NULL;
        __set_false(msger->running);
        __xapi->mutex_broadcast(msger->mtx);
        xtask_free(&msger->mainloop_task);
        __xlogd("xmsger_free 1\n");
        // xline_maker_clear(msger->mainloop_func);
        __xlogd("xmsger_free 2\n");
        xtree_clear(msger->peers, free_channel);
        __xlogd("xmsger_free 3\n");
        xtree_free(&msger->peers);
        __xlogd("xmsger_free 4\n");
        __xapi->mutex_free(msger->mtx);
        __xlogd("xmsger_free 5\n");
        if (msger->sendqueue){
            __xlogd("xmsger_free 6\n");
            free(msger->sendqueue);
        }
        __xlogd("xmsger_free 7\n");
        xbuf_free(&msger->pipe);
        free(msger);
    }
    __xlogd("xmsger_free exit\n");
}

static inline void xmsger_wait(xmsger_ptr messenger)
{
    __xapi->mutex_lock(messenger->mtx);
    __xapi->mutex_wait(messenger->mtx);
    __xapi->mutex_unlock(messenger->mtx);
}

//TODO 增加一个用户上下文参数，断开连接时，可以直接先释放channel，然后在回调中给出用户上下文，通知用户回收相关资源。
//TODO 增加一个加密验证数据，messenger 只负责将消息发送到对端，连接能否建立，取决于对端验证结果。
static inline int xmsger_connect(xmsger_ptr messenger, __xipaddr_ptr addr)
{
    __xlogd("xmsger_connect enter\n");
    xmaker_ptr ctx = xbuf_hold_writer(messenger->pipe);
    __xlogd("xmsger_connect ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    if (ctx == NULL){
        return -1;
    }
    xline_add_ptr(ctx, "addr", addr);
    // const char *key = "PING";
    // size_t len = slength(key);
    // char output[len];
    // for (size_t i = 0; i < len; ++i) {
    //     output[i] = key[i] ^ 'k';
    // }
    // __xlogd("str len %lu\n", len);
    // xline_add_text(ctx, "msg", output, len);
    xbuf_update_writer(messenger->pipe);
    __xlogd("xmsger_connect exit\n");
    return 0;
}

static inline void xmsger_disconnect(xmsger_ptr mtp, xchannel_ptr channel)
{
    __xlogd("xmsger_disconnect enter");
    xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    unit->head.type = XMSG_PACK_BYE;
    unit->head.pack_size = 0;
    unit->head.pack_range = 1;
    //主动发送 BEY，要设置 channel 主动状态。
    __set_true(channel->breaker);
    //向对方发送 BEY，对方回应后，再移除 channel。
    // __set_true(channel->bye);
    //主动断开的一端，发送第一个 BEY，并且启动超时重传。
    xchannel_push(channel, unit);
    __xlogd("xmsger_disconnect exit");
}

//ping 需要发送加密验证，如果此时链接已经断开，ping 消息可以进行重连。
static inline void xmsger_ping(xchannel_ptr channel)
{
    __xlogd("xmsger_ping enter");
    if (__xapi->clock() - channel->update >= 1000000000ULL * 10){
        xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
        unit->head.type = XMSG_PACK_PING;
        unit->head.pack_range = 1;
        struct xmaker kv;
        xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
        xline_add_text(&kv, "msg", "PING", slength("PING"));
        unit->head.pack_size = kv.wpos;
        xchannel_push(channel, unit);
    }
    __xlogd("xmsger_ping exit");
}

static inline void xmsger_send(xmsger_ptr mtp, xchannel_ptr channel, void *data, size_t size)
{
    // 非阻塞发送，当网络可发送时，通知用户层。
    char *msg_data;
    size_t msg_size;
    size_t msg_count = size / (XMSG_MAXIMUM_LENGTH);
    size_t last_msg_size = size - (msg_count * XMSG_MAXIMUM_LENGTH);

    uint16_t unit_count;
    uint16_t last_unit_size;
    uint16_t last_unit_id;

    for (int x = 0; x < msg_count; ++x){
        msg_size = XMSG_MAXIMUM_LENGTH;
        msg_data = ((char*)data) + x * XMSG_MAXIMUM_LENGTH;
        for (size_t y = 0; y < XMSG_PACK_RANGE; y++)
        {
            // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
            mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
            // unit->head.type = XMSG_PACK_MSG;
            unit->head.pack_size = PACK_BODY_SIZE;
            unit->head.pack_range = XMSG_PACK_RANGE - y;
            xchannel_push(channel, unit);
        }
    }

    if (last_msg_size){
        msg_data = ((char*)data) + (size - last_msg_size);
        msg_size = last_msg_size;

        unit_count = msg_size / PACK_BODY_SIZE;
        last_unit_size = msg_size - unit_count * PACK_BODY_SIZE;
        last_unit_id = 0;

        if (last_unit_size > 0){
            last_unit_id = 1;
        }

        for (size_t y = 0; y < unit_count; y++){
            // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
            mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
            // unit->head.type = XMSG_PACK_MSG;
            unit->head.pack_size = PACK_BODY_SIZE;
            unit->head.pack_range = (unit_count + last_unit_id) - y;
            xchannel_push(channel, unit);
        }

        if (last_unit_id){
            // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
            mcopy(unit->body, msg_data + (unit_count * PACK_BODY_SIZE), last_unit_size);
            // unit->head.type = XMSG_PACK_MSG;
            unit->head.pack_size = last_unit_size;
            unit->head.pack_range = last_unit_id;
            xchannel_push(channel, unit);
        }
    }
}

//make_channel(addr, 验证数据) //channel 在外部线程创建
//clear_channel(ChannelPtr) //在外部线程销毁


#endif //__XMSGER_H__