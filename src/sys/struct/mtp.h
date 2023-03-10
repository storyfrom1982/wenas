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
#define UNIT_BODY_SIZE          1400
#define UNIT_TOTAL_SIZE         1408
#define UNIT_GROUP_SIZE         256
#define UNIT_GROUP_BUF_SIZE     ( UNIT_BODY_SIZE * UNIT_GROUP_SIZE )

typedef struct msgchannel* msgchannel_ptr;
typedef struct msglistener* msglistener_ptr;
typedef struct msgtransmitter* msgtransmitter_ptr;

typedef struct msgaddr {
    void *addr;
    size_t addrlen;
    union{
        struct {
            uint32_t ip;
            uint16_t port;
        };
        char key[6];
    };
    uint8_t keylen;
}*msgaddr_ptr;

typedef struct transunit {
    ___atom_bool confirm;
    ___atom_bool resending;
    uint64_t timestamp;
    msgchannel_ptr chennel;
    struct {
        uint8_t type;
        uint8_t flag;
        uint8_t serial_number;
        uint8_t maximal;
        uint16_t expand;
        uint16_t body_size;
    } head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;


typedef struct transunitbuf {
    ___atom_8bit reading, rpos, wpos, confirm_pos;
    struct transunit *buf[UNIT_GROUP_SIZE];
}*transunitbuf_ptr;


typedef struct message {
    msgchannel_ptr channel;
    size_t size;
    char data[1];
}*message_ptr;


typedef struct msgbuf {
    message_ptr msg;
    uint8_t max_serial_number;
    uint8_t rpos, wpos;
    struct transunit *buf[UNIT_GROUP_SIZE];
}*msgbuf_ptr;


enum {
    CHANNEL_STATUS_OPEN = 0,
    CHANNEL_STATUS_CONNECTING = 1,
    CHANNEL_STATUS_CONNECT = 2,
    CHANNEL_STATUS_BINDING = 3,
    CHANNEL_STATUS_BIND = 4,
};

struct msgchannel {
    msgchannel_ptr prev, next;
    int status;
    ___atom_bool sending;
    ___mutex_ptr mtx;
    heap_t *timer;
    struct msgaddr addr;
    msgbuf_ptr recvbuf;
    transunitbuf_ptr ackbuf;
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
    size_t (*sendto)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;


#define RESEND_INTERVAL                 1000000000ULL
#define CHANNEL_QUEUE_COUNT             3

struct msgtransmitter {
    __tree peers;
    ___atom_bool running;
    ___mutex_ptr mtx;
    physics_socket_ptr device;
    msglistener_ptr listener;
    linekv_ptr send_func, recv_func;
    task_ptr send_task, recv_task;
    ___atom_size send_queue_length;
    struct channelqueue channels[CHANNEL_QUEUE_COUNT];
};


static inline void channel_ack_push_unit(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->chennel = channel;
    unit->resending = false;
    unit->confirm = false;
    while (UNIT_GROUP_SIZE - channel->ackbuf->wpos + channel->ackbuf->rpos == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        ___mutex_wait(channel->mtx, lk);
        ___mutex_unlock(channel->mtx, lk);
    }
    unit->head.serial_number = channel->ackbuf->wpos;
    channel->ackbuf->buf[channel->ackbuf->wpos] = unit;
    ___atom_add(&channel->ackbuf->wpos, 1);

    if (___set_true(&channel->sending)){
        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; i = ((i+1) % CHANNEL_QUEUE_COUNT))
        {
            channelqueue_ptr queue = &channel->transmitter->channels[i];
            if (!___atom_try_lock(&queue->lock)){
                continue;
            }
            channel->next = &queue->end;
            channel->prev = queue->end.prev;
            channel->prev->next = channel;
            channel->next->prev = channel;
            //TODO check result
            ___atom_add(&queue->len, 1);
            ___atom_unlock(&queue->lock);
            break;
        }
    }

    if (channel->transmitter->send_queue_length == 0){
        ___lock lk = ___mutex_lock(channel->transmitter->mtx);
        ___mutex_notify(channel->transmitter->mtx);
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }    
}

static inline transunit_ptr channel_ack_pull_unit(msgchannel_ptr channel)
{
    if (channel->ackbuf->wpos - channel->ackbuf->rpos > 0){
        transunit_ptr unit = channel->ackbuf->buf[channel->ackbuf->rpos];
        ___atom_add(&channel->ackbuf->rpos, 1);
        return unit;
    }else {
        return NULL;
    }
}

static inline void channel_push_unit(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->chennel = channel;
    unit->resending = false;
    unit->confirm = false;
    while (UNIT_GROUP_SIZE - channel->sendbuf->wpos + channel->sendbuf->rpos == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        ___mutex_wait(channel->mtx, lk);
        ___mutex_unlock(channel->mtx, lk);
    }
    unit->head.serial_number = channel->sendbuf->wpos;
    channel->sendbuf->buf[channel->sendbuf->wpos] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    if (___set_true(&channel->sending)){
        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; i = ((i+1) % CHANNEL_QUEUE_COUNT))
        {
            channelqueue_ptr queue = &channel->transmitter->channels[i];
            if (!___atom_try_lock(&queue->lock)){
                continue;
            }
            channel->next = &queue->end;
            channel->prev = queue->end.prev;
            channel->prev->next = channel;
            channel->next->prev = channel;
            //TODO check result
            ___atom_add(&queue->len, 1);
            ___atom_unlock(&queue->lock);
            break;
        }
    }

    if (___atom_add(&channel->transmitter->send_queue_length, 1) == 1){
        ___lock lk = ___mutex_lock(channel->transmitter->mtx);
        ___mutex_notify(channel->transmitter->mtx);
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }    
}

