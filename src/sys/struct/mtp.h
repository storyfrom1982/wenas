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
#define UNIT_BODY_SIZE          1400
#define UNIT_TOTAL_SIZE         1408
#define UNIT_GROUP_SIZE         256
#define UNIT_GROUP_BUF_SIZE     (UNIT_BODY_SIZE * UNIT_GROUP_SIZE)

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
    ___atom_bool timer;
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
    ___atom_8bit reading, rpos, wpos, confirm;
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
    int status;
    ___atom_bool sending;
    ___mutex_ptr mtx;
    heap_t *timer;
    struct msgaddr addr;
    struct msgbuf recvbuf;
    struct transunitbuf ackbuf;
    struct transunitbuf sendbuf;
    msglistener_ptr listener;
    msgtransmitter_ptr transmitter;
    struct msgchannel *prev, *next;
};

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

struct msgtransmitter {
    __tree peers;
    ___atom_bool running;
    ___atom_bool watting;
    ___atom_bool readable;
    ___atom_size sendable;
    ___mutex_ptr mtx;
    physics_socket_ptr device;
    msglistener_ptr listener;
    linekv_ptr send_func, recv_func;
    task_ptr send_task, recv_task;
    struct msgchannel head, end;
};

typedef struct msgtransmitter** mtp_pptr;


static inline void channel_ack_push_unit(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->chennel = channel;
    unit->resending = false;
    unit->timer = false;
    while (UNIT_GROUP_SIZE - channel->ackbuf.wpos + channel->ackbuf.rpos == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        ___mutex_wait(channel->mtx, lk);
        ___mutex_unlock(channel->mtx, lk);
    }
    unit->head.serial_number = channel->ackbuf.wpos;
    channel->ackbuf.buf[channel->ackbuf.wpos] = unit;
    ___atom_add(&channel->ackbuf.wpos, 1);
    ___mutex_notify(channel->mtx);

    if (___set_true(&channel->sending)){
        ___lock lk = ___mutex_lock(channel->transmitter->mtx);
        channel->next = &channel->transmitter->end;
        channel->prev = channel->transmitter->end.prev;
        channel->prev->next = channel;
        channel->next->prev = channel;
        ___mutex_notify(channel->transmitter->mtx);
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }
}

static inline transunit_ptr channel_ack_pull_unit(msgchannel_ptr channel)
{
    if (channel->ackbuf.wpos - channel->ackbuf.rpos > 0){
        transunit_ptr unit = channel->ackbuf.buf[channel->ackbuf.rpos];
        ___atom_add(&channel->ackbuf.rpos, 1);
        return unit;
    }else {
        return NULL;
    }
}

static inline void channel_push_unit(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->chennel = channel;
    unit->resending = false;
    unit->timer = false;
    while (UNIT_GROUP_SIZE - channel->sendbuf.wpos + channel->sendbuf.rpos == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        ___mutex_wait(channel->mtx, lk);
        ___mutex_unlock(channel->mtx, lk);
    }
    unit->head.serial_number = channel->sendbuf.wpos;
    channel->sendbuf.buf[channel->sendbuf.wpos] = unit;
    ___atom_add(&channel->sendbuf.wpos, 1);
    ___mutex_notify(channel->mtx);

    if (___set_true(&channel->sending)){
        ___lock lk = ___mutex_lock(channel->transmitter->mtx);
        channel->next = &channel->transmitter->end;
        channel->prev = channel->transmitter->end.prev;
        channel->prev->next = channel;
        channel->next->prev = channel;
        ___mutex_notify(channel->transmitter->mtx);
        ___mutex_unlock(channel->transmitter->mtx, lk);
    }

}

static inline transunit_ptr channel_hold_unit(msgchannel_ptr channel)
{
    if (channel->sendbuf.wpos - channel->sendbuf.reading > 0){
        return channel->sendbuf.buf[channel->sendbuf.reading++];
    }else {
        return NULL;
    }
}

