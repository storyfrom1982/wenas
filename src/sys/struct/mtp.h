#ifndef __MESSAGE_TRANSFER_PROTOCOL_H__
#define __MESSAGE_TRANSFER_PROTOCOL_H__


#include "env/env.h"
#include <env/task.h>
#include <env/malloc.h>
#include <sys/struct/heap.h>
#include <sys/struct/tree.h>


enum {
    TRANSUNIT_NONE = 0x00,
    TRANSUNIT_MSG = 0x01,
    TRANSUNIT_ACK = 0x02,
    TRANSUNIT_PING = 0x04,
    TRANSUNIT_PONG = 0x08
};

#define UNIT_HEAD_SIZE          8
#define UNIT_BODY_SIZE          1280
// #define UNIT_BODY_SIZE          8

#define UNIT_BUF_RANGE          16
#define UNIT_MSG_RANGE          8192

#define TRANSMSG_SIZE           ( UNIT_BODY_SIZE * UNIT_MSG_RANGE )
#define TRANSUNIT_SIZE          ( UNIT_BODY_SIZE + UNIT_HEAD_SIZE )


typedef struct msgchannel* msgchannel_ptr;
typedef struct msglistener* msglistener_ptr;
typedef struct msgtransport* msgtransport_ptr;

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
    uint8_t flag;
    uint16_t maximal;
    uint16_t serial_number;
    uint16_t body_size;
}*unithead_ptr;