static inline transunit_ptr channel_hold_unit(msgchannel_ptr channel)
{
    if (channel->sendbuf->wpos - channel->sendbuf->reading > 0){
        return channel->sendbuf->buf[channel->sendbuf->reading++];
    }else {
        return NULL;
    }
}

static inline uint8_t channel_free_unit(msgchannel_ptr channel, uint8_t serial_number)
{
    // __logi("channel_free_unit enter");
    ___atom_set(&channel->sendbuf->confirm_pos, serial_number);
    // __logi("channel_free_unit rpos %u confirm pos %u free pos %u", channel->sendbuf->rpos, channel->sendbuf->confirm_pos, serial_number);
    if (serial_number == channel->sendbuf->rpos){
        // __logi("channel_free_unit 2");
        ___atom_add(&channel->sendbuf->rpos, 1);
        // __logi("channel_free_unit 2.1");
        while (channel->sendbuf->buf[channel->sendbuf->rpos] != NULL 
                && ___is_true(&channel->sendbuf->buf[channel->sendbuf->rpos]->confirm)){
            // __logi("channel_free_unit 3");
            ___atom_add(&channel->sendbuf->rpos, 1);
        }
        // __logi("channel_free_unit 4");
        ___mutex_notify(channel->mtx);
    }
    // __logi("channel_free_unit exit");
    return channel->sendbuf->rpos;
}

static inline void channel_make_message(msgchannel_ptr channel, transunit_ptr unit)
{
    __logi("channel_make_message rpos: %u unit max: %u serial number %u body: %s", 
        channel->recvbuf->rpos, unit->head.maximal, unit->head.serial_number, unit->body);
    int serial_gap = unit->head.serial_number - channel->recvbuf->wpos;

    if (serial_gap < 0){
        channel->recvbuf->buf[unit->head.serial_number] = unit;
    }else if (serial_gap > 0){
        channel->recvbuf->buf[unit->head.serial_number] = unit;
        channel->recvbuf->wpos = unit->head.serial_number + 1;
    }else {
        channel->recvbuf->buf[channel->recvbuf->wpos] = unit;
        channel->recvbuf->wpos++;
    }

    while (channel->recvbuf->buf[channel->recvbuf->rpos] != NULL){
        __logi("channel_make_message rpos: %u", channel->recvbuf->rpos);
        if (channel->recvbuf->msg == NULL){
            channel->recvbuf->max_serial_number = channel->recvbuf->buf[channel->recvbuf->rpos]->head.maximal;
            channel->recvbuf->msg = (message_ptr)malloc(sizeof(struct message) + (channel->recvbuf->max_serial_number * UNIT_BODY_SIZE));
            channel->recvbuf->msg->size = 0;
            channel->recvbuf->msg->channel = channel;
        }
        // __logi("channel_make_message copy size %u msg: %s", channel->recvbuf->buf[channel->recvbuf->rpos]->head.body_size, 
        //         channel->recvbuf->buf[channel->recvbuf->rpos]->body);
        memcpy(channel->recvbuf->msg->data + channel->recvbuf->msg->size, 
            channel->recvbuf->buf[channel->recvbuf->rpos]->body, 
            channel->recvbuf->buf[channel->recvbuf->rpos]->head.body_size);
        channel->recvbuf->msg->size += channel->recvbuf->buf[channel->recvbuf->rpos]->head.body_size;
        channel->recvbuf->max_serial_number--;
        if (channel->recvbuf->max_serial_number == 0){
            __logi("channel_make_message msg: %s", (char*)channel->recvbuf->msg->data);
            channel->transmitter->listener->message(channel->transmitter->listener, channel, channel->recvbuf->msg);
            channel->recvbuf->msg = NULL;
        }
        // free(channel->recvbuf->buf[channel->recvbuf->rpos]);
        channel->recvbuf->buf[channel->recvbuf->rpos] = NULL;
        channel->recvbuf->rpos++;
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransmitter_ptr transmitter, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->transmitter = transmitter;
    channel->addr = *addr;
    channel->status = CHANNEL_STATUS_CONNECTING;
    channel->recvbuf = (msgbuf_ptr) calloc(1, sizeof(struct msgbuf));
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf));
    channel->ackbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf));
    channel->timer = heap_create(UNIT_GROUP_SIZE);
    channel->mtx = ___mutex_create();
    return channel;
}

