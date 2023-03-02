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

#define UNIT_HEAD_SIZE      64
#define UNIT_BODY_SIZE      1324
#define UNIT_TOTAL_SIZE     1388
#define UNIT_GROUP_SIZE     256

typedef struct transchannel* transchannel_ptr;

typedef struct transaddr {
    size_t size;
    void *byte;
}*transaddr_ptr;

typedef struct transunit {
    transchannel_ptr chennel;
    struct {
        uint16_t type;
        uint16_t size;
        uint8_t group;
        uint8_t serial;
        uint8_t ack_group;
        uint8_t ack_serial;
    } head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;


typedef struct transunitbuf {
    ___atom_8bit rpos, wpos, sendpos;
    struct transunit *buf[UNIT_GROUP_SIZE];
}*transunitbuf_ptr;

typedef struct transchannel_listener {
    void *ctx;
    void (*connected)(struct transchannel_listener*, transchannel_ptr channel);
    void (*disconnected)(struct transchannel_listener*, transchannel_ptr channel);
    void (*message_arrived)(struct transchannel_listener*, transchannel_ptr channel, void *data, size_t size);
    void (*update_status)(struct transchannel_listener*, transchannel_ptr channel);
}*transchannel_listener_ptr;

typedef struct transchannel {
    struct transaddr addr;
    linekey_ptr key;
    struct transunitbuf sendbuf, recvbuf;
    transchannel_listener_ptr listener;
}transchannel_t;

typedef struct physics_socket {
    int socket;
    size_t (*send)(int socket, transaddr_ptr addr, void *data, size_t size);
    size_t (*receive)(int socket, transaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;

typedef struct transchannelbuf {
    ___atom_8bit rpos, wpos;
    transchannel_ptr buf[UNIT_GROUP_SIZE];
}transchannelbuf_t;

typedef struct accept_listener {
    void *ctx;
    transchannel_listener_ptr (*accept)(struct accept_listener*);
}*accept_listener_ptr;

typedef struct mtp {
    ___atom_bool running;
    ___atom_bool watting;
    ___atom_bool readable;
    ___atom_size sendable;
    heap_t *timer;
    __tree peers;
    ___mutex_ptr mtx;
    struct transaddr addr;
    transchannelbuf_t channelbuf;
    physics_socket_ptr device;
    transchannel_listener_ptr listener;
}*mtp_ptr;

typedef struct mtp** mtp_pptr;

static inline mtp_ptr mtp_create(physics_socket_ptr device)
{
    mtp_ptr mtp = (mtp_ptr)calloc(1, sizeof(struct mtp));
    mtp->sendable = 0;
    mtp->device = device;
    mtp->timer = heap_create(1024);
    mtp->peers = tree_create();
    mtp->mtx = ___mutex_create();
    return mtp;
}

static inline void mtp_release(mtp_pptr pptr)
{

}

static inline void mtp_connect(mtp_ptr mtp, transaddr_ptr addr)
{
    transchannel_ptr channel = (transchannel_ptr)malloc(sizeof(transchannel_t));
    channel->addr = *addr;
    tree_inseart(mtp->peers, channel->addr.byte, channel);
}

static inline void mtp_disconnect(mtp_ptr mtp, transchannel_ptr pptr)
{

}

static inline void mtp_send(mtp_ptr mtp, transchannel_ptr ptr, void *data, size_t size)
{
    if (___set_false(&mtp->watting)){
        ___lock lk = ___mutex_lock(mtp->mtx);
        ___mutex_notify(mtp->mtx);
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
    transunit_ptr pkt = NULL;
    transchannel_ptr chennel = NULL;
    
    mtp->listener = listener;
    mtp->readable = 0;
    mtp->sendable = false;
    
    ___set_true(&mtp->running);

    while (___is_true(&mtp->running))
    {
        if (___is_true(&mtp->readable)){
            if (pkt == NULL){
                pkt = (transunit_ptr)malloc(UNIT_TOTAL_SIZE);
            }
            ret = mtp->device->receive(mtp->device->socket, &mtp->addr, &pkt, UNIT_TOTAL_SIZE);
            if (ret > 0){
                transchannel_ptr ch = (transchannel_ptr)tree_find(mtp->peers, mtp->addr.byte);
                if (ch){
                    if (pkt->head.type & TRANSUNIT_ACK){
                        ret = mtp->device->send(mtp->device->socket, &pkt->chennel->addr, 
                                    (void*)&(pkt->head), UNIT_HEAD_SIZE + pkt->head.size);
                    }else {
                        ch->listener->message_arrived(ch->listener, ch, pkt->body, ret - UNIT_HEAD_SIZE);
                    }
                }else {
                    transchannel_ptr ch = (transchannel_ptr)malloc(sizeof(struct transchannel));
                    ch->addr = mtp->addr;
                    mtp->listener->message_arrived(ch->listener, ch, pkt->body, ret - UNIT_HEAD_SIZE);
                }
                pkt = NULL;
            }else {
                ___set_false(&mtp->sendable);
            }

            while (mtp->timer->pos > 0 && (mtp->timer->array[1].key <= ___sys_clock()))
            {                
                transunit_ptr pkt = (transunit_ptr)mtp->timer->array[1].value;
                if (pkt->chennel->sendbuf.buf[pkt->head.serial] != NULL){
                    ret = mtp->device->send(mtp->device->socket, &pkt->chennel->addr, 
                            (void*)&(pkt->head), UNIT_HEAD_SIZE + pkt->head.size);
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
        }

        if (chennel == NULL && (mtp->channelbuf.wpos - mtp->channelbuf.rpos) > 0){
            chennel = mtp->channelbuf.buf[mtp->channelbuf.rpos];
        }

        while (chennel)
        {
            if ((chennel->sendbuf.wpos - chennel->sendbuf.sendpos) > 0){
                ret = mtp->device->send(mtp->device->socket, &chennel->addr, 
                    (void*)&(chennel->sendbuf.buf[chennel->sendbuf.sendpos]->head), 
                    UNIT_HEAD_SIZE + chennel->sendbuf.buf[chennel->sendbuf.sendpos]->head.size);

                if (ret > 0){
                    chennel->sendbuf.buf[chennel->sendbuf.sendpos]->chennel = chennel;
                    timer.key = ___sys_clock();
                    timer.value = chennel->sendbuf.buf[chennel->sendbuf.sendpos];
                    min_heapify_push(mtp->timer, timer);
                    chennel->sendbuf.sendpos ++;
                }else {
                    mtp->sendable ++;
                    break;
                }

            }else {
                mtp->channelbuf.rpos ++;
                chennel = NULL;
            }
        }

        if (chennel == NULL && (mtp->channelbuf.wpos - mtp->channelbuf.rpos) == 0 && ___is_false(&mtp->readable)){
            ___lock lk = ___mutex_lock(mtp->mtx);
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
            ___mutex_unlock(mtp->mtx, lk);
        }
    }

    return 0;
}


#endif //__MESSAGE_TRANSFER_PROTOCOL_H__