static inline uint8_t channel_free_unit(msgchannel_ptr channel, uint8_t serial_number)
{
    ___atom_set(&channel->sendbuf.confirm, serial_number);
    if (serial_number == channel->sendbuf.rpos){
        ___atom_add(&channel->sendbuf.rpos, 1);
        while (___is_false(&channel->sendbuf.buf[channel->sendbuf.rpos]->timer)){
            ___atom_add(&channel->sendbuf.rpos, 1);
        }
        ___mutex_notify(channel->mtx);
    }
    return channel->sendbuf.rpos;
}

static inline void channel_make_message(msgchannel_ptr channel, transunit_ptr unit)
{
    int serial_gap = unit->head.serial_number - channel->recvbuf.wpos;

    if (serial_gap < 0){
        channel->recvbuf.buf[unit->head.serial_number] = unit;
    }else if (serial_gap > 0){
        channel->recvbuf.buf[unit->head.serial_number] = unit;
        channel->recvbuf.wpos = unit->head.serial_number + 1;
    }else {
        channel->recvbuf.buf[channel->recvbuf.wpos] = unit;
        channel->recvbuf.wpos++;
    }

    while (channel->recvbuf.buf[channel->recvbuf.rpos] != NULL){
        if (channel->recvbuf.msg == NULL){
            channel->recvbuf.max_serial_number = channel->recvbuf.buf[channel->recvbuf.rpos]->head.maximal;
            channel->recvbuf.msg = (message_ptr)malloc(sizeof(struct message) + (channel->recvbuf.max_serial_number * UNIT_BODY_SIZE));
            channel->recvbuf.msg->size = 0;
            channel->recvbuf.msg->channel = channel;
        }
        memcpy(channel->recvbuf.msg->data + channel->recvbuf.msg->size, 
            channel->recvbuf.buf[channel->recvbuf.rpos]->body, 
            channel->recvbuf.buf[channel->recvbuf.rpos]->head.body_size);
        channel->recvbuf.msg->size += channel->recvbuf.buf[channel->recvbuf.rpos]->head.body_size;
        channel->recvbuf.max_serial_number--;
        if (channel->recvbuf.max_serial_number == 0){
            channel->listener->message(channel->listener, channel, channel->recvbuf.msg);
            channel->recvbuf.msg = NULL;
        }
        free(channel->recvbuf.buf[channel->recvbuf.rpos]);
        channel->recvbuf.rpos++;
    }
}

static inline void msgtransmitter_recv_loop(linekv_ptr ctx)
{
    int result;
    msgtransmitter_ptr mtp = (msgtransmitter_ptr)linekv_find_ptr(ctx, "ctx");
    struct msgaddr addr;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;

    while (___is_true(&mtp->running))
    {
        unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        result = mtp->device->recvfrom(mtp->device, &addr, &unit->head, UNIT_TOTAL_SIZE);
        if (result == (unit->head.body_size + UNIT_HEAD_SIZE)){
            channel = (msgchannel_ptr)tree_find(mtp->peers, addr.key, addr.keylen);
            switch (unit->head.type)
            {
            case TRANSUNIT_MSG:
                unit->head.type = TRANSUNIT_ACK;
                unit->head.body_size = 0;
                unit->head.maximal = 1;
                channel_ack_push_unit(channel, unit);
                channel_make_message(channel, unit);
                break;                 
            case TRANSUNIT_ACK:
                ___set_false(&channel->sendbuf.buf[unit->head.serial_number]->timer);
                channel_free_unit(channel, unit->head.serial_number);
                break;
            case TRANSUNIT_PING:
                if (channel == NULL){
                    channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
                    channel->transmitter = mtp;
                    channel->addr = addr;
                    channel->status = CHANNEL_STATUS_CONNECTING;
                    channel->timer = heap_create(UNIT_GROUP_SIZE + 1);
                    tree_inseart(mtp->peers, addr.key, addr.keylen, channel);                    
                }
                unit->head.type = TRANSUNIT_PONG;
                unit->head.body_size = 0;
                unit->head.maximal = 1;
                channel_push_unit(channel, unit);
                break;                
            case TRANSUNIT_PONG:
                ___set_false(&channel->sendbuf.buf[unit->head.serial_number]->timer);
                channel_free_unit(channel, unit->head.serial_number);
                unit->head.type = TRANSUNIT_ACK;
                unit->head.body_size = 0;
                unit->head.maximal = 1;
                channel_ack_push_unit(channel, unit);
                break;
            default:
                channel = (msgchannel_ptr)tree_delete(mtp->peers, addr.key, addr.keylen);
                free(channel);
                break;
            }
        }else {
            __logw("msgtransmitter_recv_loop break");
            free(unit);
            break;
        }
    }
}


