#ifndef __XMESSENGER_H__
#define __XMESSENGER_H__


#include <ex/ex.h>
#include <sys/struct/xheap.h>
#include <sys/struct/xtree.h>
#include <sys/struct/xline.h>


enum {
    XMSG_PACK_BYE = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_ACK = 0x02,
    XMSG_PACK_PING = 0x04,
    XMSG_PACK_FINAL = 0x08
};


#define PACK_HEAD_SIZE              16
#define PACK_BODY_SIZE              1280 // 1024 + 256
// #define PACK_BODY_SIZE              8
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_WINDOW_RANGE           16

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.4K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )



typedef struct xmsgchannel* xmsgchannel_ptr;
typedef struct xmsglistener* xmsglistener_ptr;
typedef struct xmessenger* xmessenger_ptr;
typedef struct xmsgchannellist* xmsgchannellist_ptr;

typedef struct xmsgaddr {
    void *addr;
    unsigned int addrlen;
    uint8_t keylen;
    union {
        char key[6];
        struct {
            uint32_t ip;
            uint16_t port;
        };
    };
}*xmsgaddr_ptr;

typedef struct xmsghead {
    uint8_t type;
    uint8_t sn; //serial number 包序列号
    uint8_t ack; //ack 单个包的 ACK
    uint8_t acks; //acks 这个包和它之前的所有包的 ACK
    uint8_t resend, x, y, z; //这个包的重传计数
    uint32_t crc;
    uint16_t pack_range; // message fragment count 消息分包数量
    uint16_t pack_size; // message fragment length 这个分包的长度
}*xmsghead_ptr;

typedef struct xmsgpack {
    bool comfirmed;
    uint8_t resending;
    struct xheapnode ts;
    // uint32_t timestamp; //计算往返耗时
    xmsgchannel_ptr channel;
    struct xmsghead head;
    uint8_t body[PACK_BODY_SIZE];
}*xmsgpack_ptr;

