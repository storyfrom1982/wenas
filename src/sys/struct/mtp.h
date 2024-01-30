#ifndef __MESSAGE_TRANSPORT_H__
#define __MESSAGE_TRANSPORT_H__


#include <ex/ex.h>
#include <sys/struct/xheap.h>
#include <sys/struct/xtree.h>
#include <sys/struct/xline.h>


enum {
    //TRANSUNIT_BYE = 0x00,
    TRANSUNIT_NONE = 0x00,
    TRANSUNIT_MSG = 0x01,
    TRANSUNIT_ACK = 0x02,
    TRANSUNIT_PING = 0x04,
    //TRANSUNIT_PING = 0x04,
    TRANSUNIT_HELLO = 0x08,
    TRANSUNIT_BYE = 0x10,
    TRANSUNIT_UNKNOWN = 0x20,
    TRANSUNIT_FINAL
};

#define UNIT_HEAD_SIZE          8
#define UNIT_BODY_SIZE          1280 // 1024 + 256
// #define UNIT_BODY_SIZE          8

#define UNIT_BUF_RANGE          16
// #define PACK_BUF_RANGE          16
#define UNIT_MSG_RANGE          8192 // 1K*8K=8M 0.4K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
// #define PACK_MSG_RANGE

#define TRANSMSG_SIZE           ( UNIT_BODY_SIZE * UNIT_MSG_RANGE )
#define TRANSUNIT_SIZE          ( UNIT_BODY_SIZE + UNIT_HEAD_SIZE )


typedef struct msgchannel* msgchannel_ptr;
typedef struct msglistener* msglistener_ptr;
typedef struct msgtransport* msgtransport_ptr;
typedef struct channelqueue* channelqueue_ptr;

typedef struct msgaddr {
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
}*msgaddr_ptr;

typedef struct unithead {
    uint8_t type;
    uint8_t sn; //serial number 包序列号
    uint8_t ack_sn; //ack 单个包的 ACK
    uint8_t ack_range; //acks 这个包和它之前的所有包的 ACK
    //uint8_t resend; //这个包的重传计数
    uint16_t msg_range; // message fragment count 消息分段数量
    uint16_t body_size; // message fragment length 这个分段的长度
}*unithead_ptr;