typedef struct transunit {
    bool comfirmed;
    bool resending;
    struct heapnode ts;
    msgchannel_ptr channel;
    struct unithead head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;

typedef struct transunitbuf {
    uint16_t range;
    ___atom_size16bit upos, rpos, wpos;
    struct transunit *buf[1];
}*transunitbuf_ptr;


typedef struct transack {
    msgchannel_ptr channel;
    struct unithead head;
}*transack_ptr;

typedef struct transackbuf {
    uint16_t range;
    ___atom_size16bit upos, rpos, wpos;
    struct transack buf[1];
}*transackbuf_ptr;


typedef struct transmsg {
    msgchannel_ptr channel;
    size_t size;
    char data[1];
}*transmsg_ptr;


typedef struct transmsgbuf {
    transmsg_ptr msg;
    uint16_t max_serial_number;
    uint16_t range, upos, rpos, wpos;
    struct transunit *buf[1];
}*transmsgbuf_ptr;


struct msgchannel {
    msgchannel_ptr prev, next;
    bool sending;
    uint16_t interval;
    ___atom_bool connected;
    ___mutex_ptr mtx;
    heap_ptr timer;
    transmsgbuf_ptr msgbuf;
    struct msgaddr addr;
    transunitbuf_ptr sendbuf;
    msgtransport_ptr mtp;
};


typedef struct channelqueue {
    ___atom_size len;
    ___atom_bool lock;
    struct msgchannel head, end;
}*channelqueue_ptr;


struct msglistener {
    void *ctx;
    void (*connected)(struct msglistener*, msgchannel_ptr channel);
    void (*disconnected)(struct msglistener*, msgchannel_ptr channel);
    void (*message)(struct msglistener*, msgchannel_ptr channel, transmsg_ptr msg);
    void (*status)(struct msglistener*, msgchannel_ptr channel);
};


typedef struct physics_socket {
    void *ctx;
    void (*listening)(struct physics_socket *socket);
    size_t (*sendto)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      1000000000ULL

struct msgtransport {
    __tree peers;
    ___mutex_ptr mtx;
    ___atom_bool running;
    msglistener_ptr listener;
    physics_socket_ptr device;
    linekv_ptr send_func, recv_func;
    task_ptr send_task, recv_task;
    transackbuf_ptr ackbuf;
    ___atom_size send_queue_length;
    struct channelqueue channels[MSGCHANNELQUEUE_ARRAY_RANGE];
};



#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint16_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint16_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint16_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      (((b)->range - 1 - ((uint16_t)((b)->wpos - (b)->rpos))))



static inline void msgchannel_push(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->channel = channel;
    unit->resending = false;
    unit->comfirmed = false;
    unit->head.flag = 0;

    ___lock lk = ___mutex_lock(channel->mtx);

    while (__transbuf_writable(channel->sendbuf) == 0){
        ___mutex_wait(channel->mtx, lk);
    }

    unit->head.serial_number = channel->sendbuf->wpos;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    if (!channel->sending){
        channelqueue_ptr queue;
        for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; i = ((i+1) % MSGCHANNELQUEUE_ARRAY_RANGE))
        {
            queue = &channel->mtp->channels[i];
            if (!___atom_try_lock(&queue->lock)){
                continue;
            }
            channel->next = &queue->end;
            channel->prev = queue->end.prev;
            channel->prev->next = channel;
            channel->next->prev = channel;
            channel->sending = true;
            ___atom_add(&queue->len, 1);
            ___atom_unlock(&queue->lock);
            __logi("msgchannel_push inqueue channel: 0x%x", channel);
            break;
        }
    }

    ___mutex_unlock(channel->mtx, lk);

    if (___atom_add(&channel->mtp->send_queue_length, 1) == 1){
        ___lock lk = ___mutex_lock(channel->mtp->mtx);
        ___mutex_notify(channel->mtp->mtx);
        ___mutex_unlock(channel->mtp->mtx, lk);
    }
    
}

static inline void msgchannel_pull(msgchannel_ptr channel, transack_ptr ack)
{
    // 只处理在 rpos 与 upos 之间的 SN
    if ((uint16_t)(ack->head.serial_number - channel->sendbuf->rpos) <= (uint16_t)(channel->sendbuf->upos - channel->sendbuf->rpos)){

        uint16_t index;
        transunit_ptr unit;
        heapnode_ptr timenode;

        // __logi("msgchannel_pull recv serial SN: %u max: %u rpos: %u upos: %u wpos %u", 
        //     ack->head.serial_number & (channel->sendbuf->range - 1), ack->head.maximal & (channel->sendbuf->range - 1),
        //     __transbuf_rpos(channel->sendbuf), __transbuf_upos(channel->sendbuf), __transbuf_wpos(channel->sendbuf));

        if (ack->head.maximal == ack->head.serial_number){
            // 收到连续的 ACK，连续的最大序列号是 maximal
            while (channel->sendbuf->rpos != ack->head.maximal)
            {
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                unit = channel->sendbuf->buf[index];

                if (!unit->comfirmed){
                    // 移除定时器，设置确认状态
                    assert(channel->timer->array[unit->ts.pos] != NULL);
                    unit->comfirmed = true;
                    timenode = heap_delete(channel->timer, &unit->ts);
                    assert(timenode->value == unit);
                }

                // 释放内存
                free(unit);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                ___atom_add(&channel->sendbuf->rpos, 1);
                ___mutex_notify(channel->mtx);
            }

            assert(__transbuf_inuse(channel->sendbuf) > 0);
            index = __transbuf_rpos(channel->sendbuf);
            unit = channel->sendbuf->buf[index];

            if (!unit->comfirmed){
                assert(channel->timer->array[unit->ts.pos] != NULL);
                unit->comfirmed = true;
                timenode = heap_delete(channel->timer, &unit->ts);
                assert(timenode->value == unit);
            }

            free(unit);
            channel->sendbuf->buf[index] = NULL;

            ___atom_add(&channel->sendbuf->rpos, 1);
            ___mutex_notify(channel->mtx);

        } else {

            // __logi("msgchannel_pull recv interval SN: %u max: %u", ack->head.serial_number, ack->head.maximal);
            index = ack->head.serial_number & (UNIT_BUF_RANGE - 1);
            unit = channel->sendbuf->buf[index];

            // 检测此 SN 是否未确认
            if (unit && !unit->comfirmed){
                assert(channel->timer->array[unit->ts.pos] != NULL);
                // 移除定时器，设置确认状态
                unit->comfirmed = true;
                timenode = heap_delete(channel->timer, &unit->ts);
                assert(timenode->value == unit);
            }

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            index = channel->sendbuf->rpos;
            while (index != ack->head.serial_number) {
                unit = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                if (!unit->comfirmed && !unit->resending){
                    __logi("msgchannel_pull resend SN: %u", (index & (channel->sendbuf->range - 1)));
                    unit->head.flag = 1;
                    if (channel->mtp->device->sendto(channel->mtp->device, &channel->addr, 
                        (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size) == UNIT_HEAD_SIZE + unit->head.body_size){
                        unit->resending = true;
                    }
                }
                index++;
            }
        }
    }
}

static inline void msgtransport_push_ack(msgtransport_ptr mtp, transack_ptr ack)
{
    if (__transbuf_writable(mtp->ackbuf) == 0){
        ___lock lk = ___mutex_lock(mtp->mtx);
        ___mutex_notify(mtp->mtx);
        ___mutex_wait(mtp->mtx, lk);
        ___mutex_unlock(mtp->mtx, lk);
    }

    mtp->ackbuf->buf[__transbuf_wpos(mtp->ackbuf)] = *ack;
    ___atom_add(&mtp->ackbuf->wpos, 1);
    ___mutex_notify(mtp->mtx);
}

static inline void msgchannel_recv(msgchannel_ptr channel, transunit_ptr unit)
{
    struct transack ack;
    // 设置当前最大连续 SN，重复收到 SN 时直接回复默认设置的 ACK
    ack.channel = channel;
    ack.head = unit->head;

    uint16_t index = unit->head.serial_number & (UNIT_BUF_RANGE - 1);

    // __logi("msgchannel_recv SN: %u wpos: %u rpos: %u", index, __transbuf_wpos(channel->msgbuf), __transbuf_rpos(channel->msgbuf));
    
    if (unit->head.serial_number == channel->msgbuf->wpos){

        if (channel->msgbuf->buf[index] != NULL){
            assert(channel->msgbuf->buf[index] != NULL);
        }
        channel->msgbuf->buf[index] = unit;
        channel->msgbuf->wpos++;
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
            assert(__transbuf_writable(channel->msgbuf) > 0);
            channel->msgbuf->wpos++;
        }

        // 更新最大连续 SN
        ack.head.serial_number = channel->msgbuf->wpos - 1;
        ack.head.maximal = ack.head.serial_number;

        msgtransport_push_ack(channel->mtp, &ack);

        // 所有 SN 都已经是连续的，间隔计数器清零
        channel->interval = 0;

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint16_t)(channel->msgbuf->wpos - unit->head.serial_number) > (uint16_t)(unit->head.serial_number - channel->msgbuf->wpos)){

            ack.head.serial_number = unit->head.serial_number;

            // SN 在 wpos 方向越界，是提前到达的 MSG
            if (channel->msgbuf->buf[index] == NULL){
                // __logi("msgchannel_recv over range sn: %u wpos: %u", unit->head.serial_number, channel->msgbuf->wpos);
                channel->msgbuf->buf[index] = unit;
            }else {
                // __logi("msgchannel_recv over range --------------- wpos sencond %u", unit->head.serial_number);
                free(unit);
            }

            if (++channel->interval > 2){
                // 连续收到两个不连续 SN，则反馈 ACK，促使发送端重传缺失的 SN
                ack.head.maximal = channel->msgbuf->wpos - 1;
                msgtransport_push_ack(channel->mtp, &ack);
            }
            
        }else {
            // __logi("msgchannel_recv over range ++++++++++++++++ rpos %u", unit->head.serial_number);
            
            // SN 在 rpos 方向越界，是重复的 SN
            free(unit);

            ack.head.serial_number = channel->msgbuf->wpos - 1;
            ack.head.maximal = ack.head.serial_number;
            msgtransport_push_ack(channel->mtp, &ack);
        }
    }

    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->max_serial_number = channel->msgbuf->buf[index]->head.maximal;
            if (channel->msgbuf->max_serial_number == 0){
                channel->msgbuf->max_serial_number = UNIT_BUF_RANGE;
            }
            channel->msgbuf->msg = (transmsg_ptr)malloc(sizeof(struct transmsg) + (channel->msgbuf->max_serial_number * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }

        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.body_size;
        channel->msgbuf->max_serial_number--;
        if (channel->msgbuf->max_serial_number == 0){
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
            channel->mtp->listener->message(channel->mtp->listener, channel, channel->msgbuf->msg);
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
    channel->sending = false;
    channel->connected = false;
    channel->interval = 0;
    channel->mtp = mtp;
    channel->addr = *addr;
    channel->mtx = ___mutex_create();
    channel->msgbuf = (transmsgbuf_ptr) calloc(1, sizeof(struct transmsgbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->msgbuf->range = UNIT_BUF_RANGE;
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->sendbuf->range = UNIT_BUF_RANGE;
    channel->timer = heap_create(UNIT_BUF_RANGE);
    return channel;
}

static inline void msgchannel_release(msgchannel_ptr channel)
{
    while (channel->timer->pos > 0){
        __logi("msgchannel_release clear timer: %lu", channel->timer->pos);
        heapnode_ptr node = heap_pop(channel->timer);
    }
    heap_destroy(&channel->timer);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    ___mutex_release(channel->mtx);
}

static inline void msgtransport_recv_loop(linekv_ptr ctx)
{
    __logw("msgtransport_recv_loop enter");

    int result;
    struct msgaddr addr;
    struct transack ack;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    msgtransport_ptr mtp = (msgtransport_ptr)linekv_find_ptr(ctx, "ctx");

    addr.addr = malloc(256);

    while (___is_true(&mtp->running))
    {
        if (unit == NULL){
            unit = (transunit_ptr)malloc(sizeof(struct transunit));
            if (unit == NULL){
                __logi("msgtransport_recv_loop malloc failed");
                exit(0);
            }
        }

        unit->head.type = 0;
        unit->head.body_size = 0;
        result = mtp->device->recvfrom(mtp->device, &addr, &unit->head, TRANSUNIT_SIZE);
        // __logw("msgtransport_recv_loop result %d: %u", result, unit->head.type);

        if (result == (unit->head.body_size + UNIT_HEAD_SIZE)){

            channel = (msgchannel_ptr)tree_find(mtp->peers, addr.key, addr.keylen);

            if (channel == NULL){
                if (unit->head.type == TRANSUNIT_PING){
                    channel = msgchannel_create(mtp, &addr);
                    __logi("msgtransport_recv_loop connect channel: 0x%x", channel);
                    tree_inseart(mtp->peers, addr.key, addr.keylen, channel);
                }else {
                    continue;
                }
            }

            switch (unit->head.type & 0x0F)
            {
            case TRANSUNIT_MSG:
                ack.channel = channel;
                ack.head = unit->head;
                msgchannel_recv(channel, unit);
                unit = NULL;
                break;
            case TRANSUNIT_ACK:
                ack.channel = channel;
                ack.head = unit->head;
                msgtransport_push_ack(mtp, &ack);
                break;
            case TRANSUNIT_PING:
                ack.channel = channel;
                ack.head = unit->head;
                msgchannel_recv(channel, unit);
                unit = NULL;
                break;
            case TRANSUNIT_PONG:
                ack.channel = channel;
                ack.head = unit->head;
                msgtransport_push_ack(mtp, &ack);
                break;
            default:
                __logi("msgtransport_recv_loop disconnect channel 0x%x", channel);
                tree_delete(mtp->peers, addr.key, addr.keylen);
                msgchannel_release(channel);
                channel = NULL;
            }

        }else {

            // __logw("msgtransport_recv_loop waitting enter");
            mtp->device->listening(mtp->device);
            // __logw("msgtransport_recv_loop waitting exit");

        }
    }

    if (unit != NULL){
        free(unit);
    }

    free(addr.addr);

    __logw("msgtransport_recv_loop exit");
}

static inline void msgtransport_send_loop(linekv_ptr ctx)
{
    __logw("msgtransport_send_loop enter");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000UL * 60UL;
    transack_ptr ack;
    heapnode_ptr timenode;
    transunit_ptr sendunit = NULL;
    msgchannel_ptr channel = NULL;
    channelqueue_ptr channelqueue = NULL;

    msgtransport_ptr mtp = (msgtransport_ptr)linekv_find_ptr(ctx, "ctx");

    while (___is_true(&mtp->running))
    {
        for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; ++i)
        {
            channelqueue = &mtp->channels[i];

            if (!___atom_try_lock(&channelqueue->lock)){
                continue;
            }

            if (channelqueue->len == 0){
                ___atom_unlock(&channelqueue->lock);
                continue;
            }

            channel = channelqueue->head.next;
            
            while (channel != &channelqueue->end)
            {
                if (__transbuf_usable(channel->sendbuf) > 0){

                    sendunit = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                    result = mtp->device->sendto(mtp->device, &channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                    if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                        channel->sendbuf->upos++;
                        ___mutex_notify(channel->mtx);
                        sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                        sendunit->ts.value = sendunit;
                        heap_push(channel->timer, &sendunit->ts);
                        ___atom_sub(&channel->mtp->send_queue_length, 1);
                    }

                }else {

                    ___lock lk = ___mutex_lock(channel->mtx);
                    if (channel->timer->pos == 0 && __transbuf_readable(channel->sendbuf) == 0){
                        if (channel->sending){
                            msgchannel_ptr tmp = channel;
                            __logi("msgtransport_send_loop dequeue channel 0x%x", channel);
                            channel->next->prev = channel->prev;
                            channel->prev->next = channel->next;
                            ___atom_sub(&channelqueue->len, 1);
                            channel->sending = false;
                            channel = channel->next;
                            ___mutex_unlock(tmp->mtx, lk);
                            continue;
                        }
                    }
                    ___mutex_unlock(channel->mtx, lk);
                }

                while (channel->timer->pos > 0)
                {
                    if ((timeout = __heap_min(channel->timer)->key - ___sys_clock()) > 0){
                        if (timer > timeout){
                            timer = timeout;
                        }
                        break;

                    }else {

                        __logw("msgtransport_send_loop timeout %u", sendunit->head.serial_number);

                        sendunit = (transunit_ptr)__heap_min(channel->timer)->value;
                        sendunit->head.flag = 1;
                        result = mtp->device->sendto(mtp->device, &sendunit->channel->addr, 
                                (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                        if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                            timenode = heap_pop(channel->timer);
                            sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->ts.value = sendunit;
                            heap_push(channel->timer, &sendunit->ts);
                        }else {
                            break;
                        }
                    }
                }

                channel = channel->next;
            }

            ___atom_unlock(&channelqueue->lock);
        }


        while (__transbuf_readable(mtp->ackbuf) > 0){

            ack = &mtp->ackbuf->buf[__transbuf_rpos(mtp->ackbuf)];

            switch (ack->head.type & 0x0F)
            {
            case TRANSUNIT_MSG:
                ack->head.type = TRANSUNIT_ACK;
                ack->head.body_size = 0;
                result = mtp->device->sendto(mtp->device, &ack->channel->addr, (void*)&(ack->head), UNIT_HEAD_SIZE);
                if (result == UNIT_HEAD_SIZE) {
                    ___atom_add(&mtp->ackbuf->rpos, 1);
                    ___mutex_notify(mtp->mtx);
                }
                break;
            case TRANSUNIT_ACK:
                msgchannel_pull(ack->channel, ack);
                ___atom_add(&mtp->ackbuf->rpos, 1);
                ___mutex_notify(mtp->mtx);
                break;
            case TRANSUNIT_PING:
                ack->head.type = TRANSUNIT_PONG;
                ack->head.body_size = 0;
                result = mtp->device->sendto(mtp->device, &ack->channel->addr, (void*)&(ack->head), UNIT_HEAD_SIZE);
                if (result == UNIT_HEAD_SIZE) {
                    ___atom_add(&mtp->ackbuf->rpos, 1);
                    ___mutex_notify(mtp->mtx);
                }
                break;
            case TRANSUNIT_PONG:
                msgchannel_pull(ack->channel, ack);
                ___atom_add(&mtp->ackbuf->rpos, 1);
                ___mutex_notify(mtp->mtx);
                break;
            default:
                __logw("msgtransport_send_loop TRANSUNIT_NONE %u", (ack->head.type & 0x0F));
                exit(0);
            }

        }

        if (mtp->send_queue_length == 0){
            ___lock lk = ___mutex_lock(mtp->mtx);
            ___mutex_notify(mtp->mtx);
            ___mutex_timer(mtp->mtx, lk, timer);
            ___mutex_unlock(mtp->mtx, lk);
            timer = 1000000000UL * 6UL;
        }        
    }

    __logi("msgtransport_send_loop exit");
}

static inline msgtransport_ptr msgtransport_create(physics_socket_ptr device, msglistener_ptr listener)
{
    msgtransport_ptr mtp = (msgtransport_ptr)calloc(1, sizeof(struct msgtransport));

    mtp->running = true;
    mtp->device = device;
    mtp->listener = listener;
    mtp->send_queue_length = 0;
    mtp->mtx = ___mutex_create();
    mtp->peers = tree_create();
    mtp->ackbuf = (transackbuf_ptr) calloc(1, sizeof(struct transackbuf) + sizeof(struct transack) * TRANSACK_BUFF_RANGE);
    mtp->ackbuf->range = TRANSACK_BUFF_RANGE;
    
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

    mtp->recv_func = linekv_create(1024);
    linekv_add_ptr(mtp->recv_func, "func", (void*)msgtransport_recv_loop);
    linekv_add_ptr(mtp->recv_func, "ctx", mtp);
    mtp->recv_task = task_create();
    task_post(mtp->recv_task, mtp->recv_func);

    mtp->send_func = linekv_create(1024);
    linekv_add_ptr(mtp->send_func, "func", (void*)msgtransport_send_loop);
    linekv_add_ptr(mtp->send_func, "ctx", mtp);
    mtp->send_task = task_create();
    task_post(mtp->send_task, mtp->send_func);

    return mtp;
}

static void free_channel(__ptr val)
{
    __logi(">>>>------------> free channel 0x%x", val);
    msgchannel_release((msgchannel_ptr)val);
}

static inline void msgtransport_release(msgtransport_ptr *pptr)
{
    __logi("msgtransport_release enter");
    if (pptr && *pptr){        
        msgtransport_ptr mtp = *pptr;
        *pptr = NULL;
        ___set_false(&mtp->running);
        ___mutex_broadcast(mtp->mtx);
        task_release(&mtp->send_task);
        task_release(&mtp->recv_task);
        linekv_release(&mtp->send_func);
        linekv_release(&mtp->recv_func);
        tree_clear(mtp->peers, free_channel);
        tree_release(&mtp->peers);
        ___mutex_release(mtp->mtx);
        free(mtp->ackbuf);
        free(mtp);
    }
    __logi("msgtransport_release exit");
}

static inline msgchannel_ptr msgtransport_connect(msgtransport_ptr mtp, msgaddr_ptr addr)
{
    __logi("msgtransport_connect enter");
    msgchannel_ptr channel = msgchannel_create(mtp, addr);
    tree_inseart(mtp->peers, addr->key, addr->keylen, channel);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    unit->head.type = TRANSUNIT_PING;
    unit->head.body_size = 0;
    unit->head.maximal = 1;
    msgchannel_push(channel, unit);
    ___lock lk = ___mutex_lock(channel->mtx);
    ___mutex_wait(channel->mtx, lk);
    ___mutex_unlock(channel->mtx, lk);
    __logi("msgtransport_connect exit");
    return channel;
}

static inline void msgtransport_disconnect(msgtransport_ptr mtp, msgchannel_ptr channel)
{
    __logi("msgtransport_disconnect enter");
    if (tree_delete(mtp->peers, channel->addr.key, channel->addr.keylen) == channel){
        msgchannel_release(channel);
    }
    __logi("msgtransport_disconnect exit");
}

static inline void msgtransport_send(msgtransport_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
{
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
            unit->head.maximal = UNIT_MSG_RANGE - y;
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
            unit->head.maximal = (unit_count + last_unit_id) - y;
            msgchannel_push(channel, unit);
        }

        if (last_unit_id){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (unit_count * UNIT_BODY_SIZE), last_unit_size);
            unit->head.type = TRANSUNIT_MSG;        
            unit->head.body_size = last_unit_size;
            unit->head.maximal = last_unit_id;
            msgchannel_push(channel, unit);
        }
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__