#ifndef __MESSAGE_TRANSFER_PROTOCOL_H__
#define __MESSAGE_TRANSFER_PROTOCOL_H__


#include "env/env.h"
#include <env/task.h>
#include <env/malloc.h>
#include <sys/struct/heap.h>
#include <sys/struct/tree.h>

enum {
    TRANSUNIT_NONE = 0x00,
    TRANSUNIT_REQ = 0x01,
    TRANSUNIT_ACK = 0x02,
    TRANSUNIT_CONNECT = 0x04,
    TRANSUNIT_DISCONNECT = 0x08,
    TRANSUNIT_BIND = 0x10,
    TRANSUNIT_PING = 0x20,
    TRANSUNIT_GROUP = 0x40,
    TRANSUNIT_SERIAL = 0x80
};

#define UNIT_HEAD_SIZE          64
#define UNIT_BODY_SIZE          1324
#define UNIT_TOTAL_SIZE         1388
#define UNIT_GROUP_SIZE         256
#define UNIT_GROUP_BUF_SIZE     (UNIT_BODY_SIZE * UNIT_GROUP_SIZE)

typedef struct transchannel* transchannel_ptr;

typedef struct transaddr {
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
}*transaddr_ptr;

typedef struct transunit {
    transchannel_ptr chennel;
    struct {
        uint16_t type;
        uint16_t body_size;
        uint8_t group_id;
        uint8_t group_descending;
        uint8_t serial_number;
        uint8_t ack_serial_number;
    } head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;


typedef struct transunitbuf {
    ___atom_8bit rpos, wpos, reading;
    struct transunit *buf[UNIT_GROUP_SIZE];
}*transunitbuf_ptr;

typedef struct transgroup {
    size_t group_size;
    char *group_buffer;
    uint8_t group_descending;
    ___atom_8bit rpos, wpos;
    struct transunit *serialbuf[UNIT_GROUP_SIZE];
}*transgroup_ptr;


enum {
    CHANNEL_STATUS_OPEN = 0,
    CHANNEL_STATUS_CONNECTING = 1,
    CHANNEL_STATUS_CONNECT = 2,
    CHANNEL_STATUS_BINDING = 3,
    CHANNEL_STATUS_BIND = 4,
};

struct transchannel {
    int status;
    ___atom_bool sending;
    ___mutex_ptr mtx;
    struct transaddr addr;
    struct transunitbuf sendbuf;
    struct transgroup recvbuf;
    struct transchannel *prev, *next;
};

typedef struct transchannel_listener {
    void *ctx;
    void (*connected)(struct transchannel_listener*, transchannel_ptr channel);
    void (*disconnected)(struct transchannel_listener*, transchannel_ptr channel);
    void (*message_arrived)(struct transchannel_listener*, transchannel_ptr channel, void *data, size_t size);
    void (*update_status)(struct transchannel_listener*, transchannel_ptr channel);
}*transchannel_listener_ptr;

typedef struct physics_socket {
    int socket;
    size_t (*send)(int socket, transaddr_ptr addr, void *data, size_t size);
    size_t (*receive)(int socket, transaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;

typedef struct mtp {
    ___atom_bool running;
    ___atom_bool watting;
    ___atom_bool readable;
    ___atom_size sendable;
    heap_t *timer;
    __tree peers;
    ___mutex_ptr mtx;
    struct transaddr addr;
    physics_socket_ptr device;
    transchannel_listener_ptr listener;
    struct transchannel head, end;
}*mtp_ptr;

typedef struct mtp** mtp_pptr;


static inline void transunitbuf_clear(transunitbuf_ptr buf)
{
    memset(buf, 0, sizeof(struct transunitbuf));
}

static inline void channel_put_unit(transchannel_ptr channel, transunit_ptr unit)
{
    while (UNIT_GROUP_SIZE - channel->sendbuf.wpos + channel->sendbuf.rpos == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        ___mutex_wait(channel->mtx, lk);
        ___mutex_unlock(channel->mtx, lk);
    }
    unit->head.serial_number = channel->sendbuf.wpos;
    channel->sendbuf.buf[channel->sendbuf.wpos] = unit;
    ___atom_add(&channel->sendbuf.wpos, 1);
    ___mutex_notify(channel->mtx);
}

static inline transunit_ptr channel_get_unit(transchannel_ptr channel)
{
    if (channel->sendbuf.wpos - channel->sendbuf.reading > 0){
        return channel->sendbuf.buf[channel->sendbuf.reading++];
    }else {
        return NULL;
    }
}

static inline void channel_free_unit(transchannel_ptr channel, transunit_ptr unit)
{
    unit->chennel = NULL;
    if (unit == channel->sendbuf.buf[channel->sendbuf.rpos]){
        channel->sendbuf.buf[channel->sendbuf.rpos] = NULL;
        ___atom_add(&channel->sendbuf.rpos, 1);
        while (channel->sendbuf.buf[channel->sendbuf.rpos]->chennel == NULL){
            channel->sendbuf.buf[channel->sendbuf.rpos] = NULL;
            ___atom_add(&channel->sendbuf.rpos, 1);
        }
    }
    ___mutex_notify(channel->mtx);
}

static inline void transgroupbuf_clear(transgroup_ptr group)
{
    memset(group, 0, sizeof(struct transgroup));
}

static inline void channel_write_group(transchannel_ptr channel, transunit_ptr unit)
{
    int serial_gap = unit->head.serial_number - channel->recvbuf.wpos;

    if (serial_gap < 0){
        channel->recvbuf.serialbuf[unit->head.serial_number] = unit;
    }else if (serial_gap > 0){
        channel->recvbuf.serialbuf[unit->head.serial_number] = unit;
        channel->recvbuf.wpos = unit->head.serial_number + 1;
    }else {
        channel->recvbuf.serialbuf[channel->recvbuf.wpos] = unit;
        channel->recvbuf.wpos++;
    }

    while (channel->recvbuf.serialbuf[channel->recvbuf.rpos] != NULL){
        if (channel->recvbuf.group_buffer == NULL){
            channel->recvbuf.group_descending = channel->recvbuf.serialbuf[channel->recvbuf.rpos]->head.group_descending;
            channel->recvbuf.group_size = 0;
            channel->recvbuf.group_buffer = (char*)malloc(channel->recvbuf.group_descending * UNIT_BODY_SIZE);
        }
        memcpy(channel->recvbuf.group_buffer + channel->recvbuf.group_size, 
            channel->recvbuf.serialbuf[channel->recvbuf.rpos]->body, 
            channel->recvbuf.serialbuf[channel->recvbuf.rpos]->head.body_size);
        channel->recvbuf.group_size += channel->recvbuf.serialbuf[channel->recvbuf.rpos]->head.body_size;
        channel->recvbuf.group_descending--;
        if (channel->recvbuf.group_descending == 0){
            channel->recvbuf.group_buffer = NULL;
        }
        free(channel->recvbuf.serialbuf[channel->recvbuf.rpos]);
        channel->recvbuf.rpos++;
    }
}

static inline mtp_ptr mtp_create(physics_socket_ptr device)
{
    mtp_ptr mtp = (mtp_ptr)calloc(1, sizeof(struct mtp));
    mtp->sendable = 0;
    mtp->device = device;
    mtp->timer = heap_create(1024);
    mtp->peers = tree_create();
    mtp->mtx = ___mutex_create();
    mtp->head.prev = NULL;
    mtp->end.next = NULL;
    mtp->head.next = &mtp->end;
    mtp->end.prev = &mtp->head;
    return mtp;
}

static inline void mtp_release(mtp_pptr pptr)
{

}

static inline void mtp_connect(mtp_ptr mtp, transaddr_ptr addr)
{
    transchannel_ptr channel = (transchannel_ptr)calloc(1, sizeof(struct transchannel));
    channel->status = CHANNEL_STATUS_OPEN;
    channel->addr = *addr;
    channel->mtx = ___mutex_create();
    tree_inseart(mtp->peers, channel->addr.key, channel->addr.keylen, channel);    
    transunitbuf_clear(&channel->sendbuf);
    transgroupbuf_clear(&channel->recvbuf);

    transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
    unit->head.type = TRANSUNIT_REQ | TRANSUNIT_CONNECT;
    unit->head.body_size = 0;
    unit->head.group_descending = 1;
    channel_put_unit(channel, unit);
    ___lock lk = ___mutex_lock(mtp->mtx);
    ___set_true(&channel->sending);
    channel->next = &mtp->end;
    channel->prev = mtp->end.prev;
    channel->prev->next = channel;
    channel->next->prev = channel;
    if (___set_false(&mtp->watting)){
        ___mutex_notify(mtp->mtx);
    }
    ___mutex_unlock(mtp->mtx, lk);
}

static inline void mtp_disconnect(mtp_ptr mtp, transchannel_ptr pptr)
{

}

static inline void mtp_send(mtp_ptr mtp, transchannel_ptr channel, void *data, size_t size)
{
    size_t group_count = size / (UNIT_GROUP_BUF_SIZE);
    size_t last_group_size = size - (group_count * UNIT_GROUP_BUF_SIZE);
    size_t group_size;
    void *group_data;

    for (int x = 0; x < group_count; ++x){
        group_size = UNIT_GROUP_BUF_SIZE;
        group_data = data + x * UNIT_GROUP_BUF_SIZE;
        for (size_t y = 0; y < UNIT_GROUP_SIZE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
            memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->chennel = channel;
            unit->head.type = TRANSUNIT_REQ | TRANSUNIT_GROUP;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.group_descending = UNIT_GROUP_SIZE - y;
            channel_put_unit(channel, unit);
        }

        if (___set_true(&channel->sending)){
            ___lock lk = ___mutex_lock(mtp->mtx);
            channel->next = &mtp->end;
            channel->prev = mtp->end.prev;
            channel->prev->next = channel;
            channel->next->prev = channel;
            if (___set_false(&mtp->watting)){
                ___mutex_notify(mtp->mtx);
            }
            ___mutex_unlock(mtp->mtx, lk);
        }
    }

    group_data = data + (size - last_group_size);
    group_size = last_group_size;

    uint32_t unit_count = group_size / UNIT_BODY_SIZE;
    uint8_t last_unit_size = group_size - unit_count * UNIT_BODY_SIZE;
    uint8_t last_unit_id = last_unit_size > 0 ? unit_count + 1 : unit_count;

    for (size_t y = 0; y < unit_count; y++)
    {
        transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        memcpy(unit->body, group_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
        unit->chennel = channel;
        unit->head.type = TRANSUNIT_REQ | TRANSUNIT_GROUP;
        unit->head.body_size = UNIT_BODY_SIZE;
        unit->head.group_descending = last_unit_id - y;
        channel_put_unit(channel, unit);
    }

    if (last_unit_id){
        transunit_ptr unit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        memcpy(unit->body, group_data + (last_unit_id * UNIT_BODY_SIZE), last_unit_size);
        unit->head.body_size = last_unit_size;
        unit->chennel = channel;
        unit->head.group_descending = last_unit_id - last_unit_id;
        channel_put_unit(channel, unit);
    }
    
    if (___set_true(&channel->sending)){
        ___lock lk = ___mutex_lock(mtp->mtx);
        channel->next = &mtp->end;
        channel->prev = mtp->end.prev;
        channel->prev->next = channel;
        channel->next->prev = channel;
        if (___set_false(&mtp->watting)){
            ___mutex_notify(mtp->mtx);
        }
        ___mutex_unlock(mtp->mtx, lk);
    }
}

static inline void mtp_update(mtp_ptr mtp)
{
    ___set_true(&mtp->readable);
    if (___set_false(&mtp->watting)){
        ___lock lk = ___mutex_lock(mtp->mtx);
        ___mutex_notify(mtp->mtx);
        ___mutex_unlock(mtp->mtx, lk);
    }
}

static inline int mtp_run(mtp_ptr mtp, transchannel_listener_ptr listener)
{
    int ret;
    uint64_t sleep_time;
    heapment_t timer;
    transunit_ptr recvunit = NULL;
    transunit_ptr sendunit = NULL;
    transchannel_ptr channel = NULL;
    
    mtp->listener = listener;
    mtp->readable = 0;
    mtp->sendable = false;
    
    ___set_true(&mtp->running);

    while (___is_true(&mtp->running))
    {
        if (recvunit == NULL){
            recvunit = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
        }
        ret = mtp->device->receive(mtp->device->socket, &mtp->addr, &recvunit, UNIT_TOTAL_SIZE);
        if (ret > 0){
            transchannel_ptr ch = (transchannel_ptr)tree_find(mtp->peers, mtp->addr.key, mtp->addr.keylen);
            if (ch){
                if (recvunit->head.type == (TRANSUNIT_REQ | TRANSUNIT_BIND)){
                    recvunit->head.type = TRANSUNIT_ACK | TRANSUNIT_BIND;
                    ret = mtp->device->send(mtp->device->socket, &recvunit->chennel->addr, 
                                (void*)&(recvunit->head), UNIT_HEAD_SIZE + recvunit->head.body_size);
                }else {
                    mtp->listener->message_arrived(mtp->listener, ch, recvunit->body, ret - UNIT_HEAD_SIZE);
                }
            }else {
                transchannel_ptr ch = (transchannel_ptr)malloc(sizeof(struct transchannel));
                ch->addr = mtp->addr;
                ch->status = CHANNEL_STATUS_CONNECTING;
                tree_inseart(mtp->peers, mtp->addr.key, mtp->addr.keylen, ch);
                recvunit->head.type = TRANSUNIT_ACK | TRANSUNIT_CONNECT;
                ret = mtp->device->send(mtp->device->socket, &ch->addr,
                    (void*)&(recvunit->head), UNIT_HEAD_SIZE + recvunit->head.body_size);
            }
            recvunit = NULL;
            ___set_true(&mtp->sendable);
        }else {
            ___set_false(&mtp->sendable);
        }

        while (mtp->timer->pos > 0 && (mtp->timer->array[1].key <= ___sys_clock()))
        {                
            transunit_ptr pkt = (transunit_ptr)mtp->timer->array[1].value;
            if (pkt->chennel->sendbuf.buf[pkt->head.serial_number] != NULL){
                ret = mtp->device->send(mtp->device->socket, &pkt->chennel->addr, 
                        (void*)&(pkt->head), UNIT_HEAD_SIZE + pkt->head.body_size);
                if (ret > 0){
                    min_heapify_pop(mtp->timer);
                    timer.key = ___sys_clock();
                    timer.value = pkt;
                    min_heapify_push(mtp->timer, timer);
                }else {
                    break;
                }

            }else {
                free(pkt);
            }
        }


        ___lock lk = ___mutex_lock(mtp->mtx);

        if (channel == NULL){
            channel = mtp->head.next;
        }
        
        while (channel != &mtp->end)
        {
            sendunit = channel_get_unit(channel);
            if (sendunit){
                ret = mtp->device->send(mtp->device->socket, &channel->addr, 
                    (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                if (ret > 0){
                    timer.key = ___sys_clock();
                    timer.value = channel->sendbuf.buf[channel->sendbuf.reading];
                    min_heapify_push(mtp->timer, timer);
                    channel_free_unit(channel, sendunit);
                }else {
                    ___set_false(&mtp->sendable);
                }
            }else {
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

        if (mtp->head.next != &mtp->end && ___is_false(&mtp->readable)){
            
            ___set_true(&mtp->watting);
            if (mtp->timer->pos){
                sleep_time = mtp->timer->array[1].key - ___sys_clock();
            }else {
                sleep_time = 0;
            }
            if (sleep_time){
                ___mutex_timer(mtp->mtx, lk, sleep_time);
            }else {
                ___mutex_wait(mtp->mtx, lk);
            }
        }

        ___mutex_unlock(mtp->mtx, lk);
    }

    return 0;
}


#endif //__MESSAGE_TRANSFER_PROTOCOL_H__