static inline void msgchannel_release(msgchannel_ptr channel)
{
    ___mutex_release(channel->mtx);
    while (channel->timer->pos > 0){
        __logi("msgchannel_release min_heapify_pop %lu", channel->timer->pos);
        heapment_t node = min_heapify_pop(channel->timer);
        free(node.value);
    }
    heap_destroy(&channel->timer);
    free(channel->recvbuf);
    free(channel->ackbuf);
    free(channel->sendbuf);
    free(channel);
}

static inline void msgtransmitter_recv_loop(linekv_ptr ctx)
{
    int result;
    struct msgaddr addr;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    msgtransmitter_ptr treanmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    // __logi("msgtransmitter_recv_loop running = %d", treanmitter->running);

    while (___is_true(&treanmitter->running))
    {
        unit = (transunit_ptr)malloc(sizeof(struct transunit));
        result = treanmitter->device->recvfrom(treanmitter->device, &addr, &unit->head, UNIT_TOTAL_SIZE);
        if (result == (unit->head.body_size + UNIT_HEAD_SIZE)){
            __logi("msgtransmitter_recv_loop >>>>----------> serial number %u", unit->head.serial_number);
            channel = (msgchannel_ptr)tree_find(treanmitter->peers, addr.key, addr.keylen);
            if (channel == NULL){
                channel = msgchannel_create(treanmitter, &addr);
                tree_inseart(treanmitter->peers, addr.key, addr.keylen, channel);
            }
            switch (unit->head.type & 0x0F)
            {
            case TRANSUNIT_MSG:
                __logi("msgtransmitter_recv_loop TRANSUNIT_MSG %s", unit->body);
                channel_make_message(channel, unit);
                unit->head.type = TRANSUNIT_ACK;
                unit->head.body_size = 0;
                unit->head.maximal = 1;
                channel_ack_push_unit(channel, unit);
                break;
            case TRANSUNIT_ACK:
                // __logi("msgtransmitter_recv_loop TRANSUNIT_ACK");
                // __logi("msgtransmitter_recv_loop TRANSUNIT_ACK channel 0x%x", channel);
                ___set_true(&channel->sendbuf->buf[unit->head.serial_number]->confirm);
                __logi("msgtransmitter_recv_loop TRANSUNIT_ACK unit addr 0x%x", channel->sendbuf->buf[unit->head.serial_number]);
                channel_free_unit(channel, unit->head.serial_number);
                break;
            case TRANSUNIT_PING:
                __logi("msgtransmitter_recv_loop TRANSUNIT_PING");
                channel_make_message(channel, unit);
                unit->head.type = TRANSUNIT_PONG;
                unit->head.body_size = 0;
                unit->head.maximal = 1;
                // channel_push_unit(channel, unit);
                channel_ack_push_unit(channel, unit);
                break;                
            case TRANSUNIT_PONG:
                // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG");
                // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG channel 0x%x", channel);
                // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG unit addr 0x%x", channel->sendbuf->buf[unit->head.serial_number]);
                ___set_true(&channel->sendbuf->buf[unit->head.serial_number]->confirm);
                // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG 1");
                channel_free_unit(channel, unit->head.serial_number);
                // // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG 2");
                // unit->head.type = TRANSUNIT_ACK;
                // unit->head.body_size = 0;
                // unit->head.maximal = 1;
                // // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG 3");
                // channel_ack_push_unit(channel, unit);
                // // __logi("msgtransmitter_recv_loop TRANSUNIT_PONG exit");
                break;
            default:
                channel = (msgchannel_ptr)tree_delete(treanmitter->peers, addr.key, addr.keylen);
                msgchannel_release(channel);
                break;
            }
        }else {
            __logw("msgtransmitter_recv_loop break");
            free(unit);
            break;
        }
    }

    __logw("msgtransmitter_recv_loop exit");
}


