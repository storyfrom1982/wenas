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

typedef struct unithead {
    uint8_t type;
    uint8_t flag;
    uint8_t serial_number;
    uint8_t maximal;
    uint16_t expand;
    uint16_t body_size;
}*unithead_ptr;

typedef struct transunit {
    ___atom_bool confirm;
    ___atom_bool resending;
    uint64_t timestamp;
    msgchannel_ptr chennel;
    struct unithead head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;


typedef struct transunitbuf {
    ___atom_8bit reading, rpos, wpos, confirm_pos;
    struct transunit *buf[1];
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
    struct transunit *buf[1];
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
    msgbuf_ptr msgbuf;
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
#define TRANSMITTER_RECVBUF_LEN         65536

struct msgtransmitter {
    __tree peers;
    ___atom_bool running;
    ___atom_bool listening;
    ___mutex_ptr mtx;
    physics_socket_ptr device;
    msglistener_ptr listener;
    linekv_ptr send_func, recv_func;
    task_ptr send_task, recv_task;
    ___atom_size send_queue_length;
    transunitbuf_ptr recvbuf;
    struct channelqueue channels[CHANNEL_QUEUE_COUNT];
};


static inline void channel_push_unit(msgchannel_ptr channel, transunit_ptr unit)
{
    __logi("channel_push_unit enter");
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
        __logi("channel_push_unit channel->sending");
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
        ___mutex_broadcast(channel->transmitter->mtx);
        __logi("channel_push_unit ___mutex_notify");
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }
    __logi("channel_push_unit exit");
}

static inline transunit_ptr channel_hold_reading_pos(msgchannel_ptr channel)
{
    if (channel->sendbuf->wpos - channel->sendbuf->reading > 0){
        return channel->sendbuf->buf[channel->sendbuf->reading];
    }else {
        return NULL;
    }
}

static inline void channel_move_reading_pos(msgchannel_ptr channel)
{
    channel->sendbuf->reading++;
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
        channel->msgbuf->rpos, unit->head.maximal, unit->head.serial_number, unit->body);
    int serial_gap = unit->head.serial_number - channel->msgbuf->wpos;

    if (serial_gap < 0){
        channel->msgbuf->buf[unit->head.serial_number] = unit;
    }else if (serial_gap > 0){
        channel->msgbuf->buf[unit->head.serial_number] = unit;
        channel->msgbuf->wpos = unit->head.serial_number + 1;
    }else {
        channel->msgbuf->buf[channel->msgbuf->wpos] = unit;
        channel->msgbuf->wpos++;
    }

    __logi("channel_make_message ------------- rpos: %hu", channel->msgbuf->rpos);
    while (channel->msgbuf->buf[channel->msgbuf->rpos] != NULL){
        __logi("channel_make_message rpos: %hu", channel->msgbuf->rpos);
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->max_serial_number = channel->msgbuf->buf[channel->msgbuf->rpos]->head.maximal;
            channel->msgbuf->msg = (message_ptr)malloc(sizeof(struct message) + (channel->msgbuf->max_serial_number * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }
        // __logi("channel_make_message copy size %u msg: %s", channel->msgbuf->buf[channel->msgbuf->rpos]->head.body_size, 
        //         channel->msgbuf->buf[channel->msgbuf->rpos]->body);
        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[channel->msgbuf->rpos]->body, 
            channel->msgbuf->buf[channel->msgbuf->rpos]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[channel->msgbuf->rpos]->head.body_size;
        channel->msgbuf->max_serial_number--;
        if (channel->msgbuf->max_serial_number == 0){
            // __logi("channel_make_message msg: %s", (char*)channel->msgbuf->msg->data);
            channel->transmitter->listener->message(channel->transmitter->listener, channel, channel->msgbuf->msg);
            channel->msgbuf->msg = NULL;
        }
        // free(channel->msgbuf->buf[channel->msgbuf->rpos]);
        channel->msgbuf->buf[channel->msgbuf->rpos] = NULL;
        channel->msgbuf->rpos++;
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransmitter_ptr transmitter, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->transmitter = transmitter;
    channel->addr = *addr;
    channel->status = CHANNEL_STATUS_CONNECTING;
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
        __logi("msgchannel_release min_heapify_pop %lu", channel->timer->pos);
        heapment_t node = min_heapify_pop(channel->timer);
        free(node.value);
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
        while (transmitter->recvbuf->wpos - transmitter->recvbuf->rpos == 0)
        {
            if (___is_true(&transmitter->listening)){
                ___lock lk = ___mutex_lock(transmitter->mtx);
                ___mutex_unlock(transmitter->mtx, lk);
                __logw("msgtransmitter_recv_loop listening enter");
                transmitter->device->listening(transmitter->device);
                __logw("msgtransmitter_recv_loop listening exit");
                ___set_false(&transmitter->listening);
                ___mutex_broadcast(transmitter->mtx);
            }else {
                ___lock lk = ___mutex_lock(transmitter->mtx);
                ___mutex_wait(transmitter->mtx, lk);
                ___mutex_unlock(transmitter->mtx, lk);
            }
        }

        unit = transmitter->recvbuf->buf[transmitter->recvbuf->rpos];;
        channel_make_message(unit->chennel, unit);
        ___atom_add(&transmitter->recvbuf->rpos, 1);
        ___mutex_broadcast(transmitter->mtx);
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
    struct unithead ack;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    channelqueue_ptr channellist = NULL;
    msgtransmitter_ptr transmitter = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");


    while (___is_true(&transmitter->running))
    {
        // __logi("msgtransmitter_send_loop running");
        while (___is_true(&transmitter->running))
        {
            unit = (transunit_ptr)malloc(sizeof(struct transunit));
            unit->head.body_size = 0;
            result = transmitter->device->recvfrom(transmitter->device, &addr, &unit->head, UNIT_TOTAL_SIZE);
            // __logi("msgtransmitter_send_loop recvfrom = %d", result);
            if (result == (unit->head.body_size + UNIT_HEAD_SIZE)){
                channel = (msgchannel_ptr)tree_find(transmitter->peers, addr.key, addr.keylen);
                if (channel == NULL){
                    channel = msgchannel_create(transmitter, &addr);
                    tree_inseart(transmitter->peers, addr.key, addr.keylen, channel);
                }
                switch (unit->head.type & 0x0F)
                {
                case TRANSUNIT_MSG:
                    ack = unit->head;
                    ack.type = TRANSUNIT_ACK;
                    ack.body_size = 0;
                    while (transmitter->device->sendto(transmitter->device, &channel->addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                        __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    }
                    unit->chennel = channel;
                    if (TRANSMITTER_RECVBUF_LEN - transmitter->recvbuf->wpos + transmitter->recvbuf->rpos == 0){
                        ___lock lk = ___mutex_lock(transmitter->mtx);
                        ___mutex_wait(transmitter->mtx, lk);
                        ___mutex_unlock(transmitter->mtx, lk);
                    }
                    transmitter->recvbuf->buf[transmitter->recvbuf->wpos] = unit;
                    ___atom_add(&transmitter->recvbuf->wpos, 1);
                    ___mutex_broadcast(transmitter->mtx);
                    break;
                case TRANSUNIT_ACK:
                    __loge("msgtransmitter_send_loop recv TRANSUNIT_ACK %u", unit->head.serial_number);
                    ___set_true(&channel->sendbuf->buf[unit->head.serial_number]->confirm);
                    channel_free_unit(channel, unit->head.serial_number);
                    break;
                case TRANSUNIT_PING:
                    ack = unit->head;
                    ack.type = TRANSUNIT_PONG;
                    ack.body_size = 0;
                    while (transmitter->device->sendto(transmitter->device, &channel->addr, (void*)&(ack), UNIT_HEAD_SIZE) != UNIT_HEAD_SIZE){
                        __loge("msgtransmitter_send_loop sendto failed !!!!!!");
                    }
                    unit->chennel = channel;
                    if (TRANSMITTER_RECVBUF_LEN - transmitter->recvbuf->wpos + transmitter->recvbuf->rpos == 0){
                        ___lock lk = ___mutex_lock(transmitter->mtx);
                        ___mutex_wait(transmitter->mtx, lk);
                        ___mutex_unlock(transmitter->mtx, lk);
                    }
                    transmitter->recvbuf->buf[transmitter->recvbuf->wpos] = unit;
                    ___atom_add(&transmitter->recvbuf->wpos, 1);
                    ___mutex_broadcast(transmitter->mtx);                    
                    break;
                case TRANSUNIT_PONG:
                    __loge("msgtransmitter_send_loop recv TRANSUNIT_PONG");
                    ___set_true(&channel->sendbuf->buf[unit->head.serial_number]->confirm);
                    channel_free_unit(channel, unit->head.serial_number);
                    break;
                default:
                    channel = (msgchannel_ptr)tree_delete(transmitter->peers, addr.key, addr.keylen);
                    msgchannel_release(channel);
                    break;
                }
            }else {
                // __logw("msgtransmitter_send_loop break");
                free(unit);
                ___lock lk = ___mutex_lock(transmitter->mtx);
                ___set_true(&transmitter->listening);
                ___mutex_broadcast(transmitter->mtx);
                ___mutex_unlock(transmitter->mtx, lk);
                break;
            }
        }

        // __logi("msgtransmitter_send_loop start sending");

        for (size_t i = 0; i < CHANNEL_QUEUE_COUNT; ++i)
        {
            // __logi("msgtransmitter_send_loop channel queue %lu", i);
            channellist = &transmitter->channels[i];

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
                        __logi("msgtransmitter_send_loop confirm enter");
                        if (___is_false(&unit->confirm) && ___set_true(&unit->resending)){
                            __logi("msgtransmitter_send_loop confirm exit");
                            result = transmitter->device->sendto(transmitter->device, &channel->addr, 
                                    (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                        }else {
                            break;
                        }
                        pos++;
                    }
                }
                
                // __logi("msgtransmitter_send_loop channel_hold_reading_pos");
                if ((unit = channel_hold_reading_pos(channel)) != NULL){
                    __logi("msgtransmitter_send_loop >>>>------------> msg unit serial number: %u addr: 0x%x size: %u type: %u", unit->head.serial_number, unit, unit->head.body_size, unit->head.type);
                    size_t qlen = ___atom_sub(&channel->transmitter->send_queue_length, 1);
                    // __logi("msgtransmitter_send_loop send_queue_length >>>>------------> %lu", qlen);
                    result = transmitter->device->sendto(transmitter->device, &channel->addr, 
                        (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                    // __logi("msgtransmitter_send_loop send size %lu result size %lu", UNIT_HEAD_SIZE + unit->head.body_size, result);
                    if (result == UNIT_HEAD_SIZE + unit->head.body_size){
                        // __logi("msgtransmitter_send_loop channel_move_reading_pos");
                        channel_move_reading_pos(channel);
                        unit->timestamp = ___sys_clock();
                        timenode.key = unit->timestamp + RESEND_INTERVAL;
                        // __logi("msgtransmitter_send_loop timer.key %lu", timenode.key);
                        // timenode.value = channel->sendbuf->buf[channel->sendbuf->reading-1];
                        timenode.value = unit;
                        // __logi("msgtransmitter_send_loop min_heapify_push 0x%x", timenode.value);
                        min_heapify_push(channel->timer, timenode);
                    }else {
                        __logi("msgtransmitter_send_loop return");
                        return;
                    }

                }else if (channel->timer->pos == 0 && channel->sendbuf->wpos - channel->sendbuf->rpos == 0){
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
                        // __logi("msgtransmitter_send_loop timeout %ld", timeout);
                        unit = (transunit_ptr)channel->timer->array[1].value;
                        // std::cout << "unit->confirm: " << ___is_false(&unit->confirm) << std::endl;
                        if (___is_false(&unit->confirm)){
                            result = transmitter->device->sendto(transmitter->device, &unit->chennel->addr, 
                                    (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                            if (result > 0){
                                min_heapify_pop(channel->timer);
                                unit->timestamp = ___sys_clock();
                                timenode.key = unit->timestamp + RESEND_INTERVAL;
                                // timenode.key = ___sys_clock() + RESEND_INTERVAL;
                                timenode.value = unit;
                                min_heapify_push(channel->timer, timenode);
                                __logi("msgtransmitter_send_loop timer resend %u", unit->head.serial_number);
                            }else {
                                __logi("msgtransmitter_send_loop resend failed");
                                break;
                            }
                        }else {
                            min_heapify_pop(channel->timer);
                            __logi("msgtransmitter_send_loop free unit serial number %u", unit->head.serial_number);
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
            ___lock lk = ___mutex_lock(transmitter->mtx);
            if (transmitter->send_queue_length == 0){
                // __logi("msgtransmitter_send_loop waitting enter %lu", timer);
                ___mutex_timer(transmitter->mtx, lk, timer);
                timer = 1000000000UL * 60UL;;
                // __logi("msgtransmitter_send_loop waitting exit %lu", timer);
            }
            ___mutex_unlock(transmitter->mtx, lk);
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

    mtp->running = true;
    mtp->listening = false;
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
        free(transmitter->recvbuf);
        free(transmitter);
    }
}

static inline msgchannel_ptr msgtransmitter_connect(msgtransmitter_ptr mtp, msgaddr_ptr addr)
{
    __logi("msgtransmitter_connect enter");
    msgchannel_ptr channel = msgchannel_create(mtp, addr);
    ___lock lk = ___mutex_lock(channel->mtx);
    tree_inseart(mtp->peers, addr->key, addr->keylen, channel);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    __logi("msgtransmitter_connect 0x%x", unit);
    unit->head.type = TRANSUNIT_PING;
    unit->head.body_size = 0;
    unit->head.maximal = 1;
    channel_push_unit(channel, unit);
    ___mutex_wait(channel->mtx, lk);
    ___mutex_unlock(channel->mtx, lk);
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
        // __logi("msgtransmitter_send unit addr: 0x%x size: %u type: %u msg: %s", unit, unit->head.body_size, unit->head.type, unit->body);
        channel_push_unit(channel, unit);
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__