static inline int msgtransmitter_send_loop(msgtransmitter_ptr mtp)
{
    int ret;
    uint64_t sleep_time = 0;
    struct msgaddr addr;
    heapment_t timer;
    transunit_ptr unit = NULL;
    msgchannel_ptr channel = NULL;
    
    mtp->readable = 0;
    mtp->sendable = false;


    while (___is_true(&mtp->running))
    {
        ___lock lk = ___mutex_lock(mtp->mtx);

        channel = mtp->head.next;
        
        while (channel != &mtp->end)
        {
            while (channel->timer->pos > 0)
            {
                uint64_t timeout = channel->timer->array[1].key - ___sys_clock();
                if (timeout > 0){
                    if (timeout < sleep_time ){
                        sleep_time = timeout;
                    }
                    break;

                }else {
                    unit = (transunit_ptr)channel->timer->array[1].value;
                    if (___is_true(&unit->timer)){
                        ret = mtp->device->sendto(mtp->device, &unit->chennel->addr, 
                                (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                        if (ret > 0){
                            min_heapify_pop(channel->timer);
                            timer.key = ___sys_clock();
                            timer.value = unit;
                            min_heapify_push(channel->timer, timer);
                        }else {
                            break;
                        }

                    }else {
                        free(unit);
                    }
                }
            }

            if (channel->sendbuf.rpos != channel->sendbuf.confirm){
                uint8_t pos = 0; 
                while ((channel->sendbuf.rpos + pos) != channel->sendbuf.confirm){
                    unit = channel->sendbuf.buf[channel->sendbuf.rpos];
                    if (___is_true(&unit->timer) && !unit->resending){
                        unit->resending = true;
                        ret = mtp->device->sendto(mtp->device, &channel->addr, (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                    }else {
                        break;
                    }
                }
            }

            unit = channel_hold_unit(channel);
            if (unit){

                ret = mtp->device->sendto(mtp->device, &channel->addr, 
                    (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size);
                if (ret > 0){
                    timer.key = ___sys_clock();
                    timer.value = channel->sendbuf.buf[channel->sendbuf.reading];
                    min_heapify_push(channel->timer, timer);
                }else {
                    ___set_false(&mtp->sendable);
                }

            }else if (channel->timer->pos == 0 
                && channel->sendbuf.wpos - channel->sendbuf.rpos == 0
                && channel->ackbuf.wpos - channel->ackbuf.rpos == 0){

                if (channel->next){
                    channel->next->prev = channel->prev;
                }
                if (channel->prev){
                    channel->prev->next = channel->next;
                }
                ___set_false(&channel->sending);
            }

            channel = channel->next;
        }

        if (mtp->head.next == &mtp->end){
            
            ___set_true(&mtp->watting);
            if (sleep_time > 0){
                ___mutex_timer(mtp->mtx, lk, sleep_time);
                sleep_time = 0;
            }else {
                ___mutex_wait(mtp->mtx, lk);
            }
        }

        ___mutex_unlock(mtp->mtx, lk);
    }

    return 0;
}

static inline msgtransmitter_ptr msgtransmitter_create(physics_socket_ptr device, msglistener_ptr listener)
{
    msgtransmitter_ptr mtp = (msgtransmitter_ptr)calloc(1, sizeof(struct msgtransmitter));
    mtp->sendable = 0;
    mtp->device = device;
    mtp->listener = listener;
    mtp->peers = tree_create();
    mtp->mtx = ___mutex_create();
    mtp->head.prev = NULL;
    mtp->end.next = NULL;
    mtp->head.next = &mtp->end;
    mtp->end.prev = &mtp->head;

    mtp->running = true;
    mtp->recv_func = linekv_create(128);
    linekv_add_ptr(mtp->recv_func, "func", msgtransmitter_recv_loop);
    linekv_add_ptr(mtp->recv_func, "ctx", mtp);
    mtp->recv_task = task_create();
    task_post(mtp->recv_task, mtp->recv_func);

    mtp->send_func = linekv_create(128);
    linekv_add_ptr(mtp->send_func, "func", msgtransmitter_send_loop);
    linekv_add_ptr(mtp->send_func, "ctx", mtp);
    mtp->send_task = task_create();
    task_post(mtp->send_task, mtp->send_func);

    return mtp;
}

static inline void msgtransmitter_release(mtp_pptr pptr)
{

}

static inline msgchannel_ptr msgtransmitter_connect(msgtransmitter_ptr mtp, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr)calloc(1, sizeof(struct msgchannel));
    channel->transmitter = mtp;
    channel->status = CHANNEL_STATUS_OPEN;
    channel->listener = mtp->listener;
    channel->addr = *addr;
    channel->timer = heap_create(UNIT_GROUP_SIZE);
    channel->mtx = ___mutex_create();

    tree_inseart(mtp->peers, channel->addr.key, channel->addr.keylen, channel);
    transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
    unit->head.type = TRANSUNIT_PING;
    unit->head.body_size = 0;
    unit->head.maximal = 1;
    channel_push_unit(channel, unit);

    return channel;
}

static inline void msgtransmitter_disconnect(msgtransmitter_ptr mtp, msgchannel_ptr pptr)
{

}

static inline void msgtransmitter_send(msgtransmitter_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
{
    void *group_data;
    size_t group_size;
    size_t group_count = size / (UNIT_GROUP_BUF_SIZE);
    size_t last_group_size = size - (group_count * UNIT_GROUP_BUF_SIZE);
    uint32_t unit_count;
    uint8_t last_unit_size;
    uint8_t last_unit_id;

    for (int x = 0; x < group_count; ++x){
        group_size = UNIT_GROUP_BUF_SIZE;
        group_data = data + x * UNIT_GROUP_BUF_SIZE;
        for (size_t y = 0; y < UNIT_GROUP_SIZE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
            memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->chennel = channel;
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.maximal = UNIT_GROUP_SIZE - y;
            channel_push_unit(channel, unit);
        }
    }

    group_data = data + (size - last_group_size);
    group_size = last_group_size;

    unit_count = group_size / UNIT_BODY_SIZE;
    last_unit_size = group_size - unit_count * UNIT_BODY_SIZE;
    last_unit_id = 0;

    if (last_unit_size > 0){
        last_unit_id = 1;
        unit_count += 1;
    }

    for (size_t y = 0; (y + last_unit_id) < unit_count; y++){
        transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
        unit->chennel = channel;
        unit->head.type = TRANSUNIT_MSG;
        unit->head.body_size = UNIT_BODY_SIZE;
        unit->head.maximal = last_unit_id - y;
        channel_push_unit(channel, unit);
    }

    if (last_unit_id){
        transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        memcpy(unit->body, group_data + ((unit_count - 1) * UNIT_BODY_SIZE), last_unit_size);
        unit->head.body_size = last_unit_size;
        unit->chennel = channel;
        unit->head.maximal = 1;
        channel_push_unit(channel, unit);
    }
}



#endif //__MESSAGE_TRANSFER_PROTOCOL_H__