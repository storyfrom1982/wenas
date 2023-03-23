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
// #define UNIT_BODY_SIZE          1280
#define UNIT_BODY_SIZE          8
#define UNIT_BUFF_RANGE         256


#define TRANSMSG_SIZE           ( UNIT_BODY_SIZE * UNIT_BUFF_RANGE )
#define TRANSUNIT_SIZE          ( UNIT_BODY_SIZE + UNIT_HEAD_SIZE )


typedef struct msgchannel* msgchannel_ptr;
typedef struct msglistener* msglistener_ptr;
typedef struct msgtransmitter* msgtransmitter_ptr;

typedef struct msgaddr {
    void *addr;
    uint8_t addrlen;
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
    ___atom_bool connected;
    bool sending;
    ___mutex_ptr mtx;
    heap_ptr timer;
    transmsgbuf_ptr msgbuf;
    struct msgaddr addr;
    transunitbuf_ptr sendbuf;
    msgtransmitter_ptr transmitter;
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

struct msgtransmitter {
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
#define __transbuf_writable(b)      (((b)->range - ((uint16_t)((b)->wpos - (b)->rpos))))



static inline void msgchannel_push(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->channel = channel;
    unit->resending = false;
    unit->comfirmed = false;
    unit->head.flag = 0;

    ___lock lk = ___mutex_lock(channel->mtx);

    // __logi("channel_push_unit %llu wpos: %llu", (UNIT_GROUP_SIZE - (channel->sendbuf->wpos - channel->sendbuf->rpos)), channel->sendbuf->wpos + 0);
    while (__transbuf_writable(channel->sendbuf) == 0){
        // __logi("channel_push_unit ___mutex_wait enter");
        ___mutex_wait(channel->mtx, lk);
        // __logi("channel_push_unit ___mutex_wait exit");
    }
    // __logi("msgchannel_pull >>>>---------------> enter sn: %u rpos: %llu reading: %llu wpos: %llu", unit->head.serial_number, channel->sendbuf->rpos + 0, channel->sendbuf->reading + 0, channel->sendbuf->wpos + 0);
    unit->head.serial_number = channel->sendbuf->wpos;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    if (!channel->sending){
        channelqueue_ptr queue;
        for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; i = ((i+1) % MSGCHANNELQUEUE_ARRAY_RANGE))
        {
            queue = &channel->transmitter->channels[i];
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
            __logi("channel_push_unit add channel");
            break;
        }
    }

    ___mutex_unlock(channel->mtx, lk);

    if (___atom_add(&channel->transmitter->send_queue_length, 1) == 1){
        ___lock lk = ___mutex_lock(channel->transmitter->mtx);
        ___mutex_notify(channel->transmitter->mtx);
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }
    
}

static inline void msgchannel_pull(msgchannel_ptr channel, uint16_t serial_number)
{
    transunit_ptr unit;
    heapnode_ptr timenode;
    // __logi("msgchannel_pull >>>>---------------> enter sn: %u rpos: %u reading: %u wpos: %u", 
    //     serial_number, channel->sendbuf->rpos + 0, channel->sendbuf->reading + 0, channel->sendbuf->wpos + 0);
    // __logi("msgchannel_pull >>>>---------------> enter sn: %u rpos: %u reading: %u wpos: %u", 
    //     serial_number, channel->sendbuf->rpos & (UNIT_GROUP_SIZE - 1), channel->sendbuf->reading & (UNIT_GROUP_SIZE - 1), channel->sendbuf->wpos & (UNIT_GROUP_SIZE - 1));

    uint16_t index = serial_number & (UNIT_BUFF_RANGE - 1);

    // 只处理在 rpos 与 reading 之间的 SN
    if ((uint16_t)(serial_number - channel->sendbuf->rpos) <= (uint16_t)(channel->sendbuf->upos - channel->sendbuf->rpos)){
        // 检测此 SN 是否未确认
        if (channel->sendbuf->buf[index] && !channel->sendbuf->buf[index]->comfirmed){
            // if (channel->timer->array[channel->sendbuf->buf[index]->timer.pos] == NULL){
            //     __logi("msgchannel_pull h->array[smallest]->key != node->key");
            //     exit(0);
            // }
            // 移除定时器，设置确认状态
            channel->sendbuf->buf[index]->comfirmed = true;
            timenode = heap_delete(channel->timer, &channel->sendbuf->buf[index]->ts);
            if ((transunit_ptr)timenode->value != channel->sendbuf->buf[index]){
                __logi("msgchannel_pull timer pos: %llu-%llu value: 0x%x unit: 0x%x number: %u", timenode->pos, unit->ts.pos, timenode->value, unit, unit->head.serial_number);
            }
        }

        if (serial_number == channel->sendbuf->rpos){
            // 收到索引位置连续的 SN
            index = __transbuf_rpos(channel->sendbuf);
            free(channel->sendbuf->buf[index]);
            channel->sendbuf->buf[index] = NULL;
            ___atom_add(&channel->sendbuf->rpos, 1);
            ___mutex_notify(channel->mtx);

            while (__transbuf_inuse(channel->sendbuf) > 0) {
                // 有已发送但尚未收到的 SN
                index = __transbuf_rpos(channel->sendbuf);
                if (channel->sendbuf->buf[index] != NULL && channel->sendbuf->buf[index]->comfirmed){
                    // 有提前到达，已经确认过，而且索引连续的 SN
                    free(channel->sendbuf->buf[index]);
                    channel->sendbuf->buf[index] = NULL;
                    ___atom_add(&channel->sendbuf->rpos, 1);
                    ___mutex_notify(channel->mtx);
                }else {
                    // 没有连续的 SN，不再继续处理
                    break;
                }
            }

        }else {
            // __logi("msgchannel_pull resend unit enter");
            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            uint16_t rpos = channel->sendbuf->rpos;
            while (rpos != serial_number) {
                // __logi("msgchannel_pull resend unit rpos: %u sn: %u", rpos, serial_number);
                unit = channel->sendbuf->buf[rpos & (channel->sendbuf->range - 1)];
                if (unit != NULL && !unit->comfirmed && !unit->resending){
                    // __logi("msgchannel_pull resend unit sn: %hu", unit->head.serial_number);
                    unit->head.flag = 1;
                    if (channel->transmitter->device->sendto(channel->transmitter->device, &channel->addr, 
                        (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size) == UNIT_HEAD_SIZE + unit->head.body_size){
                        unit->resending = true;
                    }
                }
                rpos++;
            }
            // __logi("msgchannel_pull resend unit exit");
        }        
    }
    // __logi("channel_free_unit >>>>---------------> exit");
}

static inline void msgchannel_recv(msgchannel_ptr channel, transunit_ptr unit)
{
    uint16_t index = unit->head.serial_number & (UNIT_BUFF_RANGE - 1);
    
    if (unit->head.serial_number == channel->msgbuf->wpos){
        if (channel->msgbuf->buf[index] == NULL){
            channel->msgbuf->buf[index] = unit;
            channel->msgbuf->wpos++;
            while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
                channel->msgbuf->wpos++;
            }
        }else {
            __logi("msgchannel_recv receive");
            free(unit);
        }
    }else {
        // SN 不等于 wpos
        if ((uint16_t)(unit->head.serial_number - channel->msgbuf->rpos) < __transbuf_writable(channel->msgbuf)){
            // SN 与 rpos 的间距小于 SN 与 wpos 的间距，SN 在 rpos 与 wpos 之间，SN 是重复的 MSG
            __logi("msgchannel_recv resend %u", unit->head.serial_number);
            free(unit);
        }else {
            // SN 不在 rpos 与 wpos 之间
            if ((uint16_t)(channel->msgbuf->wpos - unit->head.serial_number) > (uint16_t)(unit->head.serial_number - channel->msgbuf->wpos)){
                // SN 在 wpos 方向越界，是提前到达的 MSG
                if (channel->msgbuf->buf[index] == NULL){
                    // __logi("msgchannel_recv over range wpos first %u", unit->head.serial_number);
                    channel->msgbuf->buf[index] = unit;
                }else {
                    __logi("msgchannel_recv over range wpos sencond %u", unit->head.serial_number);
                    free(unit);
                }
            }else {
                // SN 在 rpos 方向越界，是重复的 MSG
                // __logi("msgchannel_recv over range rpos %u", unit->head.serial_number);
                free(unit);
            }
        }
    }

    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        // __logi("msgchannel_recv max: %hu rpos: %hu", channel->msgbuf->buf[channel->msgbuf->rpos]->head.maximal, channel->msgbuf->rpos);
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->max_serial_number = channel->msgbuf->buf[index]->head.maximal;
            if (channel->msgbuf->max_serial_number == 0){
                channel->msgbuf->max_serial_number = UNIT_BUFF_RANGE;
            }
            channel->msgbuf->msg = (transmsg_ptr)malloc(sizeof(struct transmsg) + (channel->msgbuf->max_serial_number * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }
        // __logi("msgchannel_recv copy size %u msg: %s", channel->msgbuf->buf[channel->msgbuf->rpos]->head.body_size, 
        //         channel->msgbuf->buf[channel->msgbuf->rpos]->body);
        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.body_size;
        channel->msgbuf->max_serial_number--;
        if (channel->msgbuf->max_serial_number == 0){
            __logi("msgchannel_recv msg size: %llu", channel->msgbuf->msg->size);
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
            channel->transmitter->listener->message(channel->transmitter->listener, channel, channel->msgbuf->msg);
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransmitter_ptr transmitter, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->sending = false;
    channel->connected = false;
    channel->transmitter = transmitter;
    channel->addr = *addr;
    channel->msgbuf = (transmsgbuf_ptr) calloc(1, sizeof(struct transmsgbuf) + sizeof(transunit_ptr) * UNIT_BUFF_RANGE);
    channel->msgbuf->range = UNIT_BUFF_RANGE;
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * UNIT_BUFF_RANGE);
    channel->sendbuf->range = UNIT_BUFF_RANGE;
    channel->timer = heap_create(UNIT_BUFF_RANGE);
    channel->mtx = ___mutex_create();
    return channel;
}

static inline void msgchannel_release(msgchannel_ptr channel)
{
    ___mutex_release(channel->mtx);
    while (channel->timer->pos > 0){
        __logi("msgchannel_release heap_pop %lu", channel->timer->pos);
        heapnode_ptr node = heap_pop(channel->timer);
        free(node->value);
    }
    heap_destroy(&channel->timer);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
}

static inline void msgtransmitter_recv_loop(linekv_ptr ctx)
{
    __logw("msgtransmitter_recv_loop enter");

    int result;
    struct msgaddr addr;
    struct transack ack;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    msgtransmitter_ptr transmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    addr.addr = malloc(256);

    while (___is_true(&transmitter->running))
    {
        if (unit == NULL){
            unit = (transunit_ptr)malloc(sizeof(struct transunit));
            if (unit == NULL){
                __logi("msgtransmitter_recv_loop malloc failed");
                exit(0);
            }
        }

        unit->head.body_size = 0;
        result = transmitter->device->recvfrom(transmitter->device, &addr, &unit->head, TRANSUNIT_SIZE);

        if (result == (unit->head.body_size + UNIT_HEAD_SIZE)){
            // __logi("msgtransmitter_recv_loop addr: %lu", addr.port);
            // __logi("msgtransmitter_recv_loop ----- sn: %u", unit->head.serial_number);
            channel = (msgchannel_ptr)tree_find(transmitter->peers, addr.key, addr.keylen);
            if (channel == NULL){
                channel = msgchannel_create(transmitter, &addr);
                __logi("msgtransmitter_recv_loop peers: 0x%x", transmitter->peers);
                tree_inseart(transmitter->peers, addr.key, addr.keylen, channel);
            }

            switch (unit->head.type & 0x0F)
            {
            case TRANSUNIT_MSG:
                // __logw("msgtransmitter_recv_loop ------ TRANSUNIT_MSG");
                ack.channel = channel;
                ack.head = unit->head;
                msgchannel_recv(channel, unit);
                unit = NULL;
                break;
            case TRANSUNIT_ACK:
                // __logw("msgtransmitter_recv_loop ------ TRANSUNIT_ACK");
                ack.channel = channel;
                ack.head = unit->head;                
                break;
            case TRANSUNIT_PING:
                // __logw("msgtransmitter_recv_loop ------ TRANSUNIT_PING");
                ack.channel = channel;
                ack.head = unit->head;
                msgchannel_recv(channel, unit);
                unit = NULL;
                break;
            case TRANSUNIT_PONG:
                // __logw("msgtransmitter_recv_loop ------ TRANSUNIT_PONG");
                ack.channel = channel;
                ack.head = unit->head;
                break;
            default:
                // __logw("msgtransmitter_recv_loop ------ TRANSUNIT_NONE");
                tree_delete(transmitter->peers, addr.key, addr.keylen);
                msgchannel_release(channel);
                channel = NULL;
                exit(0);
            }

            if (channel){
                if (__transbuf_writable(transmitter->ackbuf) == 0){
                    ___lock lk = ___mutex_lock(transmitter->mtx);
                    ___mutex_notify(transmitter->mtx);
                    ___mutex_wait(transmitter->mtx, lk);
                    ___mutex_unlock(transmitter->mtx, lk);
                }
                if (ack.head.type == TRANSUNIT_MSG){
                    if ((ack.head.serial_number % 3) != 0){
                        transmitter->ackbuf->buf[__transbuf_wpos(transmitter->ackbuf)] = ack;
                        ___atom_add(&transmitter->ackbuf->wpos, 1);
                        ___mutex_notify(transmitter->mtx);
                    }else {
                        if (ack.head.flag == 1){
                            transmitter->ackbuf->buf[__transbuf_wpos(transmitter->ackbuf)] = ack;
                            ___atom_add(&transmitter->ackbuf->wpos, 1);
                            ___mutex_notify(transmitter->mtx);
                        }else {
                            // __logd("msgtransmitter_recv_loop miss TRANSUNIT_MSG %u", ack.head.serial_number);
                        }
                    }   
                }else 
                {
                    transmitter->ackbuf->buf[__transbuf_wpos(transmitter->ackbuf)] = ack;
                    ___atom_add(&transmitter->ackbuf->wpos, 1);
                    ___mutex_notify(transmitter->mtx);
                }
            }

        }else {
            // __logw("msgtransmitter_recv_loop ------ listening enter");
            transmitter->device->listening(transmitter->device);
            // __logw("msgtransmitter_recv_loop ------ listening exit");
        }

        // free(unit);
        // unit = NULL;
    }

    if (unit != NULL){
        free(unit);
    }

    free(addr.addr);

    __logw("msgtransmitter_recv_loop exit");
}

static inline void msgtransmitter_send_loop(linekv_ptr ctx)
{
    __logw("msgtransmitter_send_loop enter");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000UL * 60UL;
    transack_ptr ack;
    heapnode_ptr timenode;
    transunit_ptr sendunit = NULL;
    msgchannel_ptr channel = NULL;
    channelqueue_ptr channellist = NULL;

    msgtransmitter_ptr transmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    while (___is_true(&transmitter->running))
    {
        for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; ++i)
        {
            channellist = &transmitter->channels[i];

            if (!___atom_try_lock(&channellist->lock)){
                __logi("msgtransmitter_send_loop ___atom_try_lock");
                continue;
            }

            if (channellist->len == 0){
                ___atom_unlock(&channellist->lock);
                continue;
            }

            channel = channellist->head.next;

            // __logi("channel->timer->pos %llu", channel->timer->pos);
            // __logi("msgtransmitter_send_loop channellist->len %llu", transmitter->send_queue_length + 0);
            
            while (channel != &channellist->end)
            {
                if (__transbuf_usable(channel->sendbuf) > 0){
                    sendunit = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                    if (sendunit->head.serial_number % 2){
                        result = transmitter->device->sendto(transmitter->device, &channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                    }else {
                        result = UNIT_HEAD_SIZE + sendunit->head.body_size;
                    }
                    
                    // __logi("msgtransmitter_send_loop send size %lu result size %lu", UNIT_HEAD_SIZE + unit->head.body_size, result);
                    if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                        channel->sendbuf->upos++;
                        ___mutex_notify(channel->mtx);
                        // ___atom_sub(&transmitter->send_queue_length, 1);
                        sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                        sendunit->ts.value = sendunit;
                        heap_push(channel->timer, &sendunit->ts);
                        ___atom_sub(&channel->transmitter->send_queue_length, 1);
                        // __logi("msgtransmitter_send_loop channel->timer >>>>------------> %lu", channel->timer->pos);
                    }else {
                        __logi("msgtransmitter_send_loop send noblock");
                    }

                }else {
                    ___lock lk = ___mutex_lock(channel->mtx);
                    if (channel->timer->pos == 0 && __transbuf_readable(channel->sendbuf) == 0){
                        if (channel->sending){
                            msgchannel_ptr tmp = channel;
                            __logi("msgtransmitter_send_loop remove channel");
                            channel->next->prev = channel->prev;
                            // __logi("msgtransmitter_send_loop remove channel 1");
                            channel->prev->next = channel->next;
                            // __logi("msgtransmitter_send_loop remove channel 2");
                            ___atom_sub(&channellist->len, 1);
                            // __logi("msgtransmitter_send_loop remove channel 3");
                            channel->sending = false;
                            channel = channel->next;
                            ___mutex_unlock(tmp->mtx, lk);
                            // __logi("msgtransmitter_send_loop remove channel 4");
                            // __logi("msgtransmitter_send_loop remove channel 0x%x", channel);
                            continue;
                        }
                    }
                    ___mutex_unlock(channel->mtx, lk);
                }

                // __logi("msgtransmitter_send_loop channel->timer->pos: %llu readable: %hu", channel->timer->pos, channel->sendbuf->wpos - channel->sendbuf->rpos);
                // __logi("msgtransmitter_send_loop send_queue_length: %llu", transmitter->send_queue_length + 0);

                while (channel->timer->pos > 0)
                {
                    // __logi("msgtransmitter_send_loop channel->timer->pos: %llu key: %llu", channel->timer->pos, __heap_min(channel->timer)->key);
                    if ((timeout = __heap_min(channel->timer)->key - ___sys_clock()) > 0){
                        if (timer > timeout){
                            timer = timeout;
                        }
                        // __logi("msgtransmitter_send_loop --------- timer %llu  timeout: %llu", timer, timeout);
                        break;
                    }else {
                        
                        sendunit = (transunit_ptr)__heap_min(channel->timer)->value;
                        // __logi("msgtransmitter_send_loop >>>>>>------------------------------> resend sn: %u", sendunit->head.serial_number);
                        sendunit->head.flag = 1;
                        result = transmitter->device->sendto(transmitter->device, &sendunit->channel->addr, 
                                (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                        if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                            timenode = heap_pop(channel->timer);
                            sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->ts.value = sendunit;
                            heap_push(channel->timer, &sendunit->ts);
                        }else {
                            __logi("msgtransmitter_send_loop resend failed");
                            break;
                        }
                    }
                }

                channel = channel->next;
                // __logi("msgtransmitter_send_loop next channel %x", channel);
            }

            // __logi("msgtransmitter_send_loop channel queue unlock");
            ___atom_unlock(&channellist->lock);
        }

        while (__transbuf_readable(transmitter->ackbuf) > 0){

            ack = &transmitter->ackbuf->buf[__transbuf_rpos(transmitter->ackbuf)];

            switch (ack->head.type & 0x0F)
            {
            case TRANSUNIT_MSG:
                // __logw("msgtransmitter_send_loop TRANSUNIT_MSG");
                ack->head.type = TRANSUNIT_ACK;
                ack->head.body_size = 0;
                result = transmitter->device->sendto(transmitter->device, &ack->channel->addr, (void*)&(ack->head), UNIT_HEAD_SIZE);
                if (result == UNIT_HEAD_SIZE) {
                    ___atom_add(&transmitter->ackbuf->rpos, 1);
                    ___mutex_notify(transmitter->mtx);
                }else {
                    __logw("msgtransmitter_send_loop sendto failed");
                    // exit(0);
                }
                break;
            case TRANSUNIT_ACK:
                // __logw("msgtransmitter_send_loop TRANSUNIT_ACK");
                ___atom_add(&transmitter->ackbuf->rpos, 1);
                ___mutex_notify(transmitter->mtx);
                msgchannel_pull(ack->channel, ack->head.serial_number);
                break;
            case TRANSUNIT_PING:
                // __logw("msgtransmitter_send_loop TRANSUNIT_PING");
                ack->head.type = TRANSUNIT_PONG;
                ack->head.body_size = 0;
                result = transmitter->device->sendto(transmitter->device, &ack->channel->addr, (void*)&(ack->head), UNIT_HEAD_SIZE);
                if (result == UNIT_HEAD_SIZE) {
                    ___atom_add(&transmitter->ackbuf->rpos, 1);
                    ___mutex_notify(transmitter->mtx);
                }else {
                    __logw("msgtransmitter_send_loop sendto failed");
                    // exit(0);
                }                
                break;
            case TRANSUNIT_PONG:
                ___atom_add(&transmitter->ackbuf->rpos, 1);
                ___mutex_notify(transmitter->mtx);
                msgchannel_pull(ack->channel, ack->head.serial_number);
                break;
            default:
                __logw("msgtransmitter_send_loop TRANSUNIT_NONE %u", (ack->head.type & 0x0F));
                exit(0);
            }

        }

        if (transmitter->send_queue_length == 0){
            ___lock lk = ___mutex_lock(transmitter->mtx);
            ___mutex_notify(transmitter->mtx);
            // __logi("msgtransmitter_send_loop send_queue_length: %llu", transmitter->send_queue_length + 0);
            // __logw("msgtransmitter_send_loop ackbuf len %u", ((uint16_t)(transmitter->ackbuf->wpos - transmitter->ackbuf->rpos)));
            // __logi("msgtransmitter_send_loop waitting enter %lu", timer);
            ___mutex_timer(transmitter->mtx, lk, timer);
            ___mutex_unlock(transmitter->mtx, lk);
            timer = 1000000000UL * 6UL;
            // __logi("msgtransmitter_send_loop waitting exit %lu", timer);
        }        
    }

    __logi("msgtransmitter_send_loop exit");
}

static inline msgtransmitter_ptr msgtransmitter_create(physics_socket_ptr device, msglistener_ptr listener)
{
    msgtransmitter_ptr mtp = (msgtransmitter_ptr)calloc(1, sizeof(struct msgtransmitter));

    mtp->running = true;
    mtp->device = device;
    mtp->listener = listener;
    mtp->send_queue_length = 0;
    mtp->peers = tree_create();
    mtp->mtx = ___mutex_create();
    // mtp->recvmtx = ___mutex_create();
    // mtp->recvbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * TRANSACK_BUFF_RANGE);
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
    linekv_add_ptr(mtp->recv_func, "func", (void*)msgtransmitter_recv_loop);
    linekv_add_ptr(mtp->recv_func, "ctx", mtp);
    mtp->recv_task = task_create();
    task_post(mtp->recv_task, mtp->recv_func);

    mtp->send_func = linekv_create(1024);
    linekv_add_ptr(mtp->send_func, "func", (void*)msgtransmitter_send_loop);
    linekv_add_ptr(mtp->send_func, "ctx", mtp);
    mtp->send_task = task_create();
    task_post(mtp->send_task, mtp->send_func);

    return mtp;
}

static void free_channel(__ptr val)
{
    __logi("=======================================free_channel");
    msgchannel_release((msgchannel_ptr)val);
}

static inline void msgtransmitter_release(msgtransmitter_ptr *pptr)
{
    if (pptr && *pptr){        
        msgtransmitter_ptr transmitter = *pptr;
        *pptr = NULL;
        ___set_false(&transmitter->running);
        ___mutex_broadcast(transmitter->mtx);
        __logi("task_release send");
        task_release(&transmitter->send_task);
        __logi("task_release recv");
        task_release(&transmitter->recv_task);
        __logi("tree_clear");
        tree_clear(transmitter->peers, free_channel);
        // msgchannel_ptr channel = (msgchannel_ptr)tree_min(transmitter->peers);
        // channel = (msgchannel_ptr)tree_max(transmitter->peers);
        // __logi("tree_clear channel 0x%x", channel);
        // if (channel){
        //     tree_delete(transmitter->peers, channel->addr.key, channel->addr.keylen);
        //     msgchannel_release(channel);
        // }
        __logi("tree_release");
        tree_release(&transmitter->peers);
        linekv_release(&transmitter->send_func);
        linekv_release(&transmitter->recv_func);
        ___mutex_release(transmitter->mtx);
        // ___mutex_release(transmitter->recvmtx);
        free(transmitter->ackbuf);
        free(transmitter);
    }
}

static inline msgchannel_ptr msgtransmitter_connect(msgtransmitter_ptr transmitter, msgaddr_ptr addr)
{
    __logi("msgtransmitter_connect enter");
    msgchannel_ptr channel = msgchannel_create(transmitter, addr);
    tree_inseart(transmitter->peers, addr->key, addr->keylen, channel);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    unit->head.type = TRANSUNIT_PING;
    unit->head.body_size = 0;
    unit->head.maximal = 1;
    msgchannel_push(channel, unit);
    ___lock lk = ___mutex_lock(channel->mtx);
    ___mutex_wait(channel->mtx, lk);
    ___mutex_unlock(channel->mtx, lk);
    __logi("msgtransmitter_connect exit");
    return channel;
}

static inline void msgtransmitter_disconnect(msgtransmitter_ptr transmitter, msgchannel_ptr channel)
{
    if (tree_delete(transmitter->peers, channel->addr.key, channel->addr.keylen) == channel){
        msgchannel_release(channel);
    }
}

static inline void msgtransmitter_send(msgtransmitter_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
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
        for (size_t y = 0; y < UNIT_BUFF_RANGE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.maximal = UNIT_BUFF_RANGE - y;
            msgchannel_push(channel, unit);
        }
    }

    if (last_msg_size){
        msg_data = ((char*)data) + (size - last_msg_size);
        msg_size = last_msg_size;

        unit_count = msg_size / UNIT_BODY_SIZE;
        last_unit_size = msg_size - unit_count * UNIT_BODY_SIZE;
        last_unit_id = 0;
        // __logi("last_unit_size: %llu unit_count: %llu", last_unit_size, unit_count);

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
            // __logi("msgtransmitter_send unit addr: 0x%x size: %u type: %u msg: %s", unit, unit->head.body_size, unit->head.type, unit->body);
            msgchannel_push(channel, unit);
        }
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__