typedef struct xmsgpackbuf {
    uint8_t range;
    ___atom_8bit upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgpackbuf_ptr;


typedef struct xmsg {
    xmsgchannel_ptr channel;
    size_t size;
    char data[1];
}*xmsg_ptr;


typedef struct xmsgbuf {
    xmsg_ptr msg;
    uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgbuf_ptr;


struct xmsgchannel {
    xmsgchannel_ptr prev, next;
    int status;
    __ex_lock_ptr mtx;
    ___atom_bool is_connected;
    ___atom_bool is_channel_breaker;
    ___atom_bool connected;
    ___atom_bool disconnected;
    ___atom_bool termination;
    uint8_t interval;
    uint64_t update;
    xheap_ptr timerheap;
    struct xmsghead ack;
    xmsgbuf_ptr msgbuf;
    xmsgchannellist_ptr queue;
    struct xmsgaddr addr;
    xmsgpackbuf_ptr sendbuf;
    xmessenger_ptr mtp;
};

//channellist
typedef struct xmsgchannellist {
    ___atom_size len;
    ___atom_bool lock;
    struct xmsgchannel head, end;
}*xmsgchannellist_ptr;


struct xmsglistener {
    void *ctx;
    void (*onConnectionToPeer)(struct xmsglistener*, xmsgchannel_ptr channel);
    void (*onConnectionFromPeer)(struct xmsglistener*, xmsgchannel_ptr channel);
    void (*onDisconnection)(struct xmsglistener*, xmsgchannel_ptr channel);
    void (*onReceiveMessage)(struct xmsglistener*, xmsgchannel_ptr channel, xmsg_ptr msg);
    //没有数据接收或发送，通知用户监听网络接口，有数据可读时，通知messenger
    void (*onIdle)(struct xmsglistener*, xmsgchannel_ptr channel);
    void (*onSendable)(struct xmsglistener*, xmsgchannel_ptr channel);
};


typedef struct xmsgsocket {
    void *ctx;
    //不需要内部监听，监听网络接口由用户层的线程处理
    // void (*listening)(struct xmsgsocket *socket);
    size_t (*sendto)(struct xmsgsocket *socket, xmsgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct xmsgsocket *socket, xmsgaddr_ptr addr, void *data, size_t size);
}*xmsgsocket_ptr;


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      1000000000ULL

struct xmessenger {
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __ex_lock_ptr mtx;
    ___atom_bool running;
    //socket可读或者有数据待发送时为true
    ___atom_bool working;
    xmsglistener_ptr listener;
    xmsgsocket_ptr msgsock;
    xline_maker_ptr mainloop_func;
    __ex_task_ptr mainloop_task;
    ___atom_size send_queue_length;
    ___atom_bool connection_buf_lock;
    xmsgpackbuf_ptr connection_buf;
    __ex_msg_pipe *pipe;
    xmsgchannellist_ptr sendqueue;
};


#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - 1 - (b)->wpos + (b)->rpos))



static inline void msgchannel_enqueue(xmsgchannel_ptr channel, xmsgchannellist_ptr queue)
{
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    channel->queue = queue;
    ___atom_add(&queue->len, 1);
}

static inline xmsgchannel_ptr msgchannel_dequeue(xmsgchannellist_ptr queue)
{
    if (queue->len > 0){
        if (___atom_try_lock(&queue->lock)){
            xmsgchannel_ptr channel = queue->head.next;
            channel->prev->next = channel->next;
            channel->next->prev = channel->prev;
            channel->queue = NULL;
            ___atom_sub(&queue->len, 1);
            ___atom_unlock(&queue->lock);
            return channel;
        }
    }
    return NULL;
}

static inline void msgchannel_clear(xmsgchannel_ptr channel)
{
    ___lock lk = __ex_lock(channel->mtx);
    if (channel->queue != NULL){
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
        ___atom_sub(&channel->queue->len, 1);
        channel->queue = NULL;
    }
    ___set_true(&channel->disconnected);
    __ex_notify(channel->mtx);
    __ex_unlock(channel->mtx, lk);
    ___atom_sub(&channel->mtp->send_queue_length, __transbuf_usable(channel->sendbuf));
    xtree_take(channel->mtp->peers, channel->addr.key, channel->addr.keylen);
}

static inline void msgchannel_push(xmsgchannel_ptr channel, xmsgpack_ptr unit)
{
    unit->channel = channel;
    unit->resending = 0;
    unit->comfirmed = false;

    if (___is_true(&channel->disconnected)){
        return;
    }

    while (__transbuf_writable(channel->sendbuf) == 0){
        // 不进行阻塞，添加续传逻辑
        // TODO 设置当前 channel 的等待写入标志
        ___lock lk = __ex_lock(channel->mtx);
        if (___is_true(&channel->disconnected)){
            __ex_unlock(channel->mtx, lk);
            return;
        }
        __ex_wait(channel->mtx, lk);
        if (___is_true(&channel->disconnected)){
            __ex_unlock(channel->mtx, lk);
            return;
        }
        __ex_unlock(channel->mtx, lk);
    }

    // 再将 unit 放入缓冲区 
    unit->head.sn = channel->sendbuf->wpos;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    // 在主循环中加入 Idle 条件标量，直接检测 Idle 条件。
    // 是否统计所有 channel 的待发送包数量？
    if (___atom_add(&channel->mtp->send_queue_length, 1) == 1){
        ___lock lk = __ex_lock(channel->mtp->mtx);
        __ex_notify(channel->mtp->mtx);
        __ex_unlock(channel->mtp->mtx, lk);
    }
}

static const char* get_transunit_msg(xmsgpack_ptr unit)
{
    struct xline_maker kv;
    xline_parse(&kv, unit->body);
    return xline_find(&kv, "msg");
}

static inline void msgchannel_pull(xmsgchannel_ptr channel, xmsgpack_ptr ack)
{
    __ex_logi("msgchannel_pull >>>>>-------------------> range: %u sn: %u rpos: %u upos: %u wpos %u",
           ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 xmsgpack
    if (__transbuf_inuse(channel->sendbuf) && ((uint8_t)(ack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        uint8_t index;
        xmsgpack_ptr unit;
        xheapnode_ptr timenode;

        if (ack->head.ack == ack->head.acks){
            // 收到连续的 ACK，连续的最大序列号是 maximal

            do {
                //TODO 计算往返延时
                //TODO 统计丢包率
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                unit = channel->sendbuf->buf[index];

                __ex_logi("msgchannel_pull >>>>>-------------------> range: %u sn: %u rpos: %u upos: %u wpos %u msg: [%s]",
                       ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0, get_transunit_msg(unit));

                if (!unit->comfirmed){
                    // 移除定时器，设置确认状态
                    assert(channel->timerheap->array[unit->ts.pos] != NULL);
                    unit->comfirmed = true;
                    timenode = xheap_remove(channel->timerheap, &unit->ts);
                    assert(timenode->value == unit);
                }

                if (unit->head.type == XMSG_PACK_PING){
                    if (___set_true(&channel->connected)){
                        //TODO 这里是主动发起连接 onConnectionToPeer
                        channel->mtp->listener->onConnectionToPeer(channel->mtp->listener, channel);
                    }
                }else if (unit->head.type == XMSG_PACK_BYE){
                    //TODO 不会处理 BEY 的 ACK，BEY 在释放连接之前，会一直重传。
                    if (___set_true(&channel->disconnected)){
                        msgchannel_clear(channel);
                        channel->mtp->listener->onDisconnection(channel->mtp->listener, channel);
                    }
                }

                // 释放内存
                free(unit);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                ___atom_add(&channel->sendbuf->rpos, 1);
                __ex_notify(channel->mtx);

            } while ((uint8_t)(ack->head.acks - channel->sendbuf->rpos) <= 0);

            //TODO 如果当前 channel 设置了待写入标志 与 缓冲区可写，则通知用户可写入。

//            assert(__transbuf_inuse(channel->sendbuf) > 0);
//            index = __transbuf_rpos(channel->sendbuf);
//            unit = channel->sendbuf->buf[index];

//            if (!unit->comfirmed){
//                assert(channel->timer->array[unit->ts.pos] != NULL);
//                unit->comfirmed = true;
//                timenode = heap_delete(channel->timer, &unit->ts);
//                assert(timenode->value == unit);
//            }

//            free(unit);
//            channel->sendbuf->buf[index] = NULL;

//            ___atom_add(&channel->sendbuf->rpos, 1);
//            ___mutex_notify(channel->mtx);

            channel->interval = 0;

        } else {

            // __logi("msgchannel_pull recv interval ack: %u acks: %u", ack->head.ack, ack->head.acks);
            index = ack->head.ack & (PACK_WINDOW_RANGE - 1);
            unit = channel->sendbuf->buf[index];

            __ex_logi("msgchannel_pull >>>>>>---------->>> range: %u sn: %u rpos: %u upos: %u wpos %u msg: [%s]",
                   ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0, get_transunit_msg(unit));

            // 检测此 SN 是否未确认
            if (unit && !unit->comfirmed){
                assert(channel->timerheap->array[unit->ts.pos] != NULL);
                // 移除定时器，设置确认状态
                unit->comfirmed = true;
                timenode = xheap_remove(channel->timerheap, &unit->ts);
                assert(timenode->value == unit);
            }

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            if (++channel->interval > 2){
                index = channel->sendbuf->rpos;
                while (index != ack->head.sn) {
                    unit = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                    if (!unit->comfirmed && unit->resending == 0){
                        __ex_logi("msgchannel_pull ############################### resend sn: %u", index);
                        if (channel->mtp->msgsock->sendto(channel->mtp->msgsock, &channel->addr, 
                            (void*)&(unit->head), PACK_HEAD_SIZE + unit->head.pack_size) == PACK_HEAD_SIZE + unit->head.pack_size){
                            unit->resending++;
                        }
                    }
                    index++;
                }
            }
        }
    }
}

static inline void msgchannel_recv(xmsgchannel_ptr channel, xmsgpack_ptr unit)
{
    channel->ack = unit->head;
    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.pack_size = 0;
    uint16_t index = unit->head.sn & (PACK_WINDOW_RANGE - 1);
    
    __ex_logi("msgchannel_recv >>>>---------> SN: %u rpos: %u wpos: %u msg: [%s]",
           unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos, get_transunit_msg(unit));
    
    if (unit->head.sn == channel->msgbuf->wpos){

        // 更新最大连续 SN
        channel->ack.ack = unit->head.sn;
        channel->ack.acks = channel->msgbuf->wpos;

        channel->msgbuf->buf[index] = unit;
        channel->msgbuf->wpos++;
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
            //TODO
            assert(__transbuf_writable(channel->msgbuf) > 0);
            channel->msgbuf->wpos++;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->msgbuf->wpos - unit->head.sn) > (uint8_t)(unit->head.sn - channel->msgbuf->wpos)){

            channel->ack.ack = unit->head.sn;
            channel->ack.acks = channel->msgbuf->wpos - 1;

            // SN 在 wpos 方向越界，是提前到达的 MSG
            if (channel->msgbuf->buf[index] == NULL){
                // __logi("msgchannel_recv over range sn: %u wpos: %u", unit->head.serial_number, channel->msgbuf->wpos);
                channel->msgbuf->buf[index] = unit;
            }else {
                free(unit);
            }
            
        }else {
            // __logi("msgchannel_recv over range ++++++++++++++++ rpos %u", unit->head.serial_number);
            channel->ack.ack = channel->msgbuf->wpos - 1;
            channel->ack.acks = channel->ack.ack;
            // SN 在 rpos 方向越界，是重复的 SN
            free(unit);
        }
    }

//    __logi("msgchannel_recv sendto enter");
    if ((channel->mtp->msgsock->sendto(channel->mtp->msgsock, &channel->addr, (void*)&(channel->ack), PACK_HEAD_SIZE)) == PACK_HEAD_SIZE) {
//        __logi("msgchannel_recv sendto return ----------->");
        __ex_logi("msgchannel_recv ACK ---> SN: %u rpos: %u wpos: %u",
               unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);
        // channel->ack.type = TRANSUNIT_NONE;
    }
//    __logi("msgchannel_recv sendto exit");

    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->pack_range = channel->msgbuf->buf[index]->head.pack_range;
            assert(channel->msgbuf->pack_range != 0 && channel->msgbuf->pack_range <= XMSG_PACK_RANGE);
            channel->msgbuf->msg = (xmsg_ptr)malloc(sizeof(struct xmsg) + (channel->msgbuf->pack_range * PACK_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }

        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.pack_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.pack_size;
        channel->msgbuf->pack_range--;
//        __logi("msgchannel_recv pack_range %u", channel->msgbuf->pack_range);
        if (channel->msgbuf->pack_range == 0){
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
//            __logi("msgchannel_recv process msg %s", channel->msgbuf->msg->data);
//            __logi("msgchannel_recv message enter");
            channel->mtp->listener->onReceiveMessage(channel->mtp->listener, channel, channel->msgbuf->msg);
//            __logi("msgchannel_recv message exit");
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }
}

static inline xmsgchannel_ptr msgchannel_create(xmessenger_ptr mtp, xmsgaddr_ptr addr)
{
    xmsgchannel_ptr channel = (xmsgchannel_ptr) malloc(sizeof(struct xmsgchannel));
    channel->connected = false;
    channel->disconnected = false;
    channel->update = __ex_clock();
    channel->interval = 0;
    channel->mtp = mtp;
    channel->addr = *addr;
    channel->mtx = __ex_lock_create();
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    channel->timerheap = xheap_create(PACK_WINDOW_RANGE);
    channel->queue = NULL;
    return channel;
}

static inline void msgchannel_release(xmsgchannel_ptr channel)
{
    __ex_logi("msgchannel_release enter");
    __ex_lock_free(channel->mtx);
    xheap_free(&channel->timerheap);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __ex_logi("msgchannel_release exit");
}

//提供给用户一个从Idle状态唤醒主循环的接口
static inline void xmessenger_wake(xmessenger_ptr messenger)
{
    __ex_logi("xmessenger_wake enter %p\n", messenger);
    ___lock lk = __ex_lock(messenger->mtx);
    __ex_notify(messenger->mtx);
    __ex_unlock(messenger->mtx, lk);
    __ex_logi("xmessenger_wake exit\n");
}

static inline void messenger_loop(xline_maker_ptr ctx)
{
    __ex_logi("messenger_loop enter\n");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000ULL;
    // transack_ptr ack;
    struct xmsgaddr addr;
    xmsgpack_ptr recvunit = NULL;
    xheapnode_ptr timenode;
    xmsgpack_ptr sendunit = NULL;
    xmsgchannel_ptr channel = NULL;
    xmsgchannel_ptr next = NULL;

    xmessenger_ptr messenger = (xmessenger_ptr)xline_find_ptr(ctx, "ctx");

    addr.addr = NULL;

    while (___is_true(&messenger->running))
    {

        if (recvunit == NULL){
            recvunit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            if (recvunit == NULL){
                __ex_logi("xmessenger_wake malloc failed\n");
                exit(0);
            }
        }

        recvunit->head.type = 0;
        recvunit->head.pack_size = 0;
        result = messenger->msgsock->recvfrom(messenger->msgsock, &addr, &recvunit->head, PACK_ONLINE_SIZE);

        if (result == (recvunit->head.pack_size + PACK_HEAD_SIZE)){

//            __logi("messenger_loop recv ip: %u port: %u msg: %s", addr.ip, addr.port, get_transunit_msg(recvunit));

            //TODO 先检测是否为 BEY。
            if (recvunit->head.type == XMSG_PACK_BYE){
                //如果当前状态已经是断开连接中，说明之前主动发送过 BEY。所以直接释放连接和相关资源。
                channel = (xmsgchannel_ptr)xtree_find(messenger->peers, addr.key, addr.keylen);
                if (channel != NULL){
                    if (___set_true(&channel->disconnected)){
                        //被动断开的一端，收到第一个 BEY，回复第一个 BEY，并且启动超时重传。
                        msgchannel_push(channel, recvunit);
                    }else {
                        if (___is_true(&channel->is_channel_breaker)){
                            //主动方收到第一个 BEY，直接 sendto 发送第二个 BEY。
                        }
                        //被动方收到第二个 BEY。
                        //释放连接，结束超时重传。
                        messenger->listener->onDisconnection(messenger->listener, channel);
                    }
                }else {
                    //回复的第二个 BEY 可能丢包了
                    //直接 sendto 回复 FINAL
                }

            }else if (recvunit->head.type == XMSG_PACK_FINAL){
                //被动方收到 FINAL，释放连接，结束超时重传。
                channel = (xmsgchannel_ptr)xtree_find(messenger->peers, addr.key, addr.keylen);
                if (channel){
                    messenger->listener->onDisconnection(messenger->listener, channel);
                }

            }else if (recvunit->head.type == XMSG_PACK_PING){

                channel = (xmsgchannel_ptr)xtree_find(messenger->peers, addr.key, addr.keylen);
                __ex_logi("messenger_loop XMSG_PACK_PING find channel: 0x%x ip: %u port: %u\n", channel, addr.ip, addr.port);
                if (channel != NULL){
                    //回复建立连接成功消息
                    //continue
                    //TODO 删除进行重连的逻辑
                    if (___set_true(&channel->disconnected)){
                        __ex_logi("messenger_loop reconnection channel clear: 0x%x ip: %u port: %u\n", channel, addr.ip, addr.port);
                        msgchannel_clear(channel);
                        messenger->listener->onDisconnection(messenger->listener, channel);
                    }
                }
                //定期向中心服务器更新密文，并记录中心服务器的更新时间戳。
                //在这里进行验证，首先检测（密文）的生成时间戳，确定使用当前的KEY解密，还是上一次的KEY解密。
                //解密失败，把所有非法连接挡在这里。
                //解密成功，创建连接。
                //回复建立连接成功消息。
                channel = msgchannel_create(messenger, &addr);
                __ex_logi("messenger_loop new connections channel: 0x%x ip: %u port: %u\n", channel, addr.ip, addr.port);
                msgchannel_enqueue(channel, messenger->sendqueue);
                xtree_save(messenger->peers, addr.key, addr.keylen, channel);
                ___set_true(&channel->connected);
                //TODO 这里是被动建立连接 onConnectionFromPeer
                channel->mtp->listener->onConnectionFromPeer(channel->mtp->listener, channel);

            }else {

                channel = (xmsgchannel_ptr)xtree_find(messenger->peers, addr.key, addr.keylen);
            }


            if (channel != NULL){

                channel->update = __ex_clock();

                if (recvunit->head.type == XMSG_PACK_ACK){
                    msgchannel_pull(channel, recvunit);
                }else if (/*recvunit->head.type == XMSG_PACK_MSG*/ recvunit->head.type > XMSG_PACK_BYE && recvunit->head.type < XMSG_PACK_FINAL){
                    msgchannel_recv(channel, recvunit);
                    if (recvunit->head.type == XMSG_PACK_BYE){
                        //如果当前状态已经是断开连接中，说明之前主动发送过 BEY。所以直接释放连接和相关资源。
                        if (___set_true(&channel->disconnected)){
                            __ex_logi("messenger_loop disconnection channel: 0x%x ip: %u port: %u\n", channel, addr.ip, addr.port);
                            msgchannel_clear(channel);
                            messenger->listener->onDisconnection(messenger->listener, channel);
                        }else {
                            //如果当前不是断开连接中状态，说明是对方主动起 BEY，需要回复 ACK。
                            //等收到 BEY 的 ACK 再释放连接。
                        }
                    }
                    recvunit = NULL;
                }

            }else {
                //忽略非法连接
                __ex_logi("messenger_loop recv a invalid xmsgpack: %u ip: %u port: %u\n", recvunit->head.type, addr.ip, addr.port);
            }



        }else {

            //定时select，使用下一个重传定时器到期时间，如果定时器为空，最大10毫秒间隔。
            //主动创建连接，最多需要10毫秒。
            if (messenger->send_queue_length == 0 && __ex_msg_pipe_readable(messenger->pipe) == 0){
                messenger->listener->onIdle(messenger->listener, channel);
                ___lock lk = __ex_lock(messenger->mtx);
                __ex_notify(messenger->mtx);
                __ex_timed_wait(messenger->mtx, lk, timer);
                __ex_unlock(messenger->mtx, lk);
                timer = 1000000000ULL * 10;
            }
        }


        if (__ex_msg_pipe_readable(messenger->pipe) > 0){
            __ex_logi("create channel enter\n");
            xline_maker_ptr ctx = __ex_msg_pipe_hole_reader(messenger->pipe);
            xmsgaddr_ptr *addr = (xmsgaddr_ptr)xline_find_ptr(ctx, "addr");
            xmsgchannel_ptr channel = msgchannel_create(messenger, addr);
            __ex_msg_pipe_update_reader(messenger->pipe);
            if (channel){
                xtree_save(messenger->peers, channel->addr.key, channel->addr.keylen, channel);
                msgchannel_enqueue(channel, messenger->sendqueue);
                xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
                //TODO 以PING消息建立连接。
                // unit->head.type = XMSG_PACK_PING;
                unit->head.type = XMSG_PACK_PING;
                unit->head.pack_range = 1;
                struct xline_maker kv;
                xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
                //TODO 加密数据，需要对端验证
                xline_ptr msg = xline_find(ctx, "msg");
                xline_add_text(&kv, "msg", __xline_to_data(msg), __xline_sizeof_data(msg));
                unit->head.pack_size = kv.pos;
                msgchannel_push(channel, unit);
            }
            __ex_logi("create channel exit\n");
        }


        channel = messenger->sendqueue->head.next;
        while (channel != &messenger->sendqueue->end)
        {
            next = channel->next;

            //TODO 一次发送一个包，还是全部发送，还是指定每次发送数量？
            if (__transbuf_usable(channel->sendbuf) > 0){
                sendunit = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                //不要插手 pack 类型，这里只管发送。

                __ex_logi("messenger_loop sendto ip: %u port: %u channel: 0x%x SN: %u msg: %s\n",
                       channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));

                result = messenger->msgsock->sendto(messenger->msgsock, &channel->addr, (void*)&(sendunit->head), PACK_HEAD_SIZE + sendunit->head.pack_size);
                if (result == PACK_HEAD_SIZE + sendunit->head.pack_size){
                    channel->sendbuf->upos++;
                    //检测是否写阻塞，再执行唤醒。（因为只有缓冲区已满，才会写阻塞，所以无需原子锁定再进行检测，写阻塞会在下一次检测时被唤醒）
                    __ex_notify(channel->mtx);//TODO
                    //检测队列为空，才开启定时器
                    //所有channel统一使用一个定时器（还是每个channel各自维护定时器逻辑比较简单）。
                    //定时器的堆越小，操作效率越高。
                    sendunit->ts.key = __ex_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                    sendunit->ts.value = sendunit;
                    xheap_push(channel->timerheap, &sendunit->ts);
                    ___atom_sub(&messenger->send_queue_length, 1);
                    sendunit->resending = 0;
                }else {
                    sendunit->resending++;
                    //重新加入定时器，间隔递增。
                    if (sendunit->resending > 5){
                        //抛出断开连接事件，区分正常断开和超时断开。
                        __ex_logi("messenger_loop ***try again timeout*** ip: %u port: %u channel: 0x%x SN: %u msg: %s\n",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));

                        msgchannel_clear(channel);
                        messenger->listener->onDisconnection(messenger->listener, channel);
                    }
                }

            }else {
                //TODO 检测 channel 空闲时间，如果连接超时，就断开连接。清理僵尸连接。
                //onDisconnect 回调要区分发送超时和连接超时
                if (channel->timerheap->pos == 0 && __transbuf_readable(channel->sendbuf) == 0){
                    if (__ex_clock() - channel->update > (1000000000ULL * 30)){
                        __ex_logi("messenger_loop ***timeout recycle*** ip: %u port: %u channel: 0x%x\n", channel->addr.ip, channel->addr.port, channel);
                        msgchannel_clear(channel);
                        messenger->listener->onDisconnection(messenger->listener, channel);
                    }
                }
            }

            if (channel->timerheap->pos > 0){

                timeout = __heap_min(channel->timerheap)->key - __ex_clock();
//                __logi("messenger_loop timeout resend timestamp: %llu current: %llu timeout: %lld", sendunit->ts.key, __ex_clock(), timeout);
                if (timeout > 0){

                    if (timer > timeout){
                        timer = timeout;
                    }

                }else {

                    sendunit = (xmsgpack_ptr)__heap_min(channel->timerheap)->value;
                    if (sendunit->resending < 5){
                        __ex_logi("messenger_loop resend ip: %u port: %u channel: 0x%x SN: %u msg: %s\n",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));
                        messenger->msgsock->sendto(messenger->msgsock, &sendunit->channel->addr, (void*)&(sendunit->head), PACK_HEAD_SIZE + sendunit->head.pack_size);
                        if (result == PACK_HEAD_SIZE + sendunit->head.pack_size){
                            xheap_pop(channel->timerheap);
                            sendunit->ts.key = __ex_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->ts.value = sendunit;
                            xheap_push(channel->timerheap, &sendunit->ts);
                            sendunit->resending = 0;
                        }else {
                            timer = TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->resending++;
                        }

                    }else {

                        __ex_logi("messenger_loop ***resend timeout*** ip: %u port: %u channel: 0x%x SN: %u msg: %s\n",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));
                        msgchannel_clear(channel);
                        messenger->listener->onDisconnection(messenger->listener, channel);
                    }
                }
            }

            //TODO 如果当前 channel 设置了待写入标志 与 缓冲区可写，则通知用户可写入（因为设置待写入标志时没有加锁，所以需要重复检测状态）。

            channel = next;
        }

        // //TODO 不再需要断开连接队列，让用户收到 onDisconntion 事件后，调用 disconnect 方法，发送一个 BEY pack，主循环会释放 channel 的相关资源。
        // if (messenger->disconnctionqueue->len > 0){
        //     if (___atom_try_lock(&messenger->disconnctionqueue->lock)){
        //         channel = messenger->disconnctionqueue->head.next;
        //         do {
        //             __ex_logi("messenger_loop ----------------------------------------- delete channel 0x%x\n", channel);
        //             channel->next->prev = channel->prev;
        //             channel->prev->next = channel->next;
        //             ___atom_sub(&messenger->disconnctionqueue->len, 1);
        //             msgchannel_release(channel);
        //             channel = messenger->disconnctionqueue->head.next;
        //         }while (channel != &messenger->disconnctionqueue->end);
        //         ___atom_unlock(&messenger->disconnctionqueue->lock);
        //     }
        // }
    }


    if (recvunit != NULL){
        free(recvunit);
    }

    free(addr.addr);

    __ex_logi("messenger_loop exit\n");
}