typedef struct transunit {
    bool comfirmed;
    uint8_t resending;
    struct xheapnode ts;
    // uint32_t timestamp; //计算往返耗时
    msgchannel_ptr channel;
    struct unithead head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;

typedef struct transunitbuf {
    uint8_t range;
    ___atom_8bit upos, rpos, wpos;
    struct transunit *buf[1];
}*transunitbuf_ptr;


typedef struct transmsg {
    msgchannel_ptr channel;
    size_t size;
    char data[1];
}*transmsg_ptr;


typedef struct transmsgbuf {
    transmsg_ptr msg;
    uint16_t msg_range;
    uint8_t range, upos, rpos, wpos;
    struct transunit *buf[1];
}*transmsgbuf_ptr;


struct msgchannel {
    msgchannel_ptr prev, next;
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
    struct unithead ack;
    transmsgbuf_ptr msgbuf;
    channelqueue_ptr queue;
    struct msgaddr addr;
    transunitbuf_ptr sendbuf;
    msgtransport_ptr mtp;
};

//channellist
typedef struct channelqueue {
    ___atom_size len;
    ___atom_bool lock;
    struct msgchannel head, end;
}*channelqueue_ptr;


struct msglistener {
    void *ctx;
    void (*onConnectionToPeer)(struct msglistener*, msgchannel_ptr channel);
    void (*onConnectionFromPeer)(struct msglistener*, msgchannel_ptr channel);
    void (*onDisconnection)(struct msglistener*, msgchannel_ptr channel);
    void (*onMessage)(struct msglistener*, msgchannel_ptr channel, transmsg_ptr msg);
    //没有数据接收或发送，通知用户监听网络接口，有数据可读时，通知messenger
    void (*onIdle)(struct msglistener*, msgchannel_ptr channel);
    void (*onSendable)(struct msglistener*, msgchannel_ptr channel);
    void (*connection)(struct msglistener*, msgchannel_ptr channel);
    void (*disconnection)(struct msglistener*, msgchannel_ptr channel);
    void (*message)(struct msglistener*, msgchannel_ptr channel, transmsg_ptr msg);
    void (*timeout)(struct msglistener*, msgchannel_ptr channel);
};


typedef struct physics_socket {
    void *ctx;
    //不需要内部监听，监听网络接口由用户层的线程处理
    void (*listening)(struct physics_socket *socket);
    size_t (*sendto)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      1000000000ULL

struct msgtransport {
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __ex_lock_ptr mtx;
    ___atom_bool running;
    //socket可读或者有数据待发送时为true
    ___atom_bool working;
    msglistener_ptr listener;
    physics_socket_ptr device;
    xline_maker_ptr mainloop_func;
    __ex_task_ptr mainloop_task;
    ___atom_size send_queue_length;
    channelqueue_ptr connectionqueue, sendqueue, disconnctionqueue;
    struct channelqueue channels[MSGCHANNELQUEUE_ARRAY_RANGE];
};


#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - 1 - (b)->wpos + (b)->rpos))



static inline void msgchannel_enqueue(msgchannel_ptr channel, channelqueue_ptr queue)
{
    ___atom_lock(&queue->lock);
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    channel->queue = queue;
    ___atom_add(&queue->len, 1);
    ___atom_unlock(&queue->lock);
}

static inline msgchannel_ptr msgchannel_dequeue(channelqueue_ptr queue)
{
    if (queue->len > 0){
        if (___atom_try_lock(&queue->lock)){
            msgchannel_ptr channel = queue->head.next;
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

static inline void msgchannel_clear(msgchannel_ptr channel)
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

static inline void msgchannel_push(msgchannel_ptr channel, transunit_ptr unit)
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

static const char* get_transunit_msg(transunit_ptr unit)
{
    struct xline_maker kv;
    xline_parse(&kv, unit->body);
    return xline_find(&kv, "msg");
}

static inline void msgchannel_pull(msgchannel_ptr channel, transunit_ptr ack)
{
    __ex_logi("msgchannel_pull >>>>>-------------------> range: %u sn: %u rpos: %u upos: %u wpos %u",
           ack->head.ack_range, ack->head.ack_sn, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 transunit
    if (__transbuf_inuse(channel->sendbuf) && ((uint8_t)(ack->head.ack_sn - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        uint8_t index;
        transunit_ptr unit;
        xheapnode_ptr timenode;

        if (ack->head.ack_sn == ack->head.ack_range){
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
                       ack->head.ack_range, ack->head.ack_sn, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0, get_transunit_msg(unit));

                if (!unit->comfirmed){
                    // 移除定时器，设置确认状态
                    assert(channel->timerheap->array[unit->ts.pos] != NULL);
                    unit->comfirmed = true;
                    timenode = xheap_remove(channel->timerheap, &unit->ts);
                    assert(timenode->value == unit);
                }

                if (unit->head.type == TRANSUNIT_HELLO){
                    if (___set_true(&channel->connected)){
                        //TODO 这里是主动发起连接 onConnectionToPeer
                        channel->mtp->listener->connection(channel->mtp->listener, channel);
                    }
                }else if (unit->head.type == TRANSUNIT_BYE){
                    //TODO 不会处理 BEY 的 ACK，BEY 在释放连接之前，会一直重传。
                    if (___set_true(&channel->disconnected)){
                        msgchannel_clear(channel);
                        channel->mtp->listener->disconnection(channel->mtp->listener, channel);
                    }
                }

                // 释放内存
                free(unit);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                ___atom_add(&channel->sendbuf->rpos, 1);
                __ex_notify(channel->mtx);

            } while ((uint8_t)(ack->head.ack_range - channel->sendbuf->rpos) <= 0);

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

            // __logi("msgchannel_pull recv interval ack_sn: %u ack_range: %u", ack->head.ack_sn, ack->head.ack_range);
            index = ack->head.ack_sn & (UNIT_BUF_RANGE - 1);
            unit = channel->sendbuf->buf[index];

            __ex_logi("msgchannel_pull >>>>>>---------->>> range: %u sn: %u rpos: %u upos: %u wpos %u msg: [%s]",
                   ack->head.ack_range, ack->head.ack_sn, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0, get_transunit_msg(unit));

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
                        if (channel->mtp->device->sendto(channel->mtp->device, &channel->addr, 
                            (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size) == UNIT_HEAD_SIZE + unit->head.body_size){
                            unit->resending++;
                        }
                    }
                    index++;
                }
            }
        }
    }
}

static inline void msgchannel_recv(msgchannel_ptr channel, transunit_ptr unit)
{
    channel->ack = unit->head;
    channel->ack.type = TRANSUNIT_ACK;
    channel->ack.body_size = 0;
    uint16_t index = unit->head.sn & (UNIT_BUF_RANGE - 1);
    
    __ex_logi("msgchannel_recv >>>>---------> SN: %u rpos: %u wpos: %u msg: [%s]",
           unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos, get_transunit_msg(unit));
    
    if (unit->head.sn == channel->msgbuf->wpos){

        // 更新最大连续 SN
        channel->ack.ack_sn = unit->head.sn;
        channel->ack.ack_range = channel->msgbuf->wpos;

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

            channel->ack.ack_sn = unit->head.sn;
            channel->ack.ack_range = channel->msgbuf->wpos - 1;

            // SN 在 wpos 方向越界，是提前到达的 MSG
            if (channel->msgbuf->buf[index] == NULL){
                // __logi("msgchannel_recv over range sn: %u wpos: %u", unit->head.serial_number, channel->msgbuf->wpos);
                channel->msgbuf->buf[index] = unit;
            }else {
                free(unit);
            }
            
        }else {
            // __logi("msgchannel_recv over range ++++++++++++++++ rpos %u", unit->head.serial_number);
            channel->ack.ack_sn = channel->msgbuf->wpos - 1;
            channel->ack.ack_range = channel->ack.ack_sn;
            // SN 在 rpos 方向越界，是重复的 SN
            free(unit);
        }
    }

//    __logi("msgchannel_recv sendto enter");
    if ((channel->mtp->device->sendto(channel->mtp->device, &channel->addr, (void*)&(channel->ack), UNIT_HEAD_SIZE)) == UNIT_HEAD_SIZE) {
//        __logi("msgchannel_recv sendto return ----------->");
        __ex_logi("msgchannel_recv ACK ---> SN: %u rpos: %u wpos: %u",
               unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);
        channel->ack.type = TRANSUNIT_NONE;
    }
//    __logi("msgchannel_recv sendto exit");

    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->msg_range = channel->msgbuf->buf[index]->head.msg_range;
            assert(channel->msgbuf->msg_range != 0 && channel->msgbuf->msg_range <= UNIT_MSG_RANGE);
            channel->msgbuf->msg = (transmsg_ptr)malloc(sizeof(struct transmsg) + (channel->msgbuf->msg_range * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }

        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.body_size;
        channel->msgbuf->msg_range--;
//        __logi("msgchannel_recv msg_range %u", channel->msgbuf->msg_range);
        if (channel->msgbuf->msg_range == 0){
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
//            __logi("msgchannel_recv process msg %s", channel->msgbuf->msg->data);
//            __logi("msgchannel_recv message enter");
            channel->mtp->listener->message(channel->mtp->listener, channel, channel->msgbuf->msg);
//            __logi("msgchannel_recv message exit");
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransport_ptr mtp, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->connected = false;
    channel->disconnected = false;
    channel->update = __ex_clock();
    channel->interval = 0;
    channel->mtp = mtp;
    channel->addr = *addr;
    channel->mtx = __ex_lock_create();
    channel->msgbuf = (transmsgbuf_ptr) calloc(1, sizeof(struct transmsgbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->msgbuf->range = UNIT_BUF_RANGE;
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->sendbuf->range = UNIT_BUF_RANGE;
    channel->timerheap = xheap_create(UNIT_BUF_RANGE);
    channel->queue = NULL;
    return channel;
}

static inline void msgchannel_release(msgchannel_ptr channel)
{
    __ex_logi("msgchannel_release enter");
    __ex_lock_free(channel->mtx);
    xheap_free(&channel->timerheap);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __ex_logi("msgchannel_release exit");
}

static inline void msgchannel_termination(msgchannel_ptr *pptr)
{
    __ex_logi("msgchannel_termination enter");
    if (pptr && *pptr){
        msgchannel_ptr channel = *pptr;
        *pptr = NULL;
        ___atom_lock(&channel->mtp->disconnctionqueue->lock);
        channel->next = &channel->mtp->disconnctionqueue->end;
        channel->prev = channel->mtp->disconnctionqueue->end.prev;
        channel->next->prev = channel;
        channel->prev->next = channel;
        ___atom_add(&channel->mtp->disconnctionqueue->len, 1);
        ___atom_unlock(&channel->mtp->disconnctionqueue->lock);
    }
    __ex_logi("msgchannel_termination exit");
}


//提供给用户一个从Idle状态唤醒主循环的接口
static inline void msgtransport_recv_loop(msgtransport_ptr mtp)
{
    __ex_logi("msgtransport_recv_loop enter");

    while (___is_true(&mtp->running))
    {
        mtp->device->listening(mtp->device);
//        __logi("msgtransport_recv_loop readable");
        if (___is_true(&mtp->running)){
            ___lock lk = __ex_lock(mtp->mtx);
            __ex_notify(mtp->mtx);
            __ex_wait(mtp->mtx, lk);
            __ex_unlock(mtp->mtx, lk);
        }
    }

    __ex_logi("msgtransport_recv_loop exit");
}

static inline void msgtransport_main_loop(xline_maker_ptr ctx)
{
    __ex_logi("msgtransport_main_loop enter");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000ULL;
    // transack_ptr ack;
    struct msgaddr addr;
    transunit_ptr recvunit = NULL;
    xheapnode_ptr timenode;
    transunit_ptr sendunit = NULL;
    msgchannel_ptr channel = NULL;
    msgchannel_ptr next = NULL;

    msgtransport_ptr mtp = (msgtransport_ptr)xline_find_ptr(ctx, "ctx");

    addr.addr = NULL;

    while (___is_true(&mtp->running))
    {

        if (recvunit == NULL){
            recvunit = (transunit_ptr)malloc(sizeof(struct transunit));
            if (recvunit == NULL){
                __ex_logi("msgtransport_recv_loop malloc failed");
                exit(0);
            }
        }

        recvunit->head.type = 0;
        recvunit->head.body_size = 0;
        result = mtp->device->recvfrom(mtp->device, &addr, &recvunit->head, TRANSUNIT_SIZE);

        if (result == (recvunit->head.body_size + UNIT_HEAD_SIZE)){

//            __logi("msgtransport_main_loop recv ip: %u port: %u msg: %s", addr.ip, addr.port, get_transunit_msg(recvunit));

            //TODO 先检测是否为 BEY。
            if (recvunit->head.type == TRANSUNIT_BYE){
                //如果当前状态已经是断开连接中，说明之前主动发送过 BEY。所以直接释放连接和相关资源。
                channel = (msgchannel_ptr)xtree_find(mtp->peers, addr.key, addr.keylen);
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
                        mtp->listener->disconnection(mtp->listener, channel);
                    }
                }else {
                    //回复的第二个 BEY 可能丢包了
                    //直接 sendto 回复 FINAL
                }

            }else if (recvunit->head.type == TRANSUNIT_FINAL){
                //被动方收到 FINAL，释放连接，结束超时重传。
                channel = (msgchannel_ptr)xtree_find(mtp->peers, addr.key, addr.keylen);
                if (channel){
                    mtp->listener->disconnection(mtp->listener, channel);
                }

            }else if (recvunit->head.type == TRANSUNIT_HELLO){

                channel = (msgchannel_ptr)xtree_find(mtp->peers, addr.key, addr.keylen);
                __ex_logi("msgtransport_main_loop TRANSUNIT_HELLO find channel: 0x%x ip: %u port: %u", channel, addr.ip, addr.port);
                if (channel != NULL){
                    //回复建立连接成功消息
                    //continue
                    //TODO 删除进行重连的逻辑
                    if (___set_true(&channel->disconnected)){
                        __ex_logi("msgtransport_main_loop reconnection channel clear: 0x%x ip: %u port: %u", channel, addr.ip, addr.port);
                        msgchannel_clear(channel);
                        mtp->listener->disconnection(mtp->listener, channel);
                    }
                }
                //定期向中心服务器更新密文，并记录中心服务器的更新时间戳。
                //在这里进行验证，首先检测（密文）的生成时间戳，确定使用当前的KEY解密，还是上一次的KEY解密。
                //解密失败，把所有非法连接挡在这里。
                //解密成功，创建连接。
                //回复建立连接成功消息。
                channel = msgchannel_create(mtp, &addr);
                __ex_logi("msgtransport_main_loop new connections channel: 0x%x ip: %u port: %u", channel, addr.ip, addr.port);
                msgchannel_enqueue(channel, mtp->sendqueue);
                xtree_save(mtp->peers, addr.key, addr.keylen, channel);
                ___set_true(&channel->connected);
                //TODO 这里是被动建立连接 onConnectionFromPeer
                channel->mtp->listener->connection(channel->mtp->listener, channel);

            }else {

                channel = (msgchannel_ptr)xtree_find(mtp->peers, addr.key, addr.keylen);
            }


            if (channel != NULL){

                channel->update = __ex_clock();

                if (recvunit->head.type == TRANSUNIT_ACK){
                    msgchannel_pull(channel, recvunit);
                }else if (/*recvunit->head.type == TRANSUNIT_MSG*/ recvunit->head.type > TRANSUNIT_NONE && recvunit->head.type < TRANSUNIT_UNKNOWN){
                    msgchannel_recv(channel, recvunit);
                    if (recvunit->head.type == TRANSUNIT_BYE){
                        //如果当前状态已经是断开连接中，说明之前主动发送过 BEY。所以直接释放连接和相关资源。
                        if (___set_true(&channel->disconnected)){
                            __ex_logi("msgtransport_main_loop disconnection channel: 0x%x ip: %u port: %u", channel, addr.ip, addr.port);
                            msgchannel_clear(channel);
                            mtp->listener->disconnection(mtp->listener, channel);
                        }else {
                            //如果当前不是断开连接中状态，说明是对方主动起 BEY，需要回复 ACK。
                            //等收到 BEY 的 ACK 再释放连接。
                        }
                    }
                    recvunit = NULL;
                }

            }else {
                //忽略非法连接
                __ex_logi("msgtransport_main_loop recv a invalid transunit: %u ip: %u port: %u", recvunit->head.type, addr.ip, addr.port);
            }



        }else {

             __ex_logi("msgtransport_main_loop send_queue_length %llu channel queue length %llu",
                   mtp->send_queue_length + 0, mtp->channels[0].len + mtp->channels[1].len + mtp->channels[2].len);
            //定时select，使用下一个重传定时器到期时间，如果定时器为空，最大10毫秒间隔。
            //主动创建连接，最多需要10毫秒。
            if (mtp->send_queue_length == 0 && (mtp->connectionqueue->len + mtp->disconnctionqueue->len) == 0){
                ___lock lk = __ex_lock(mtp->mtx);
                __ex_notify(mtp->mtx);
                __ex_timed_wait(mtp->mtx, lk, timer);
                __ex_unlock(mtp->mtx, lk);
                timer = 1000000000ULL * 10;
            }
        }


        //to peer channel 在外部线程中创建，在主循环中回收。
        channel = msgchannel_dequeue(mtp->connectionqueue);
        if (channel != NULL){
            xtree_save(mtp->peers, channel->addr.key, channel->addr.keylen, channel);
            msgchannel_enqueue(channel, mtp->sendqueue);
        }


        channel = mtp->sendqueue->head.next;
        while (channel != &mtp->sendqueue->end)
        {
            next = channel->next;

            //TODO 一次发送一个包，还是全部发送，还是指定每次发送数量？
            if (__transbuf_usable(channel->sendbuf) > 0){
                sendunit = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                //不要插手 pack 类型，这里只管发送。

                __ex_logi("msgtransport_main_loop sendto ip: %u port: %u channel: 0x%x SN: %u msg: %s",
                       channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));

                result = mtp->device->sendto(mtp->device, &channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                    channel->sendbuf->upos++;
                    //检测是否写阻塞，再执行唤醒。（因为只有缓冲区已满，才会写阻塞，所以无需原子锁定再进行检测，写阻塞会在下一次检测时被唤醒）
                    __ex_notify(channel->mtx);//TODO
                    //检测队列为空，才开启定时器
                    //所有channel统一使用一个定时器（还是每个channel各自维护定时器逻辑比较简单）。
                    //定时器的堆越小，操作效率越高。
                    sendunit->ts.key = __ex_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                    sendunit->ts.value = sendunit;
                    xheap_push(channel->timerheap, &sendunit->ts);
                    ___atom_sub(&mtp->send_queue_length, 1);
                    sendunit->resending = 0;
                }else {
                    sendunit->resending++;
                    //重新加入定时器，间隔递增。
                    if (sendunit->resending > 5){
                        //抛出断开连接事件，区分正常断开和超时断开。
                        __ex_logi("msgtransport_main_loop ***try again timeout*** ip: %u port: %u channel: 0x%x SN: %u msg: %s",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));

                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }

            }else {
                //TODO 检测 channel 空闲时间，如果连接超时，就断开连接。清理僵尸连接。
                //onDisconnect 回调要区分发送超时和连接超时
                if (channel->timerheap->pos == 0 && __transbuf_readable(channel->sendbuf) == 0){
                    if (__ex_clock() - channel->update > (1000000000ULL * 30)){
                        __ex_logi("msgtransport_main_loop ***timeout recycle*** ip: %u port: %u channel: 0x%x", channel->addr.ip, channel->addr.port, channel);
                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }
            }

            if (channel->timerheap->pos > 0){

                timeout = __heap_min(channel->timerheap)->key - __ex_clock();
//                __logi("msgtransport_main_loop timeout resend timestamp: %llu current: %llu timeout: %lld", sendunit->ts.key, __ex_clock(), timeout);
                if (timeout > 0){

                    if (timer > timeout){
                        timer = timeout;
                    }

                }else {

                    sendunit = (transunit_ptr)__heap_min(channel->timerheap)->value;
                    if (sendunit->resending < 5){
                        __ex_logi("msgtransport_main_loop resend ip: %u port: %u channel: 0x%x SN: %u msg: %s",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));
                        mtp->device->sendto(mtp->device, &sendunit->channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                        if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
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

                        __ex_logi("msgtransport_main_loop ***resend timeout*** ip: %u port: %u channel: 0x%x SN: %u msg: %s",
                               channel->addr.ip, channel->addr.port, channel, sendunit->head.sn, get_transunit_msg(sendunit));
                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }
            }

            //TODO 如果当前 channel 设置了待写入标志 与 缓冲区可写，则通知用户可写入（因为设置待写入标志时没有加锁，所以需要重复检测状态）。

            channel = next;
        }

        //TODO 不再需要断开连接队列，让用户收到 onDisconntion 事件后，调用 disconnect 方法，发送一个 BEY pack，主循环会释放 channel 的相关资源。
        if (mtp->disconnctionqueue->len > 0){
            if (___atom_try_lock(&mtp->disconnctionqueue->lock)){
                channel = mtp->disconnctionqueue->head.next;
                do {
                    __ex_logi("msgtransport_main_loop ----------------------------------------- delete channel 0x%x", channel);
                    channel->next->prev = channel->prev;
                    channel->prev->next = channel->next;
                    ___atom_sub(&mtp->disconnctionqueue->len, 1);
                    msgchannel_release(channel);
                    channel = mtp->disconnctionqueue->head.next;
                }while (channel != &mtp->disconnctionqueue->end);
                ___atom_unlock(&mtp->disconnctionqueue->lock);
            }
        }
    }


    if (recvunit != NULL){
        free(recvunit);
    }

    free(addr.addr);

    __ex_logi("msgtransport_main_loop exit");
}

static inline msgtransport_ptr msgtransport_create(physics_socket_ptr device, msglistener_ptr listener)
{
    __ex_logi("msgtransport_create enter");

    msgtransport_ptr mtp = (msgtransport_ptr)calloc(1, sizeof(struct msgtransport));

    mtp->running = true;
    mtp->device = device;
    mtp->listener = listener;
    mtp->send_queue_length = 0;
    mtp->mtx = __ex_lock_create();
    mtp->peers = xtree_create();
    
    for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; i++)
    {
        channelqueue_ptr queueptr = &mtp->channels[i];
        queueptr->len = 0;
        queueptr->lock = false;
        queueptr->head.prev = NULL;
        queueptr->end.next = NULL;
        queueptr->head.next = &queueptr->end;
        queueptr->end.prev = &queueptr->head;
    }

    mtp->connectionqueue = &mtp->channels[0];
    mtp->sendqueue = &mtp->channels[1];
    mtp->disconnctionqueue = &mtp->channels[2];

//    mtp->mainloop_func = linekv_create(1024);
//    linekv_add_ptr(mtp->mainloop_func, "func", (void*)msgtransport_main_loop);
//    linekv_add_ptr(mtp->mainloop_func, "ctx", mtp);
//    mtp->mainloop_task = taskqueue_create();
//    taskqueue_post(mtp->mainloop_task, mtp->mainloop_func);

    mtp->mainloop_task = __ex_task_run(msgtransport_main_loop, mtp);

    __ex_logi("msgtransport_create exit");

    return mtp;
}

static void free_channel(void *val)
{
    __ex_logi(">>>>------------> free channel 0x%x", val);
    msgchannel_release((msgchannel_ptr)val);
}

static inline void msgtransport_release(msgtransport_ptr *pptr)
{
    __ex_logi("msgtransport_release enter");
    if (pptr && *pptr){        
        msgtransport_ptr mtp = *pptr;
        *pptr = NULL;
        ___set_false(&mtp->running);
        __ex_broadcast(mtp->mtx);
        __ex_task_free(&mtp->mainloop_task);
        xline_maker_clear(&mtp->mainloop_func);
        xtree_clear(mtp->peers, free_channel);
        xtree_free(&mtp->peers);
        __ex_lock_free(mtp->mtx);
        free(mtp);
    }
    __ex_logi("msgtransport_release exit");
}

//TODO 增加一个用户上下文参数，断开连接时，可以直接先释放channel，然后在回调中给出用户上下文，通知用户回收相关资源。
//TODO 增加一个加密验证数据，messenger 只负责将消息发送到对端，连接能否建立，取决于对端验证结果。
static inline msgchannel_ptr msgtransport_connect(msgtransport_ptr mtp, msgaddr_ptr addr)
{
    __ex_logi("msgtransport_connect enter");
    msgchannel_ptr channel = msgchannel_create(mtp, addr);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    //TODO 以PING消息建立连接。
    // unit->head.type = TRANSUNIT_PING;
    unit->head.type = TRANSUNIT_HELLO;
    unit->head.msg_range = 1;

    struct xline_maker kv;
    xline_bind(&kv, unit->body, UNIT_BODY_SIZE);
    //TODO 加密数据，需要对端验证
    xline_add_text(&kv, "msg", "HELLO");
    unit->head.body_size = kv.pos;

    //TODO 先 push pack，再入队。
    msgchannel_enqueue(channel, mtp->connectionqueue);
    msgchannel_push(channel, unit);

    // ___lock lk = ___mutex_lock(channel->mtx);
    // ___mutex_wait(channel->mtx, lk);
    // ___mutex_unlock(channel->mtx, lk);
    // __logi("msgtransport_connect exit");
    __ex_logi("msgtransport_connect exit");
    return channel;
}

static inline void msgtransport_disconnect(msgtransport_ptr mtp, msgchannel_ptr channel)
{
    __ex_logi("msgtransport_disconnect enter");
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    unit->head.type = TRANSUNIT_BYE;
    unit->head.body_size = 0;
    unit->head.msg_range = 1;
    //主动发送 BEY，要设置 channel 主动状态。
    ___set_true(&channel->is_channel_breaker);
    //向对方发送 BEY，对方回应后，再移除 channel。
    ___set_true(&channel->disconnected);
    //主动断开的一端，发送第一个 BEY，并且启动超时重传。
    msgchannel_push(channel, unit);
    __ex_logi("msgtransport_disconnect exit");
}

//ping 需要发送加密验证，如果此时链接已经断开，ping 消息可以进行重连。
static inline void msgtransport_ping(msgchannel_ptr channel)
{
    __ex_logi("msgtransport_ping enter");
    if (__ex_clock() - channel->update >= 1000000000ULL * 10){
        transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
        unit->head.type = TRANSUNIT_PING;
        unit->head.msg_range = 1;
        struct xline_maker kv;
        xline_bind(&kv, unit->body, UNIT_BODY_SIZE);
        xline_add_text(&kv, "msg", "PING");
        unit->head.body_size = kv.pos;
        msgchannel_push(channel, unit);
    }
    __ex_logi("msgtransport_ping exit");
}

static inline void msgtransport_send(msgtransport_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
{
    // 非阻塞发送，当网络可发送时，通知用户层。
    char *msg_data;
    size_t msg_size;
    size_t msg_count = size / (TRANSMSG_SIZE);
    size_t last_msg_size = size - (msg_count * TRANSMSG_SIZE);

    uint16_t unit_count;
    uint16_t last_unit_size;
    uint16_t last_unit_id;

    for (int x = 0; x < msg_count; ++x){
        msg_size = TRANSMSG_SIZE;
        msg_data = ((char*)data) + x * TRANSMSG_SIZE;
        for (size_t y = 0; y < UNIT_MSG_RANGE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.msg_range = UNIT_MSG_RANGE - y;
            msgchannel_push(channel, unit);
        }
    }

    if (last_msg_size){
        msg_data = ((char*)data) + (size - last_msg_size);
        msg_size = last_msg_size;

        unit_count = msg_size / UNIT_BODY_SIZE;
        last_unit_size = msg_size - unit_count * UNIT_BODY_SIZE;
        last_unit_id = 0;

        if (last_unit_size > 0){
            last_unit_id = 1;
        }

        for (size_t y = 0; y < unit_count; y++){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.msg_range = (unit_count + last_unit_id) - y;
            msgchannel_push(channel, unit);
        }

        if (last_unit_id){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (unit_count * UNIT_BODY_SIZE), last_unit_size);
            unit->head.type = TRANSUNIT_MSG;        
            unit->head.body_size = last_unit_size;
            unit->head.msg_range = last_unit_id;
            msgchannel_push(channel, unit);
        }
    }
}


//make_channel(addr, 验证数据) //channel 在外部线程创建
//clear_channel(ChannelPtr) //在外部线程销毁


#endif //__MESSAGE_TRANSPORT_H__



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