static inline void msgtransmitter_send_loop(linekv_ptr ctx)
{
    int result;
    int64_t timeout;
    uint64_t timer = 1000000000UL * 60UL;
    heapment_t timenode;
    struct msgaddr addr;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    channelqueue_ptr channellist = NULL;
    msgtransmitter_ptr tansmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");

    // __logi("msgtransmitter_send_loop running = %d", tansmitter->running);

    while (___is_true(&tansmitter->running))
    {
        // __logi("msgtransmitter_send_loop channel queue %d", tansmitter->running);

        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; ++i)
        {
            // __logi("msgtransmitter_send_loop channel queue %lu", i);
            channellist = &tansmitter->channels[i];

            if (!___atom_try_lock(&channellist->lock)){
                continue;
            }

            if (channellist->len == 0){
                ___atom_unlock(&channellist->lock);
                continue;
            }

            channel = channellist->head.next;
            
            while (channel != &channellist->end)
            {
                // __logi("msgtransmitter_send_loop channel list len %lu", channellist->len);
                if (channel->sendbuf->rpos != channel->sendbuf->confirm_pos){
                    int8_t pos = -1; 
                    while ((channel->sendbuf->rpos + pos) != channel->sendbuf->confirm_pos){
                        // __logi("msgtransmitter_send_loop rpos %u  confirm pos %u", channel->sendbuf->rpos, channel->sendbuf->confirm_pos);
                        unit = channel->sendbuf->buf[channel->sendbuf->rpos + pos];
                        if (___is_false(&unit->confirm) && ___set_true(&unit->resending)){
                            result = tansmitter->device->sendto(tansmitter->device, &channel->addr, 
                                    (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                        }else {
                            break;
                        }
                        pos++;
                    }
                }

                // __logi("msgtransmitter_send_loop channel_ack_pull_unit");
                while ((unit = channel_ack_pull_unit(channel)) != NULL)
                {
                    __logi("msgtransmitter_send_loop >>>>------------> ack unit serial number: %u addr: 0x%x size: %u type: %u", unit->head.serial_number, unit, unit->head.body_size, unit->head.type);
                    if (tansmitter->device->sendto(tansmitter->device, &channel->addr, 
                        (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size) < 0){
                        __logi("msgtransmitter_send_loop return");
                        return;
                    }
                }
                
                // __logi("msgtransmitter_send_loop channel_hold_unit");
                if ((unit = channel_hold_unit(channel)) != NULL){
                    __logi("msgtransmitter_send_loop >>>>------------> msg unit serial number: %u addr: 0x%x size: %u type: %u", unit->head.serial_number, unit, unit->head.body_size, unit->head.type);
                    size_t qlen = ___atom_sub(&channel->transmitter->send_queue_length, 1);
                    // __logi("msgtransmitter_send_loop send_queue_length %lu", qlen);
                    result = tansmitter->device->sendto(tansmitter->device, &channel->addr, 
                        (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                    // __logi("msgtransmitter_send_loop send size %lu result size %lu", UNIT_HEAD_SIZE + unit->head.body_size, result);
                    if (result == UNIT_HEAD_SIZE + unit->head.body_size){
                        timenode.key = ___sys_clock() + RESEND_INTERVAL;
                        // __logi("msgtransmitter_send_loop timer.key %lu", timenode.key);
                        timenode.value = channel->sendbuf->buf[channel->sendbuf->reading-1];
                        // __logi("msgtransmitter_send_loop min_heapify_push 0x%x", timenode.value);
                        min_heapify_push(channel->timer, timenode);
                    }else {
                        __logi("msgtransmitter_send_loop return");
                        return;
                    }

                }else if (channel->timer->pos == 0 
                            && channel->sendbuf->wpos - channel->sendbuf->rpos == 0 
                            && channel->ackbuf->wpos - channel->ackbuf->rpos == 0){
                    channel->next->prev = channel->prev;
                    channel->prev->next = channel->next;
                    ___atom_sub(&channellist->len, 1);
                    ___set_false(&channel->sending);
                }

                while (channel->timer->pos > 0)
                {
                    if ((timeout = channel->timer->array[1].key - ___sys_clock()) > 0){
                        if (timer > timeout){
                            timer = timeout;
                            // __logi("msgtransmitter_send_loop timer %lu", timer);
                        }
                        break;
                    }else {
                        __logi("msgtransmitter_send_loop timeout %ld", timeout);
                        unit = (transunit_ptr)channel->timer->array[1].value;
                        // std::cout << "unit->confirm: " << ___is_false(&unit->confirm) << std::endl;
                        if (___is_false(&unit->confirm)){
                            result = tansmitter->device->sendto(tansmitter->device, &unit->chennel->addr, 
                                    (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                            if (result > 0){
                                min_heapify_pop(channel->timer);
                                timenode.key = ___sys_clock() + RESEND_INTERVAL;
                                timenode.value = unit;
                                min_heapify_push(channel->timer, timenode);
                                __logi("msgtransmitter_send_loop timer repush %u", unit->head.serial_number);
                            }else {
                                break;
                            }
                        }else {
                            min_heapify_pop(channel->timer);
                            __logi("msgtransmitter_send_loop free unit 0x%x", unit);
                            // std::cout << "unit->confirm: " << ___is_false(&unit->confirm) << std::endl;
                            free(unit);
                        }
                    }
                }

                channel = channel->next;
                // __logi("msgtransmitter_send_loop next channel %x", channel);
            }

            // __logi("msgtransmitter_send_loop channel queue unlock");

            ___atom_unlock(&channellist->lock);
        }
        
        {
            ___lock lk = ___mutex_lock(tansmitter->mtx);
            if (tansmitter->send_queue_length == 0){
                // __logi("msgtransmitter_send_loop waitting enter %lu", timer);
                ___mutex_timer(tansmitter->mtx, lk, timer);
                timer = 1000000000UL * 60UL;;
                // __logi("msgtransmitter_send_loop waitting exit %lu", timer);
            }
            ___mutex_unlock(tansmitter->mtx, lk);
        }
    }

    __logi("msgtransmitter_send_loop exit");
}

static inline msgtransmitter_ptr msgtransmitter_create(physics_socket_ptr device, msglistener_ptr listener)
{
    msgtransmitter_ptr mtp = (msgtransmitter_ptr)calloc(1, sizeof(struct msgtransmitter));

    mtp->device = device;
    mtp->listener = listener;
    mtp->peers = tree_create();
    mtp->mtx = ___mutex_create();

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

    mtp->running = true;
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
        __logi("tree_release");
        tree_release(&transmitter->peers);
        linekv_release(&transmitter->send_func);
        linekv_release(&transmitter->recv_func);
        ___mutex_release(transmitter->mtx);
        free(transmitter);
    }
}

static inline msgchannel_ptr msgtransmitter_connect(msgtransmitter_ptr mtp, msgaddr_ptr addr)
{
    __logi("msgtransmitter_connect enter");
    msgchannel_ptr channel = msgchannel_create(mtp, addr);
    tree_inseart(mtp->peers, addr->key, addr->keylen, channel);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    __logi("msgtransmitter_connect 0x%x", unit);
    unit->head.type = TRANSUNIT_PING;
    unit->head.body_size = 0;
    unit->head.maximal = 1;
    channel_push_unit(channel, unit);
    __logi("msgtransmitter_connect exit");
    return channel;
}

static inline void msgtransmitter_disconnect(msgtransmitter_ptr transmitter, msgchannel_ptr channel)
{
    if (tree_delete(transmitter->peers, channel->addr.key, channel->addr.keylen) != channel){
        //TODO
    }
    msgchannel_release(channel);
}

static inline void msgtransmitter_send(msgtransmitter_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
{
    char *group_data;
    size_t group_size;
    size_t group_count = size / (UNIT_GROUP_BUF_SIZE);
    size_t last_group_size = size - (group_count * UNIT_GROUP_BUF_SIZE);
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
            channel_push_unit(channel, unit);
        }
    }

    group_data = ((char*)data) + (size - last_group_size);
    group_size = last_group_size;

    unit_count = group_size / UNIT_BODY_SIZE;
    last_unit_size = group_size - unit_count * UNIT_BODY_SIZE;
    last_unit_id = 0;

    if (last_unit_size > 0){
        last_unit_id = 1;
        unit_count += 1;
    }

    for (size_t y = 0; (y + last_unit_id) < unit_count; y++){
        transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
        memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
        unit->head.type = TRANSUNIT_MSG;
        unit->head.body_size = UNIT_BODY_SIZE;
        unit->head.maximal = last_unit_id - y;
        channel_push_unit(channel, unit);
    }

    if (last_unit_id){
        transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
        memcpy(unit->body, group_data + ((unit_count - 1) * UNIT_BODY_SIZE), last_unit_size);
        unit->head.type = TRANSUNIT_MSG;        
        unit->head.body_size = last_unit_size;
        unit->head.maximal = 1;
        __logi("msgtransmitter_send unit addr: 0x%x size: %u type: %u msg: %s", unit, unit->head.body_size, unit->head.type, unit->body);
        channel_push_unit(channel, unit);
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__