static inline xmessenger_ptr xmessenger_create(xmsgsocket_ptr msgsock, xmsglistener_ptr listener)
{
    __ex_logi("xmessenger_create enter\n");

    xmessenger_ptr messenger = (xmessenger_ptr)calloc(1, sizeof(struct xmessenger));

    messenger->running = true;
    messenger->msgsock = msgsock;
    messenger->listener = listener;
    messenger->send_queue_length = 0;
    messenger->mtx = __ex_lock_create();
    messenger->peers = xtree_create();
    
    messenger->sendqueue = malloc(sizeof(struct xmsgchannellist));
    messenger->sendqueue->len = 0;
    messenger->sendqueue->lock = false;
    messenger->sendqueue->head.prev = NULL;
    messenger->sendqueue->end.next = NULL;
    messenger->sendqueue->head.next = &messenger->sendqueue->end;
    messenger->sendqueue->end.prev = &messenger->sendqueue->head;

    messenger->pipe = __ex_msg_pipe_create(256);
    __ex_check(messenger->pipe != NULL);

//    mtp->mainloop_func = linekv_create(1024);
//    linekv_add_ptr(mtp->mainloop_func, "func", (void*)messenger_loop);
//    linekv_add_ptr(mtp->mainloop_func, "ctx", mtp);
//    mtp->mainloop_task = taskqueue_create();
//    taskqueue_post(mtp->mainloop_task, mtp->mainloop_func);

    __ex_logi("xmessenger_create exit\n");

    return messenger;

Clean:

    return NULL;
}

