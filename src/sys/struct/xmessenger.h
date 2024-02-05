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
    XMSG_PACK_PONG = 0x08,
    XMSG_PACK_FINAL = 0x10
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
    uint32_t cid;
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
    uint8_t key;
    uint32_t cid;
    uint32_t peer_cid;
    uint32_t peer_key;
    xmsgchannel_ptr prev, next;
    int status;
    __ex_mutex_ptr mtx;
    ___atom_bool is_connected;
    ___atom_bool is_channel_breaker;
    ___atom_bool connected;
    ___atom_bool disconnected;
    ___atom_bool termination;
    uint8_t delay;
    uint64_t update;
    xheap_ptr timerheap;
    struct xmsghead ack;
    xmsgbuf_ptr msgbuf;
    xmsgchannellist_ptr queue;
    struct xmsgaddr addr;
    xmsgpackbuf_ptr sendbuf;
    xmessenger_ptr msger;
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
    // 每次创建新的channel时加一
    uint32_t cid;
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __ex_mutex_ptr mtx;
    ___atom_bool running;
    //socket可读或者有数据待发送时为true
    ___atom_bool working;
    xmsglistener_ptr listener;
    xmsgsocket_ptr msgsock;
    xmaker_ptr mainloop_func;
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

#define XMSG_KEY    'X'

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
    ___lock lk = __ex_mutex_lock(channel->mtx);
    if (channel->queue != NULL){
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
        ___atom_sub(&channel->queue->len, 1);
        channel->queue = NULL;
    }
    ___set_true(&channel->disconnected);
    __ex_mutex_notify(channel->mtx);
    __ex_mutex_unlock(channel->mtx, lk);
    ___atom_sub(&channel->msger->send_queue_length, __transbuf_usable(channel->sendbuf));
    xtree_take(channel->msger->peers, channel->addr.key, channel->addr.keylen);
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
        ___lock lk = __ex_mutex_lock(channel->mtx);
        if (___is_true(&channel->disconnected)){
            __ex_mutex_unlock(channel->mtx, lk);
            return;
        }
        __ex_mutex_wait(channel->mtx, lk);
        if (___is_true(&channel->disconnected)){
            __ex_mutex_unlock(channel->mtx, lk);
            return;
        }
        __ex_mutex_unlock(channel->mtx, lk);
    }

    // 再将 unit 放入缓冲区 
    unit->head.sn = channel->sendbuf->wpos;
    // 设置校验码
    unit->head.x = XMSG_KEY ^ channel->peer_key;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    // 在主循环中加入 Idle 条件标量，直接检测 Idle 条件。
    // 是否统计所有 channel 的待发送包数量？
    if (___atom_add(&channel->msger->send_queue_length, 1) == 1){
        ___lock lk = __ex_mutex_lock(channel->msger->mtx);
        __ex_mutex_notify(channel->msger->mtx);
        __ex_mutex_unlock(channel->msger->mtx, lk);
    }
}

static const char* get_transunit_msg(xmsgpack_ptr unit)
{
    struct xmaker kv = xline_parse((xline_ptr)unit->body);
    return (const char*)xline_find(&kv, "msg");
}

