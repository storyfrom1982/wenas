#ifndef __MESSAGE_TRANSFER_PROTOCOL_H__
#define __MESSAGE_TRANSFER_PROTOCOL_H__


#include "env/env.h"
#include <env/task.h>
#include <env/malloc.h>
#include <sys/struct/heap.h>
#include <sys/struct/tree.h>

// #include <iostream>

enum {
    TRANSUNIT_NONE = 0x00,
    TRANSUNIT_MSG = 0x01,
    TRANSUNIT_ACK = 0x02,
    TRANSUNIT_PING = 0x04,
    TRANSUNIT_PONG = 0x08
};

#define UNIT_HEAD_SIZE          8
// #define UNIT_BODY_SIZE          1280
// #define UNIT_TOTAL_SIZE         1288
#define UNIT_BODY_SIZE          8
#define UNIT_TOTAL_SIZE         16
#define UNIT_GROUP_SIZE         256
#define UNIT_GROUP_BUF_SIZE     ( UNIT_BODY_SIZE * UNIT_GROUP_SIZE )


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
    bool sending;
    bool confirm;
    ___atom_bool resending;
    // uint64_t timestamp;
    struct heapnode timer;
    msgchannel_ptr channel;
    struct msgaddr addr;
    struct unithead head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;


typedef struct transunitbuf {
    ___atom_size16bit reading, rpos, wpos;
    struct transunit *buf[1];
}*transunitbuf_ptr;

#define __transunitbuf_write(b, u) \
            ((b)->buf[(b)->wpos & (UNIT_GROUP_SIZE - 1)]) = (u); \
            ___atom_add(&(b)->wpos, 1)

#define __transunitbuf_read(b, u) \
            (u) = ((b)->buf[(b)->rpos & (UNIT_GROUP_SIZE - 1)]); \
            ___atom_add(&(b)->rpos, 1)

#define __transunitbuf_wpos(b)          ((b)->wpos & (UNIT_GROUP_SIZE - 1))
#define __transunitbuf_rpos(b)          ((b)->rpos & (UNIT_GROUP_SIZE - 1))
#define __transunitbuf_reading(b)       ((b)->reading & (UNIT_GROUP_SIZE - 1))

#define __transunitbuf_writable(buf)        ((UNIT_GROUP_SIZE - (buf)->wpos + (buf)->rpos) & (UNIT_GROUP_SIZE - 1))
#define __transunitbuf_readable(buf)        ((buf)->wpos - (buf)->rpos)
#define __transunitbuf_readingable(buf)     ((buf)->wpos - (buf)->reading)


typedef struct message {
    msgchannel_ptr channel;
    size_t size;
    char data[1];
}*message_ptr;


typedef struct msgbuf {
    message_ptr msg;
    uint16_t max_serial_number;
    uint16_t rpos, wpos;
    struct transunit *buf[1];
}*msgbuf_ptr;


struct msgchannel {
    msgchannel_ptr prev, next;
    ___atom_bool connected;
    bool sending;
    ___mutex_ptr mtx;
    heap_ptr timer;
    msgbuf_ptr msgbuf;
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
    void (*message)(struct msglistener*, msgchannel_ptr channel, message_ptr msg);
    void (*status)(struct msglistener*, msgchannel_ptr channel);
};