static inline void xmessenger_run(xmessenger_ptr messenger)
{
    messenger->mainloop_task = __ex_task_run(messenger_loop, messenger);
}

static void free_channel(void *val)
{
    __ex_logi(">>>>------------> free channel 0x%x\n", val);
    msgchannel_release((xmsgchannel_ptr)val);
}

static inline void xmessenger_free(xmessenger_ptr *pptr)
{
    __ex_logi("xmessenger_free enter\n");
    if (pptr && *pptr){        
        xmessenger_ptr msger = *pptr;
        *pptr = NULL;
        ___set_false(&msger->running);
        __ex_broadcast(msger->mtx);
        __ex_task_free(&msger->mainloop_task);
        xline_maker_clear(&msger->mainloop_func);
        xtree_clear(msger->peers, free_channel);
        xtree_free(&msger->peers);
        __ex_lock_free(msger->mtx);
        if (msger->sendqueue){
            free(msger->sendqueue);
        }
        free(msger);
    }
    __ex_logi("xmessenger_free exit\n");
}

static inline void xmessenger_wait(xmessenger_ptr messenger)
{
    ___lock lk = __ex_lock(messenger->mtx);
    __ex_wait(messenger->mtx, lk);
    __ex_unlock(messenger->mtx, lk);
}

//TODO 增加一个用户上下文参数，断开连接时，可以直接先释放channel，然后在回调中给出用户上下文，通知用户回收相关资源。
//TODO 增加一个加密验证数据，messenger 只负责将消息发送到对端，连接能否建立，取决于对端验证结果。
static inline int xmessenger_connect(xmessenger_ptr messenger, xmsgaddr_ptr addr)
{
    __ex_logi("xmessenger_connect enter\n");
    xline_maker_ptr *ctx = __ex_msg_pipe_hold_writer(messenger->pipe);
    if (ctx == NULL){
        return -1;
    }
    xline_add_ptr(ctx, "addr", addr);
    xline_add_text(ctx, "msg", "PING", strlen("PING"));
    __ex_msg_pipe_update_writer(messenger->pipe);
    __ex_logi("xmessenger_connect exit\n");
    return 0;
}