static inline void msgchannel_pull(xmsgchannel_ptr channel, xmsgpack_ptr ack)
{
    __ex_logi("msgchannel_pull >>>>>-------------------> range: %u sn: %u rpos: %u upos: %u wpos %u\n",
           ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 xmsgpack
    if (__transbuf_inuse(channel->sendbuf) > 0 && ((uint8_t)(ack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        uint8_t index;
        xmsgpack_ptr unit;
        xheapnode_ptr timenode;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        if (ack->head.ack == ack->head.acks){

            do {
                //TODO 计算往返延时
                //TODO 统计丢包率
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                unit = channel->sendbuf->buf[index];

                __ex_logi("msgchannel_pull >>>>>-------------------> range: %u sn: %u rpos: %u upos: %u wpos %u msg: [%s]\n",
                       ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0, get_transunit_msg(unit));

                if (!unit->comfirmed){
                    // 移除定时器，设置确认状态
                    assert(channel->timerheap->array[unit->ts.pos] != NULL);
                    unit->comfirmed = true;
                    timenode = xheap_remove(channel->timerheap, &unit->ts);
                    assert(timenode->value == unit);
                }else {
                    // 这里可以统计收到重复 ACK 的次数
                }

                // if (unit->head.type == XMSG_PACK_PING){
                //     if (___set_true(&channel->connected)){
                //         //TODO 这里是主动发起连接 onConnectionToPeer
                //         channel->msger->listener->onConnectionToPeer(channel->msger->listener, channel);
                //     }
                // }else if (unit->head.type == XMSG_PACK_BYE){
                //     //TODO 不会处理 BEY 的 ACK，BEY 在释放连接之前，会一直重传。
                //     if (___set_true(&channel->disconnected)){
                //         msgchannel_clear(channel);
                //         channel->msger->listener->onDisconnection(channel->msger->listener, channel);
                //     }
                // }

                // 释放内存
                free(unit);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                ___atom_add(&channel->sendbuf->rpos, 1);
                // TODO 通知 channel 可写
                // channel->msger->listener->onSendable();
                __ex_mutex_notify(channel->mtx);

                // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
            } while (channel->sendbuf->rpos != ack->head.ack);

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

            channel->delay = 0;

        } else {

            // __logi("msgchannel_pull recv interval ack: %u acks: %u", ack->head.ack, ack->head.acks);
            index = ack->head.ack & (PACK_WINDOW_RANGE - 1);
            unit = channel->sendbuf->buf[index];

            __ex_logi("msgchannel_pull >>>>>>---------->>> range: %u sn: %u rpos: %u upos: %u wpos %u msg: [%s]\n",
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
            if (++channel->delay > 2){
                index = channel->sendbuf->rpos;
                while (index != ack->head.sn) {
                    unit = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                    if (!unit->comfirmed && unit->resending == 0){
                        __ex_logi("msgchannel_pull ############################### resend sn: %u\n", index);
                        if (channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, 
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

static inline int64_t msgchannel_send(xmsgchannel_ptr channel, xmsgpack_ptr ack)
{
    ssize_t result;
    xmsgpack_ptr pack;

    if (__transbuf_usable(channel->sendbuf) > 0){
        pack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
        if (ack != NULL){
            pack->head.ack = ack->head.ack;
            pack->head.acks = ack->head.acks;
        }
        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            channel->sendbuf->upos++;
            pack->ts.key = __ex_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timerheap, &pack->ts);
            ___atom_sub(&channel->msger->send_queue_length, 1);
            pack->resending = 0;
            channel->update = __ex_clock();
        }

    }else if (ack != NULL){

        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, (void*)&(ack->head), PACK_HEAD_SIZE);
        if (result == PACK_HEAD_SIZE){
            channel->update = __ex_clock();
        }
    }

    while (channel->timerheap->pos > 0 && __heap_min(channel->timerheap)->key - __ex_clock() < 0){
        pack = (xmsgpack_ptr)__heap_min(channel->timerheap)->value;
        result = channel->msger->msgsock->sendto(channel->msger->msgsock, &pack->channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            xheap_pop(channel->timerheap);
            pack->ts.key = __ex_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timerheap, &pack->ts);
            pack->resending ++;
            channel->update = __ex_clock();
        }else {
            break;
        }
    }

    if (channel->timerheap->pos > 0){
        return __heap_min(channel->timerheap)->key - __ex_clock();
    }

    return 0;

}

static inline void msgchannel_recv(xmsgchannel_ptr channel, xmsgpack_ptr unit)
{
    if (unit->head.y == 1){
        // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
        msgchannel_pull(channel, unit);
    }

    channel->ack = unit->head;
    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.pack_size = 0;
    uint16_t index = unit->head.sn & (PACK_WINDOW_RANGE - 1);
    
    __ex_logi("msgchannel_recv >>>>---------> SN: %u rpos: %u wpos: %u msg: [%s]\n",
           unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos, get_transunit_msg(unit));
    
    // 如果收到连续的 PACK
    if (unit->head.sn == channel->msgbuf->wpos){

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
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            channel->ack.ack = channel->msgbuf->wpos - 1;
            channel->ack.acks = channel->ack.ack;
            // 释放 PACK
            free(unit);
        }
    }

//    __logi("msgchannel_recv sendto enter");
    if ((channel->msger->msgsock->sendto(channel->msger->msgsock, &channel->addr, (void*)&(channel->ack), PACK_HEAD_SIZE)) == PACK_HEAD_SIZE) {
//        __logi("msgchannel_recv sendto return ----------->");
        __ex_logi("msgchannel_recv ACK ---> SN: %u rpos: %u wpos: %u\n",
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
            channel->msger->listener->onReceiveMessage(channel->msger->listener, channel, channel->msgbuf->msg);
//            __logi("msgchannel_recv message exit");
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }
}

static inline xmsgchannel_ptr msgchannel_create(xmessenger_ptr msger, xmsgaddr_ptr addr)
{
    xmsgchannel_ptr channel = (xmsgchannel_ptr) malloc(sizeof(struct xmsgchannel));
    channel->connected = false;
    channel->disconnected = false;
    channel->update = __ex_clock();
    channel->delay = 0;
    channel->msger = msger;
    channel->addr = *addr;
    channel->mtx = __ex_mutex_create();
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    channel->timerheap = xheap_create(PACK_WINDOW_RANGE);
    channel->queue = NULL;
    while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
        if (++msger->cid == 0){
            msger->cid = 1;
        }
    }
    channel->cid = msger->cid;
    channel->key = msger->cid % 255;
    return channel;
}

static inline void msgchannel_release(xmsgchannel_ptr channel)
{
    __ex_logi("msgchannel_release enter\n");
    __ex_mutex_free(channel->mtx);
    xheap_free(&channel->timerheap);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __ex_logi("msgchannel_release exit\n");
}

//提供给用户一个从Idle状态唤醒主循环的接口
static inline void xmessenger_wake(xmessenger_ptr mesger)
{
    __ex_logi("xmessenger_wake enter %p\n", mesger);
    ___lock lk = __ex_mutex_lock(mesger->mtx);
    ___set_true(&mesger->working);
    __ex_mutex_notify(mesger->mtx);
    __ex_mutex_unlock(mesger->mtx, lk);
    __ex_logi("xmessenger_wake exit\n");
}

static inline void messenger_loop(xmaker_ptr ctx)
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

    xmessenger_ptr msger = (xmessenger_ptr)xline_find_ptr(ctx, "ctx");

    addr.addr = NULL;

    while (___is_true(&msger->running))
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
        result = msger->msgsock->recvfrom(msger->msgsock, &addr, &recvunit->head, PACK_ONLINE_SIZE);

        if (result == (recvunit->head.pack_size + PACK_HEAD_SIZE)){

           __ex_logi("messenger_loop recv ip: %u port: %u msg: %s\n", addr.ip, addr.port, get_transunit_msg(recvunit));

            //TODO 先检测是否为 BEY。
            if (recvunit->head.type == XMSG_PACK_BYE){
                //如果当前状态已经是断开连接中，说明之前主动发送过 BEY。所以直接释放连接和相关资源。
                channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
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
                        msger->listener->onDisconnection(msger->listener, channel);
                    }
                }else {
                    //回复的第二个 BEY 可能丢包了
                    //直接 sendto 回复 FINAL
                }

            }else if (recvunit->head.type == XMSG_PACK_FINAL){
                //被动方收到 FINAL，释放连接，结束超时重传。
                channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
                if (channel){
                    msger->listener->onDisconnection(msger->listener, channel);
                }

            }else if (recvunit->head.type == XMSG_PACK_PING){

                if (recvunit->head.cid == 0){
                    if (recvunit->head.x ^ XMSG_KEY == XMSG_KEY){
                        struct xmaker parser = xline_parse((xline_ptr)recvunit->body);
                        uint32_t id = xline_find_uint32(&parser, "id");
                        __ex_logd("channel id = %u\n", id);
                        // 创建连接
                        channel = msgchannel_create(msger, &addr);
                        channel->peer_cid = id;
                        channel->peer_key = id % 255;
                        __ex_logi("messenger_loop new connections channel: 0x%x ip: %u port: %u\n", channel, addr.ip, addr.port);
                        msgchannel_enqueue(channel, msger->sendqueue);
                        xtree_save(msger->peers, addr.key, addr.keylen, channel);
                        ___set_true(&channel->connected);
                        //TODO 这里是被动建立连接 onConnectionFromPeer
                        channel->msger->listener->onConnectionFromPeer(channel->msger->listener, channel);

                        xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
                        unit->head.type = XMSG_PACK_PONG;
                        unit->head.pack_range = 1;
                        // 建立连接时，cid 必须是 0
                        unit->head.cid = 0;
                        struct xmaker kv;
                        xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
                        unit->head.x = (XMSG_KEY ^ XMSG_KEY);
                        xline_add_uint32(&kv, "id", channel->cid);
                        unit->head.pack_size = kv.wpos + 9;
                        msgchannel_push(channel, unit);

                        msgchannel_recv(channel, recvunit);
                    }                    
                }else {
                    // 通过索引获取 channel
                    channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
                    // 协议层验证                  
                    if (recvunit->head.x ^ channel->key == XMSG_KEY){
                        msgchannel_recv(channel, recvunit);
                    }
                }

            }else if (recvunit->head.type == XMSG_PACK_PONG){

                if (recvunit->head.cid == 0){

                    channel = (xmsgchannel_ptr)xtree_take(msger->peers, addr.key, addr.keylen);
                    if (channel && recvunit->head.x ^ channel->key == XMSG_KEY){
                        // 删除以 IP 地址为 KEY 建立的索引
                        struct xmaker parser = xline_parse((xline_ptr)recvunit->body);
                        uint32_t cid = xline_find_uint32(&parser, "id");
                        __ex_logd("msger channel to peer connected: cid = %u ip: %u port: %u\n", cid, addr.ip, addr.port);
                        // 设置对端 cid 与 key
                        channel->peer_cid = cid;
                        channel->peer_key = cid % 255;
                        msgchannel_enqueue(channel, msger->sendqueue);
                        // 以 cip 为 KEY 建立索引
                        xtree_save(msger->peers, &channel->peer_cid, 4, channel);
                        ___set_true(&channel->connected);
                        // 主动发起 PING 的一方会收到 PONG，是主动建立连接
                        channel->msger->listener->onConnectionToPeer(channel->msger->listener, channel);
                        
                        msgchannel_recv(channel, recvunit);
                    }

                }else {
                    // 通过索引获取 channel
                    channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
                    // 协议层验证                  
                    if (recvunit->head.x ^ channel->key == XMSG_KEY){
                        msgchannel_recv(channel, recvunit);
                    }
                }

            }else if (recvunit->head.type == XMSG_PACK_MSG) {

                channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
                // 协议层验证                  
                if (recvunit->head.x ^ channel->key == XMSG_KEY){
                    msgchannel_recv(channel, recvunit);
                }

            }else if (recvunit->head.type == XMSG_PACK_ACK) {

                channel = (xmsgchannel_ptr)xtree_find(msger->peers, &recvunit->head.cid, 4);
                // 协议层验证                  
                if (recvunit->head.x ^ channel->key == XMSG_KEY){
                    msgchannel_pull(channel, recvunit);
                }
            }

        }else {

            //定时select，使用下一个重传定时器到期时间，如果定时器为空，最大10毫秒间隔。
            //主动创建连接，最多需要10毫秒。
            if (msger->send_queue_length == 0 && __ex_msg_pipe_readable(msger->pipe) == 0){
                if (___set_false(&msger->working)){
                    msger->listener->onIdle(msger->listener, channel);
                }
                ___lock lk = __ex_mutex_lock(msger->mtx);
                __ex_mutex_notify(msger->mtx);
                if (msger->send_queue_length == 0 && __ex_msg_pipe_readable(msger->pipe) == 0){
                    __ex_mutex_timed_wait(msger->mtx, lk, timer);
                }
                __ex_mutex_unlock(msger->mtx, lk);
                timer = 1000000000ULL * 10;
            }
        }


        if (__ex_msg_pipe_readable(msger->pipe) > 0){
            __ex_logi("create channel enter\n");
            xmaker_ptr ctx = __ex_msg_pipe_hold_reader(msger->pipe);
            __ex_logi("xmessenger_loop ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
            if (ctx){
                xmsgaddr_ptr addr = (xmsgaddr_ptr)xline_find_ptr(ctx, "addr");
                // TODO 对方应答后要设置 peer_cid 和 key；
                xmsgchannel_ptr channel = msgchannel_create(msger, addr);
                if (channel){
                    xtree_save(msger->peers, channel->addr.key, channel->addr.keylen, channel);
                    msgchannel_enqueue(channel, msger->sendqueue);
                    xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
                    //TODO 以PING消息建立连接。
                    // unit->head.type = XMSG_PACK_PING;
                    unit->head.type = XMSG_PACK_PING;
                    unit->head.pack_range = 1;
                    // 建立连接时，cid 必须是 0
                    unit->head.cid = 0;
                    struct xmaker kv;
                    xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
                    //TODO 加密数据，需要对端验证
                    // 这里是协议层验证
                    // xline_ptr msg = xline_find(ctx, "msg");
                    // __ex_logi("create channel msg len %lu msg = %s\n", __xline_sizeof_data(msg), __xline_to_data(msg));
                    unit->head.x = (XMSG_KEY ^ XMSG_KEY);
                    xline_add_uint32(&kv, "id", channel->cid);
                    unit->head.pack_size = kv.wpos + 9;
                    msgchannel_push(channel, unit);
                }
                __ex_msg_pipe_update_reader(msger->pipe);
            }
            __ex_logi("create channel exit\n");
        }

        channel = msger->sendqueue->head.next;

        while (channel != &msger->sendqueue->end)
        {
            next = channel->next;

            timeout = msgchannel_send(channel, NULL);
            if (timeout > 0 && timer > timeout){
                timer = timeout;
            }

            if (__ex_clock() - channel->update > (1000000000ULL * 30)){
                // 十秒内没有收发过数据，释放连建
                msgchannel_clear(channel);
                channel->msger->listener->onDisconnection(channel->msger->listener, channel);
            }

            channel = next;
        }
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

    xmessenger_ptr msger = (xmessenger_ptr)calloc(1, sizeof(struct xmessenger));

    msger->cid = 1;
    msger->running = true;
    msger->msgsock = msgsock;
    msger->listener = listener;
    msger->send_queue_length = 0;
    msger->mtx = __ex_mutex_create();
    msger->peers = xtree_create();
    
    msger->sendqueue = (xmsgchannellist_ptr)malloc(sizeof(struct xmsgchannellist));
    msger->sendqueue->len = 0;
    msger->sendqueue->lock = false;
    msger->sendqueue->head.prev = NULL;
    msger->sendqueue->end.next = NULL;
    msger->sendqueue->head.next = &msger->sendqueue->end;
    msger->sendqueue->end.prev = &msger->sendqueue->head;

    msger->pipe = __ex_msg_pipe_create(2);
    __ex_check(msger->pipe != NULL);

    __ex_logi("xmessenger_create exit\n");

    return msger;

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
        __ex_mutex_broadcast(msger->mtx);
        __ex_task_free(&msger->mainloop_task);
        xline_maker_clear(msger->mainloop_func);
        xtree_clear(msger->peers, free_channel);
        xtree_free(&msger->peers);
        __ex_mutex_free(msger->mtx);
        if (msger->sendqueue){
            free(msger->sendqueue);
        }
        free(msger);
    }
    __ex_logi("xmessenger_free exit\n");
}

static inline void xmessenger_wait(xmessenger_ptr messenger)
{
    ___lock lk = __ex_mutex_lock(messenger->mtx);
    __ex_mutex_wait(messenger->mtx, lk);
    __ex_mutex_unlock(messenger->mtx, lk);
}

//TODO 增加一个用户上下文参数，断开连接时，可以直接先释放channel，然后在回调中给出用户上下文，通知用户回收相关资源。
//TODO 增加一个加密验证数据，messenger 只负责将消息发送到对端，连接能否建立，取决于对端验证结果。
static inline int xmessenger_connect(xmessenger_ptr messenger, xmsgaddr_ptr addr)
{
    __ex_logi("xmessenger_connect enter\n");
    xmaker_ptr ctx = __ex_msg_pipe_hold_writer(messenger->pipe);
    __ex_logi("xmessenger_connect ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    if (ctx == NULL){
        return -1;
    }
    xline_add_ptr(ctx, "addr", addr);
    // const char *key = "PING";
    // size_t len = strlen(key);
    // char output[len];
    // for (size_t i = 0; i < len; ++i) {
    //     output[i] = key[i] ^ 'k';
    // }
    // __ex_logd("str len %lu\n", len);
    // xline_add_text(ctx, "msg", output, len);
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
        struct xmaker kv;
        xline_maker_setup(&kv, unit->body, PACK_BODY_SIZE);
        xline_add_text(&kv, "msg", "PING", strlen("PING"));
        unit->head.pack_size = kv.wpos;
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