typedef struct physics_socket {
    void *ctx;
    void (*listening)(struct physics_socket *socket);
    size_t (*sendto)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;


#define RESEND_INTERVAL                 1000000000ULL
#define CHANNEL_QUEUE_COUNT             3
#define TRANSMITTER_RECVBUF_LEN         4096

struct msgtransmitter {
    __tree peers;
    bool listening;
    ___atom_bool running;
    physics_socket_ptr device;
    msglistener_ptr listener;
    linekv_ptr send_func, recv_func;
    task_ptr send_task, recv_task;
    // ___mutex_ptr recvmtx;
    transunitbuf_ptr recvbuf;
    ___mutex_ptr sendmtx;
    ___atom_size send_queue_length;
    struct channelqueue channels[CHANNEL_QUEUE_COUNT];
};


static inline void msgchannel_push(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->channel = channel;
    unit->resending = false;
    unit->sending = false;
    unit->confirm = false;
    unit->head.flag = 0;

    ___lock lk = ___mutex_lock(channel->mtx);

    // __logi("channel_push_unit %llu wpos: %llu", (UNIT_GROUP_SIZE - (channel->sendbuf->wpos - channel->sendbuf->rpos)), channel->sendbuf->wpos + 0);
    while ((UNIT_GROUP_SIZE - ((uint16_t)(channel->sendbuf->wpos - channel->sendbuf->rpos))) == 0){
        // __logi("channel_push_unit ___mutex_wait enter");
        ___mutex_wait(channel->mtx, lk);
        // __logi("channel_push_unit ___mutex_wait exit");
    }
    // __logi("msgchannel_pull >>>>---------------> enter sn: %u rpos: %llu reading: %llu wpos: %llu", unit->head.serial_number, channel->sendbuf->rpos + 0, channel->sendbuf->reading + 0, channel->sendbuf->wpos + 0);
    unit->head.serial_number = channel->sendbuf->wpos;
    ___atom_add(&channel->sendbuf->wpos, 1);
    channel->sendbuf->buf[unit->head.serial_number & (UNIT_GROUP_SIZE - 1)] = unit;

    if (!channel->sending){
        channelqueue_ptr queue;
        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; i = ((i+1) % CHANNEL_QUEUE_COUNT))
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
        ___lock lk = ___mutex_lock(channel->transmitter->sendmtx);
        ___mutex_notify(channel->transmitter->sendmtx);
        ___mutex_unlock(channel->transmitter->sendmtx, lk);
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
    uint16_t index = serial_number & (UNIT_GROUP_SIZE - 1);
    
    // 如果索引位置是 NULL，那么索引位置一定已经处理过了，
    // 如果 ACK 携带的 SN 不等于索引位置的 SN，那么这个 SN 一定是重复发送的，而且之前发送的 SN 已经到达了，而且又已经写入了新的 SN
    // 不处理上述两种情况的 ACK
    if (channel->sendbuf->buf[index] && serial_number == channel->sendbuf->buf[index]->head.serial_number){

        // __logi("msgchannel_pull >>>>---------------> enter sn: %u SN: %u timer pos: %u", serial_number, channel->sendbuf->buf[index]->head.serial_number, channel->sendbuf->buf[index]->timer.pos);
        //只处理 非 NULL 和 SN 相等的 ACK
        if (channel->sendbuf->buf[index] && !channel->sendbuf->buf[index]->confirm){
            // 移除定时器，设置确认状态
            channel->sendbuf->buf[index]->confirm = true;
            timenode = heap_delete(channel->timer, &channel->sendbuf->buf[index]->timer);
            if ((transunit_ptr)timenode->value != channel->sendbuf->buf[index]){
                __logi("msgchannel_pull timer pos: %llu-%llu value: 0x%x unit: 0x%x number: %u", timenode->pos, unit->timer.pos, timenode->value, unit, unit->head.serial_number);
            }
        }

        if (serial_number == channel->sendbuf->rpos){
            // 收到索引位置连续的 SN
            index = channel->sendbuf->rpos & (UNIT_GROUP_SIZE - 1);
            free(channel->sendbuf->buf[index]);
            channel->sendbuf->buf[index] = NULL;
            ___atom_add(&channel->sendbuf->rpos, 1);
            ___mutex_notify(channel->mtx);

            while ((uint16_t)(channel->sendbuf->reading - channel->sendbuf->rpos) > 0) {
                // 有已发送但尚未收到的 SN
                index = channel->sendbuf->rpos & (UNIT_GROUP_SIZE - 1);
                if (channel->sendbuf->buf[index]->confirm){
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
            // SN 一定在 rpos 后边，rpos 前面的 SN 不会到达这里
            uint16_t rpos = channel->sendbuf->rpos;
            while (rpos != serial_number) {
                // __logi("msgchannel_pull resend unit 1");
                unit = channel->sendbuf->buf[rpos & (UNIT_GROUP_SIZE - 1)];
                // __logi("msgchannel_pull resend unit 2");
                if (unit != NULL && !unit->confirm && !unit->resending){
                    // __logi("msgchannel_pull resend unit %hu", unit->head.serial_number);
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

static inline void channel_make_message(msgchannel_ptr channel, transunit_ptr unit)
{
    uint16_t index = unit->head.serial_number & (UNIT_GROUP_SIZE - 1);
    
    if (unit->head.serial_number == channel->msgbuf->wpos){
        if (channel->msgbuf->buf[index] == NULL){
            channel->msgbuf->buf[index] = unit;
            channel->msgbuf->wpos++;
            while (channel->msgbuf->buf[channel->msgbuf->wpos & (UNIT_GROUP_SIZE - 1)] != NULL){
                channel->msgbuf->wpos++;
            }
        }else {
            free(unit);
        }
    }else {
        // SN 不等于 wpos
        if ((uint16_t)(unit->head.serial_number - channel->msgbuf->rpos) < (uint16_t)(channel->msgbuf->wpos - channel->msgbuf->rpos)){
            // SN 与 rpos 的间距小于 SN 与 wpos 的间距，SN 在 rpos 与 wpos 之间，SN 是重复的 MSG
            free(unit);
        }else {
            // SN 不在 rpos 与 wpos 之间
            if ((uint16_t)(channel->msgbuf->wpos - unit->head.serial_number) > (uint16_t)(unit->head.serial_number - channel->msgbuf->wpos)){
                // SN 在 wpos 方向越界，是提前到达的 MSG
                if (channel->msgbuf->buf[index] == NULL){
                    channel->msgbuf->buf[index] = unit;
                }else {
                    free(unit);
                }
            }else {
                // SN 在 rpos 方向越界，是重复的 MSG
                free(unit);
            }
        }
    }

    index = channel->msgbuf->rpos & (UNIT_GROUP_SIZE - 1);
    while (channel->msgbuf->buf[index] != NULL){
        // __logi("channel_make_message max: %hu rpos: %hu", channel->msgbuf->buf[channel->msgbuf->rpos]->head.maximal, channel->msgbuf->rpos);
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->max_serial_number = channel->msgbuf->buf[index]->head.maximal;
            if (channel->msgbuf->max_serial_number == 0){
                channel->msgbuf->max_serial_number = UNIT_GROUP_SIZE;
            }
            channel->msgbuf->msg = (message_ptr)malloc(sizeof(struct message) + (channel->msgbuf->max_serial_number * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }
        // __logi("channel_make_message copy size %u msg: %s", channel->msgbuf->buf[channel->msgbuf->rpos]->head.body_size, 
        //         channel->msgbuf->buf[channel->msgbuf->rpos]->body);
        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.body_size;
        channel->msgbuf->max_serial_number--;
        if (channel->msgbuf->max_serial_number == 0){
            __logi("channel_make_message msg size: %llu", channel->msgbuf->msg->size);
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
            channel->transmitter->listener->message(channel->transmitter->listener, channel, channel->msgbuf->msg);
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = channel->msgbuf->rpos & (UNIT_GROUP_SIZE - 1);
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransmitter_ptr transmitter, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->sending = false;
    channel->connected = false;
    channel->transmitter = transmitter;
    channel->addr = *addr;
    channel->msgbuf = (msgbuf_ptr) calloc(1, sizeof(struct msgbuf) + sizeof(transunit_ptr) * UNIT_GROUP_SIZE);
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * UNIT_GROUP_SIZE);
    channel->timer = heap_create(UNIT_GROUP_SIZE);
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

    transunit_ptr unit = NULL;
    msgtransmitter_ptr transmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    while (___is_true(&transmitter->running))
    {
        if (transmitter->recvbuf->wpos - transmitter->recvbuf->rpos == 0){
            ___lock lk = ___mutex_lock(transmitter->sendmtx);
            if (transmitter->listening){
                transmitter->listening = false;
                ___mutex_unlock(transmitter->sendmtx, lk);
                // __logi("msgtransmitter_recv_loop listening enter");
                transmitter->device->listening(transmitter->device);
                ___mutex_notify(transmitter->sendmtx);
                // __logi("msgtransmitter_recv_loop listening exit");
            }else {
                // __logi("msgtransmitter_recv_loop waiting enter");
                ___mutex_notify(transmitter->sendmtx);
                ___mutex_wait(transmitter->sendmtx, lk);
                // __logi("msgtransmitter_recv_loop waiting exit");
                ___mutex_unlock(transmitter->sendmtx, lk);
            }
        }

        // __logw("msgtransmitter_recv_loop wpos %u: rpos: %u", transmitter->recvbuf->wpos + 0, transmitter->recvbuf->rpos + 0);

        if ((uint16_t)(transmitter->recvbuf->wpos - transmitter->recvbuf->rpos) > 0){
            unit = transmitter->recvbuf->buf[transmitter->recvbuf->rpos & (TRANSMITTER_RECVBUF_LEN - 1)];
            // __logi("msgtransmitter_recv_loop ip: %d port: %u", unit->addr.ip, unit->addr.port);
            unit->channel = (msgchannel_ptr)tree_find(transmitter->peers, unit->addr.key, unit->addr.keylen);
            if (unit->channel == NULL){
                unit->channel = msgchannel_create(transmitter, &unit->addr);
                tree_inseart(transmitter->peers, unit->addr.key, unit->addr.keylen, unit->channel);
            }
            channel_make_message(unit->channel, unit);
            // free(unit);
            ___atom_add(&transmitter->recvbuf->rpos, 1);
            ___mutex_notify(transmitter->sendmtx);
        }
    }

    __logw("msgtransmitter_recv_loop exit");
}

static inline void msgtransmitter_send_loop(linekv_ptr ctx)
{
    __logw("msgtransmitter_send_loop enter");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000UL * 60UL;
    heapnode_ptr timenode;
    struct msgaddr addr;
    struct unithead ack;
    transunit_ptr sendunit = NULL;
    transunit_ptr recvunit = NULL;
    msgchannel_ptr channel = NULL;
    channelqueue_ptr channellist = NULL;

    msgtransmitter_ptr transmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    addr.addr = malloc(256);

    while (___is_true(&transmitter->running))
    {
        // __logi("msgtransmitter_send_loop running");

        // __logi("msgtransmitter_send_loop start sending");

        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; ++i)
        {
            // __logi("msgtransmitter_send_loop channel queue %lu", i);
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
                // __logi("msgtransmitter_send_loop channel_hold_reading_pos");
                if ((uint16_t)(channel->sendbuf->wpos - channel->sendbuf->reading) > 0){
                    sendunit = channel->sendbuf->buf[channel->sendbuf->reading & (UNIT_GROUP_SIZE - 1)];
                    if (sendunit->head.serial_number % 2){
                        result = transmitter->device->sendto(transmitter->device, &channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                    }else {
                        result = UNIT_HEAD_SIZE + sendunit->head.body_size;
                    }
                    
                    // __logi("msgtransmitter_send_loop send size %lu result size %lu", UNIT_HEAD_SIZE + unit->head.body_size, result);
                    if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                        channel->sendbuf->reading++;
                        ___mutex_notify(channel->mtx);
                        // ___atom_sub(&transmitter->send_queue_length, 1);
                        sendunit->timer.key = ___sys_clock() + RESEND_INTERVAL;
                        sendunit->timer.value = sendunit;
                        heap_push(channel->timer, &sendunit->timer);
                        sendunit->sending = true;
                        ___atom_sub(&channel->transmitter->send_queue_length, 1);
                        // __logi("msgtransmitter_send_loop channel->timer >>>>------------> %lu", channel->timer->pos);
                    }else {
                        __logi("msgtransmitter_send_loop send noblock");
                    }

                }else {
                    ___lock lk = ___mutex_lock(channel->mtx);
                    if (channel->timer->pos == 0 && channel->sendbuf->wpos - channel->sendbuf->rpos == 0){
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
                            sendunit->timer.key = ___sys_clock() + RESEND_INTERVAL;
                            sendunit->timer.value = sendunit;
                            heap_push(channel->timer, &sendunit->timer);
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

        while (___is_true(&transmitter->running))
        {
            if (recvunit == NULL){
                recvunit = (transunit_ptr)malloc(sizeof(struct transunit));
                if (recvunit == NULL){
                    __logi("msgtransmitter_send_loop malloc failed");
                    exit(0);
                }
            }

            recvunit->head.body_size = 0;
            result = transmitter->device->recvfrom(transmitter->device, &addr, &recvunit->head, UNIT_TOTAL_SIZE);

            if (result == (recvunit->head.body_size + UNIT_HEAD_SIZE)){
                // __logi("msgtransmitter_recv_loop ------- ip: %d port: %u", addr.ip, addr.port);
                switch (recvunit->head.type & 0x0F)
                {
                case TRANSUNIT_MSG:
                    // __logw("msgtransmitter_send_loop TRANSUNIT_MSG %hu", recvunit->head.serial_number);
                    ack = recvunit->head;
                    ack.type = TRANSUNIT_ACK;
                    ack.body_size = 0;
                    while (transmitter->device->sendto(transmitter->device, &addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                        __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    }                    
                    // if (recvunit->head.serial_number % 5){
                    //     // __logw("msgtransmitter_send_loop TRANSUNIT_MSG send ACK %hu", recvunit->head.serial_number);
                    //     while (transmitter->device->sendto(transmitter->device, &addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                    //         __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    //     }
                    // }else {
                    //     if (recvunit->head.flag == 1){
                    //         __logw("msgtransmitter_send_loop TRANSUNIT_MSG resend ACK %hu", recvunit->head.serial_number);
                    //         while (transmitter->device->sendto(transmitter->device, &addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                    //             __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    //         }
                    //     }else {
                    //         __logw("msgtransmitter_send_loop TRANSUNIT_MSG miss ACK %hu", recvunit->head.serial_number);
                    //     }
                    // }
                    recvunit->addr = addr;
                    if ((TRANSMITTER_RECVBUF_LEN - (transmitter->recvbuf->wpos - transmitter->recvbuf->rpos)) == 0){
                        ___lock lk = ___mutex_lock(transmitter->sendmtx);
                        ___mutex_wait(transmitter->sendmtx, lk);
                        ___mutex_unlock(transmitter->sendmtx, lk);
                    }
                    transmitter->recvbuf->buf[transmitter->recvbuf->wpos & (TRANSMITTER_RECVBUF_LEN - 1)] = recvunit;
                    ___atom_add(&transmitter->recvbuf->wpos, 1);
                    ___mutex_notify(transmitter->sendmtx);
                    recvunit = NULL;
                    break;
                case TRANSUNIT_ACK:
                    // __loge("msgtransmitter_send_loop TRANSUNIT_ACK %hu", recvunit->head.serial_number);
                    channel = (msgchannel_ptr)tree_find(transmitter->peers, addr.key, addr.keylen);
                    if (channel){
                        msgchannel_pull(channel, recvunit->head.serial_number);
                    }
                    break;
                case TRANSUNIT_PING:
                    __logw("msgtransmitter_send_loop TRANSUNIT_PING");
                    ack = recvunit->head;
                    ack.type = TRANSUNIT_PONG;
                    ack.body_size = 0;
                    while (transmitter->device->sendto(transmitter->device, &addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                        __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    }

                    recvunit->addr = addr;
                    if ((TRANSMITTER_RECVBUF_LEN - (transmitter->recvbuf->wpos - transmitter->recvbuf->rpos)) == 0){
                        ___lock lk = ___mutex_lock(transmitter->sendmtx);
                        ___mutex_wait(transmitter->sendmtx, lk);
                        ___mutex_unlock(transmitter->sendmtx, lk);
                    }
                    transmitter->recvbuf->buf[transmitter->recvbuf->wpos & (TRANSMITTER_RECVBUF_LEN - 1)] = recvunit;
                    ___atom_add(&transmitter->recvbuf->wpos, 1);
                    ___mutex_notify(transmitter->sendmtx);
                    recvunit = NULL;
                    break;
                case TRANSUNIT_PONG:
                    __logw("msgtransmitter_send_loop TRANSUNIT_PONG");
                    channel = (msgchannel_ptr)tree_find(transmitter->peers, addr.key, addr.keylen);
                    if (channel){
                        msgchannel_pull(channel, recvunit->head.serial_number);
                    }
                    break;
                default:
                    channel = (msgchannel_ptr)tree_find(transmitter->peers, recvunit->addr.key, recvunit->addr.keylen);
                    if (channel){
                        msgchannel_release(channel);
                    }else {

                    }
                    break;
                }

            }else {

                if (transmitter->send_queue_length == 0){
                    ___lock lk = ___mutex_lock(transmitter->sendmtx);
                    transmitter->listening = true;
                    ___mutex_notify(transmitter->sendmtx);
                    // __logi("msgtransmitter_send_loop send_queue_length: %llu", transmitter->send_queue_length + 0);
                    // __logi("msgtransmitter_send_loop waitting enter %lu", timer);
                    ___mutex_timer(transmitter->sendmtx, lk, timer);
                    ___mutex_unlock(transmitter->sendmtx, lk);
                    timer = 1000000000UL * 6UL;
                    // __logi("msgtransmitter_send_loop waitting exit %lu", timer);
                }

                break;
            }
        }
    }

    if (recvunit != NULL){
        free(recvunit);
    }

    free(addr.addr);

    __logi("msgtransmitter_send_loop exit");
}

static inline msgtransmitter_ptr msgtransmitter_create(physics_socket_ptr device, msglistener_ptr listener)
{
    msgtransmitter_ptr mtp = (msgtransmitter_ptr)calloc(1, sizeof(struct msgtransmitter));

    mtp->running = true;
    mtp->listening = true;
    mtp->device = device;
    mtp->listener = listener;
    mtp->send_queue_length = 0;
    mtp->peers = tree_create();
    mtp->sendmtx = ___mutex_create();
    // mtp->recvmtx = ___mutex_create();
    mtp->recvbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * TRANSMITTER_RECVBUF_LEN);
    
    for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; i++)
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
        ___mutex_broadcast(transmitter->sendmtx);
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
        ___mutex_release(transmitter->sendmtx);
        // ___mutex_release(transmitter->recvmtx);
        free(transmitter->recvbuf);
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
    char *group_data;
    size_t group_size;
    size_t group_count = size / (UNIT_GROUP_BUF_SIZE);
    size_t last_group_size = size - (group_count * UNIT_GROUP_BUF_SIZE);
    // __logi("size: %llu last_group_size: %llu group_count: %llu", size, last_group_size, group_count);
    uint32_t unit_count;
    uint8_t last_unit_size;
    uint8_t last_unit_id;

    for (int x = 0; x < group_count; ++x){
        group_size = UNIT_GROUP_BUF_SIZE;
        group_data = ((char*)data) + x * UNIT_GROUP_BUF_SIZE;
        for (size_t y = 0; y < UNIT_GROUP_SIZE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.maximal = UNIT_GROUP_SIZE - y;
            msgchannel_push(channel, unit);
        }
    }

    if (last_group_size){
        group_data = ((char*)data) + (size - last_group_size);
        group_size = last_group_size;

        unit_count = group_size / UNIT_BODY_SIZE;
        last_unit_size = group_size - unit_count * UNIT_BODY_SIZE;
        last_unit_id = 0;
        // __logi("last_unit_size: %llu unit_count: %llu", last_unit_size, unit_count);

        if (last_unit_size > 0){
            last_unit_id = 1;
        }

        for (size_t y = 0; y < unit_count; y++){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.maximal = (unit_count + last_unit_id) - y;
            msgchannel_push(channel, unit);
        }

        if (last_unit_id){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, group_data + (unit_count * UNIT_BODY_SIZE), last_unit_size);
            unit->head.type = TRANSUNIT_MSG;        
            unit->head.body_size = last_unit_size;
            unit->head.maximal = last_unit_id;
            // __logi("msgtransmitter_send unit addr: 0x%x size: %u type: %u msg: %s", unit, unit->head.body_size, unit->head.type, unit->body);
            msgchannel_push(channel, unit);
        }
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__