static inline void xmessenger_disconnect(xmessenger_ptr mtp, xmsgchannel_ptr channel)
{
    __ex_logi("xmessenger_disconnect enter");
    xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    unit->head.type = XMSG_PACK_BYE;
    unit->head.pack_size = 0;
    unit->head.pack_range = 1;
    //主动发送 BEY，要设置 channel 主动状态。
    ___set_true(&channel->is_channel_breaker);
    //向对方发送 BEY，对方回应后，再移除 channel。
    ___set_true(&channel->disconnected);
    //主动断开的一端，发送第一个 BEY，并且启动超时重传。
    msgchannel_push(channel, unit);
    __ex_logi("xmessenger_disconnect exit");
}

//ping 需要发送加密验证，如果此时链接已经断开，ping 消息可以进行重连。
static inline void xmessenger_ping(xmsgchannel_ptr channel)
{
    __ex_logi("xmessenger_ping enter");
    if (__ex_clock() - channel->update >= 1000000000ULL * 10){
        xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
        unit->head.type = XMSG_PACK_PING;
        unit->head.pack_range = 1;
        struct xline_maker kv;
        xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
        xline_add_text(&kv, "msg", "PING", strlen("PING"));
        unit->head.pack_size = kv.pos;
        msgchannel_push(channel, unit);
    }
    __ex_logi("xmessenger_ping exit");
}

