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
// #define PACK_BODY_SIZE              1280 // 1024 + 256
#define PACK_BODY_SIZE              32
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
    uint64_t timestamp; //计算往返耗时
    xchannel_ptr channel;
    struct xmsgpack *prev, *next;
    struct xmsghead head;
    uint8_t body[PACK_BODY_SIZE];
}*xmsgpack_ptr;

struct xpacklist {
    uint8_t len;
    struct xmsgpack head, end;
};

typedef struct xmsgpackbuf {
    uint8_t range;
    __atom_size upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgpackbuf_ptr;

enum {
    XMSG_DGRAM = 0,
    XMSG_STREAM
};

typedef struct xmsg {
    uint64_t type; //XMSG_DGRAM or XMSG_STREAM
    xchannel_ptr channel; //如果 channel 是 NULL，这个 msg 是一个创建连接的 PING
    size_t wpos, rpos, len, range;
    void *addr;
    uint8_t data[1];
}*xmsg_ptr;


typedef struct xmsgbuf {
    // struct xmsg msg;
    uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgbuf_ptr;


typedef struct file_ctx {
    xchannel_ptr channel;
    size_t pos, len;
    __xfile_ptr fp;
}file_ctx_t;


struct xchannel {
    uint8_t key;
    uint32_t cid;
    uint32_t peer_cid;
    uint32_t peer_key;
    xchannel_ptr prev, next;
    __atom_bool sending;
    __atom_bool breaker;
    __atom_bool connected;
    // __atom_bool termination;
    __atom_size pos, len;
    uint8_t delay;
    uint64_t timestamp;
    uint64_t update;
    xheap_ptr timer; //可以使用队列做定时，因为新入队的 pack 时间戳都比较前面的大，所以队首的时间戳最小
    struct xmsghead ack;
    xmsgbuf_ptr msgbuf;
    // xchannellist_ptr queue;
    struct __xipaddr addr;
    xmsgpackbuf_ptr sendbuf;
    xmsger_ptr msger;
    xpipe_ptr msgqueue;
    struct xpacklist timedlist;
    xmsg_ptr msg;
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head, end;
}*xchannellist_ptr;


struct xmsglistener {
    void *ctx;
    void (*onConnectionToPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onConnectionFromPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onDisconnection)(struct xmsglistener*, xchannel_ptr channel);
    void (*onMessageFromPeer)(struct xmsglistener*, xchannel_ptr channel, xmsg_ptr msg);
    void (*onMessageToPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg);
};


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      10000000000ULL

struct xmsger {
    int sock;
    __atom_size pos, len;
    // 每次创建新的channel时加一
    uint32_t cid;
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __xmutex_ptr mtx;
    __atom_bool running;
    //socket可读或者有数据待发送时为true
    __atom_bool working;
    __atom_bool readable;
    xmsglistener_ptr listener;
    // xmsgsocket_ptr msgsock;
    // xtask_ptr mainloop_task;
    // __atom_size tasklen;
    // __atom_bool connection_buf_lock;
    // xmsgpackbuf_ptr connection_buf;
    xpipe_ptr mpipe, spipe, rpipe;
    __xprocess_ptr mpid, rpid, spid;
    struct xchannellist squeue, timed_queue;
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

static inline void xmsger_enqueue_channel(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    queue->len ++;
    channel->sending = true;
}

static inline void xmsger_dequeue_channel(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->prev->next = channel->next;
    channel->next->prev = channel->prev;
    queue->len --;
    channel->sending = false;
}

static inline void xchannel_pause(xchannel_ptr channel)
{
    xmsger_dequeue_channel(&channel->msger->squeue, channel);
    xmsger_enqueue_channel(&channel->msger->timed_queue, channel);
}

static inline void xchannel_resume(xchannel_ptr channel)
{
    xmsger_dequeue_channel(&channel->msger->timed_queue, channel);
    xmsger_enqueue_channel(&channel->msger->squeue, channel);
}

static inline void xchannel_push(xchannel_ptr channel, xmsgpack_ptr unit)
{
    unit->channel = channel;
    unit->resending = 0;
    unit->comfirmed = false;
    // 再将 unit 放入缓冲区 
    unit->head.sn = channel->sendbuf->wpos;
    // 设置校验码
    unit->head.x = XMSG_VAL ^ channel->peer_key;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    __atom_add(channel->sendbuf->wpos, 1);
}

//xchannel_update_buf
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
                // TODO 这里为什么要报错？
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                pack = channel->sendbuf->buf[index];

                // 数据已送达，从待发送数据中减掉这部分长度
                __atom_add(channel->pos, pack->head.pack_size);
                __atom_add(channel->msger->pos, pack->head.pack_size);
                if (channel->msg != NULL){
                    __xlogd("xchannel_pull >>>>------------> msg.rpos: %lu msg.len: %lu pack size: %lu\n", channel->msg->rpos, channel->msg->len, pack->head.pack_size);
                    channel->msg->rpos += pack->head.pack_size;
                    if (channel->msg->rpos == channel->msg->len){
                        //TODO 通知上层，消息发送完成
                        channel->msger->listener->onMessageToPeer(channel->msger->listener, channel, channel->msg->addr);
                        free(channel->msg);
                        if (xpipe_readable(channel->msgqueue) > 0){
                            __xlogd("xchannel_pull >>>>------------> next msg\n");
                            xpipe_read(channel->msgqueue, &channel->msg, sizeof(xmsg_ptr));
                            xpipe_write(channel->msger->spipe, &channel->msg, sizeof(void*));
                        }else {
                            __xlogd("xchannel_pull >>>>------------> msg receive finished\n");
                            channel->msg = NULL;
                            xchannel_pause(channel);
                        }
                        // 写线程可以一次性（buflen - 1）个 pack
                    }else if(channel->msg->wpos < channel->msg->len && __transbuf_readable(channel->sendbuf) < (channel->sendbuf->range >> 1)){
                        xpipe_write(channel->msger->spipe, &channel->msg, sizeof(void*));
                    }
                }
                __xlogd("xchannel_pull >>>>------------------------------------> channel len: %lu msger len %lu\n", channel->len - 0, channel->msger->len - 0);

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
                        if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, 
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

static inline void xchannel_free_msg(xmsg_ptr msg)
{
    free(msg);
}

static inline xmsgpack_ptr make_pack(xchannel_ptr channel, uint8_t type)
{
    xmsgpack_ptr pack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    // 设置包类型
    pack->head.type = type;
    // 设置消息封包数量
    pack->head.pack_range = 0;
    // 设置对方 cid
    pack->head.cid = channel->peer_cid;
    // 设置校验码
    pack->head.x = XMSG_VAL ^ channel->peer_key;
    // 设置是否附带 ACK 标志
    pack->head.y = 0;
    return pack;
}

static inline void xchannel_send_msg(xchannel_ptr channel, xmsg_ptr msg)
{
    __xlogd("xchannel_send_msg data enter\n");
    while (msg->wpos < msg->len)
    {
        if (__transbuf_writable(channel->sendbuf) == 0){
            break;
        }
        xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
        if (msg->len - msg->wpos < PACK_BODY_SIZE){
            unit->head.pack_size = msg->len - msg->wpos;
        }else{
            unit->head.pack_size = PACK_BODY_SIZE;
        }
        unit->head.pack_range = msg->range;
        mcopy(unit->body, msg->addr + msg->wpos, unit->head.pack_size);
        xchannel_push(channel, unit);
        msg->wpos += unit->head.pack_size;
        msg->range --;
        __xlogd("xchannel_send_msg data -------------------------------------------------------------- len: %lu wpos: %lu range %u\n", msg->len, msg->wpos, msg->range);
    }
    __xlogd("xchannel_send_msg data exit -------------------------------------------------------------- len: %lu wpos: %lu range %u\n", msg->len, msg->wpos, msg->range);
}

extern int64_t xchannel_send(xchannel_ptr channel, xmsghead_ptr ack);

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

        if (channel->msgbuf->buf[index]->head.pack_range > 0){
            if (channel->msg == NULL){
                // 收到消息的第一个包，创建 msg，记录范围
                channel->msgbuf->pack_range = channel->msgbuf->buf[index]->head.pack_range;
                assert(channel->msgbuf->pack_range != 0 && channel->msgbuf->pack_range <= XMSG_PACK_RANGE);
                channel->msg = (xmsg_ptr)malloc(sizeof(struct xmsg) + (channel->msgbuf->pack_range * PACK_BODY_SIZE));
                channel->msg->channel = channel;
                channel->msg->wpos = 0;
            }
            __xlogd("xchannel_recv >>>>--------------------------------------------------------------------> pos %u range %u\n", channel->msg->wpos, channel->msgbuf->buf[index]->head.pack_range);
            mcopy(channel->msg->data + channel->msg->wpos, 
                channel->msgbuf->buf[index]->body, 
                channel->msgbuf->buf[index]->head.pack_size);
            channel->msg->wpos += channel->msgbuf->buf[index]->head.pack_size;
            channel->msgbuf->pack_range--;
            if (channel->msgbuf->pack_range == 0){
                channel->msger->listener->onMessageFromPeer(channel->msger->listener, channel, channel->msg);
                channel->msg = NULL;
            }
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
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    channel->timer = xheap_create(PACK_WINDOW_RANGE);
    channel->msgqueue = xpipe_create(8 * sizeof(void*));
    channel->peer_cid = 0;
    while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
        if (++msger->cid == 0){
            msger->cid = 1;
        }
    }
    channel->cid = msger->cid;
    channel->key = msger->cid % 255;

    // 设置 0 的操作，都由 calloc 做了
    channel->timedlist.len = 0;
    channel->timedlist.head.prev = NULL;
    channel->timedlist.end.next = NULL;
    channel->timedlist.head.next = &channel->timedlist.end;
    channel->timedlist.end.prev = &channel->timedlist.head;
    channel->timedlist.end.timestamp = UINT64_MAX;

    // TODO 是否要在创建连接时就加入发送队列，因为这时连接中还没有消息要发送
    xmsger_enqueue_channel(&msger->squeue, channel);
    return channel;
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    __atom_sub(channel->msger->len, channel->len - channel->pos);
    if (channel->sending){
        xmsger_dequeue_channel(&channel->msger->squeue, channel);
    }else {
        xmsger_dequeue_channel(&channel->msger->timed_queue, channel);
    }
    xheap_free(&channel->timer);
    xpipe_free(&channel->msgqueue);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_free exit\n");
}

// static inline void xmsger_run(xmsger_ptr messenger)
// {
//     messenger->mainloop_task = xtask_run(xmsger_loop, messenger);
// }

static void free_channel(void *val)
{
    __xlogd(">>>>------------> free channel 0x%x\n", val);
    xchannel_free((xchannel_ptr)val);
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
    __xlogd("xmsger_ping exit");
}

extern bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, const char *addr, uint16_t port);
extern xmsger_ptr xmsger_create(xmsglistener_ptr listener);
extern void xmsger_free(xmsger_ptr *pptr);

// static inline void xmsger_send(xmsger_ptr mtp, xchannel_ptr channel, void *data, size_t size)
// {
//     // 非阻塞发送，当网络可发送时，通知用户层。
//     char *msg_data;
//     size_t msg_size;
//     size_t msg_count = size / (XMSG_MAXIMUM_LENGTH);
//     size_t last_msg_size = size - (msg_count * XMSG_MAXIMUM_LENGTH);

//     uint16_t unit_count;
//     uint16_t last_unit_size;
//     uint16_t last_unit_id;

//     for (int x = 0; x < msg_count; ++x){
//         msg_size = XMSG_MAXIMUM_LENGTH;
//         msg_data = ((char*)data) + x * XMSG_MAXIMUM_LENGTH;
//         for (size_t y = 0; y < XMSG_PACK_RANGE; y++)
//         {
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = PACK_BODY_SIZE;
//             unit->head.pack_range = XMSG_PACK_RANGE - y;
//             xchannel_push(channel, unit);
//         }
//     }

//     if (last_msg_size){
//         msg_data = ((char*)data) + (size - last_msg_size);
//         msg_size = last_msg_size;

//         unit_count = msg_size / PACK_BODY_SIZE;
//         last_unit_size = msg_size - unit_count * PACK_BODY_SIZE;
//         last_unit_id = 0;

//         if (last_unit_size > 0){
//             last_unit_id = 1;
//         }

//         for (size_t y = 0; y < unit_count; y++){
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = PACK_BODY_SIZE;
//             unit->head.pack_range = (unit_count + last_unit_id) - y;
//             xchannel_push(channel, unit);
//         }

//         if (last_unit_id){
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (unit_count * PACK_BODY_SIZE), last_unit_size);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = last_unit_size;
//             unit->head.pack_range = last_unit_id;
//             xchannel_push(channel, unit);
//         }
//     }
// }

//make_channel(addr, 验证数据) //channel 在外部线程创建
//clear_channel(ChannelPtr) //在外部线程销毁


#endif //__XMSGER_H__