static inline void xmessenger_send(xmessenger_ptr mtp, xmsgchannel_ptr channel, void *data, size_t size)
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
            xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            memcpy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
            unit->head.type = XMSG_PACK_MSG;
            unit->head.pack_size = PACK_BODY_SIZE;
            unit->head.pack_range = XMSG_PACK_RANGE - y;
            msgchannel_push(channel, unit);
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
            xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            memcpy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
            unit->head.type = XMSG_PACK_MSG;
            unit->head.pack_size = PACK_BODY_SIZE;
            unit->head.pack_range = (unit_count + last_unit_id) - y;
            msgchannel_push(channel, unit);
        }

        if (last_unit_id){
            xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
            memcpy(unit->body, msg_data + (unit_count * PACK_BODY_SIZE), last_unit_size);
            unit->head.type = XMSG_PACK_MSG;        
            unit->head.pack_size = last_unit_size;
            unit->head.pack_range = last_unit_id;
            msgchannel_push(channel, unit);
        }
    }
}


//make_channel(addr, 验证数据) //channel 在外部线程创建
//clear_channel(ChannelPtr) //在外部线程销毁


#endif //__XMESSENGER_H__



// #include <stdio.h>
// #include <string.h>

// // 函数：对字符串进行异或加密/解密
// void xorEncryptDecrypt(char *input, char *output, char key) {
//     size_t len = strlen(input);

//     for (size_t i = 0; i < len; ++i) {
//         output[i] = input[i] ^ key;
//     }

//     output[len] = '\0';  // 添加字符串结束符
// }

// int main() {
//     // 原始字符串
//     char original[] = "Hello, XOR Encryption!";

//     // 密钥
//     char key = 'K';

//     // 加密
//     char encrypted[256];
//     xorEncryptDecrypt(original, encrypted, key);

//     // 输出加密后的字符串
//     printf("Encrypted: %s\n", encrypted);

//     // 解密
//     char decrypted[256];
//     xorEncryptDecrypt(encrypted, decrypted, key);

//     // 输出解密后的字符串
//     printf("Decrypted: %s\n", decrypted);

//     return 0;
// }
