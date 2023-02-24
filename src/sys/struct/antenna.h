#ifndef __ANTENNA_H__
#define __ANTENNA_H__


#include "env/env.h"
#include <env/malloc.h>
#include <sys/struct/heap.h>
#include <sys/struct/tree.h>

enum {
    MSG_NONE = 0x00,
    MSG_REQ = 0x01,
    MSG_ACK = 0x02,
    MSG_CONNECT = 0x04,
    MSG_BIND = 0x08,
    MSG_PING = 0x10,
    MSG_PACKET = 0x20,
    MSG_DISCONNECT = 0xc0
};

typedef struct msghead {
    uint8_t type;
    uint8_t flag;
    uint8_t group;
    uint8_t serial;
    uint16_t size;
    uint8_t ack_group;
    uint8_t ack_serial;
}*msghead_ptr;

#define MSG_HEAD_SIZE       64
#define MSG_PAYLOAD_SIZE    1324
#define MSG_PACKET_SIZE     1388
#define MSG_GROUP_SIZE      256

typedef struct msgchannel* msgchannel_ptr;

typedef struct msgaddr {
    void *byte;
    size_t size;
}*msgaddr_ptr;

typedef struct msgpack {
    msgchannel_ptr chennel;
    struct msghead head;
    uint8_t msgbody[MSG_PAYLOAD_SIZE];
}*msgpack_ptr;


typedef struct msgbuf {
    uint8_t rpos, wpos, sendpos;
    struct msgpack *buf[MSG_GROUP_SIZE];
}*msgbuf_ptr;

typedef struct msgchannel_listener {
    void *user_private_ctx;
    void (*connected)(void *ctx, msgchannel_ptr channel);
    void (*disconnected)(void *ctx, msgchannel_ptr channel);
    void (*message_arrived)(void *ctx, msgchannel_ptr channel, void *data, size_t size);
    void (*update_status)(void *ctx, msgchannel_ptr channel);
}*msgchannel_listener_ptr;

typedef struct msgchannel {
    struct msgaddr addr;
    linekey_ptr key;
    struct msgbuf sendbuf, recvbuf;
    msgchannel_listener_ptr listener;
}msgchannel_t;

typedef struct physics_socket {
    int socket;
    size_t (*send)(int socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*receive)(int socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;

typedef struct channelbuf {
    uint8_t rpos, wpos;
    msgchannel_ptr buf[MSG_GROUP_SIZE];
}channelbuf_t;

typedef struct sendingpkg {
    
}sendingpkg_t;

typedef struct antenna {
    uint32_t sending, receiving;
    heap_t *timer;
    __tree peers;
    struct msgaddr addr;
    channelbuf_t channelbuf;
    physics_socket_ptr device;
}*antenna_ptr;

typedef struct antenna** antenna_pptr;

static inline antenna_ptr antenna_create(physics_socket_ptr device)
{
    antenna_ptr antenna = (antenna_ptr)calloc(1, sizeof(struct antenna));
    antenna->sending = 0;
    antenna->device = device;
    antenna->timer = heap_create(1024);
    antenna->peers = tree_create();
    return antenna;
}

static inline void antenna_release(antenna_pptr pptr)
{

}

static inline void antenna_connect(antenna_ptr antenna, msgaddr_ptr addr, msgchannel_listener_ptr listener, void *ctx)
{
    msgchannel_ptr channel = (msgchannel_ptr)malloc(sizeof(msgchannel_t));
    channel->addr.size = addr->size;
    // channel->addr.byte = malloc(addr->size);
    // memcpy(channel->addr.byte, addr->byte, addr->size);
    channel->key = linekey_from_data(addr->byte, addr->size);
    tree_inseart(antenna->peers, channel->key, channel);
}

static inline void antenna_disconnect(antenna_ptr antenna, msgchannel_ptr pptr)
{

}

static inline void antenna_send_message(antenna_ptr antenna, msgchannel_ptr ptr, void *data, size_t size)
{

}

static inline void antenna_start_receive(antenna_ptr antenna)
{
    int ret = 0;
    __set_true(antenna->sending);
    while (__is_true(antenna->sending))
    {
        msgpack_ptr pkt = (msgpack_ptr)malloc(MSG_PACKET_SIZE);
        ret = antenna->device->receive(antenna->device->socket, &antenna->addr, &pkt, MSG_PACKET_SIZE);
        __logi("antenna_start_receive packet size %d", ret);
    }
}

#include <time.h>
static inline uint64_t antenna_start_send(antenna_ptr antenna)
{
    int ret;
    heapment_t timer;
    msgchannel_ptr chennel = NULL;

    antenna->sending = antenna->receiving = 0;

    while (antenna->receiving < 3 
        || ((antenna->channelbuf.wpos - antenna->channelbuf.rpos > 0) || chennel != NULL) 
        || (antenna->timer->pos > 0 && antenna->timer->array[1].key <= env_time()))
    {
        msgpack_ptr pkt = (msgpack_ptr)malloc(MSG_PACKET_SIZE);
        ret = antenna->device->receive(antenna->device->socket, &antenna->addr, &pkt, MSG_PACKET_SIZE);
        if (ret < 0){
            antenna->receiving ++;
            free(pkt);
        }

        if (chennel == NULL && (antenna->channelbuf.wpos - antenna->channelbuf.rpos) > 0){
            chennel = antenna->channelbuf.buf[antenna->channelbuf.rpos];
        }else {
            antenna->sending ++;
            nanosleep((struct timespec[]){{1, 1000L}}, NULL);
            __logi("antenna_start_send packet size %d", antenna->channelbuf.wpos);
        }

        while (chennel)
        {
            if ((chennel->sendbuf.wpos - chennel->sendbuf.sendpos) > 0){
                ret = antenna->device->send(antenna->device->socket, &chennel->addr, 
                    (void*)&(chennel->sendbuf.buf[chennel->sendbuf.sendpos]->head), 
                    MSG_HEAD_SIZE + chennel->sendbuf.buf[chennel->sendbuf.sendpos]->head.size);

                if (ret > 0){
                    chennel->sendbuf.buf[chennel->sendbuf.sendpos]->chennel = chennel;
                    timer.key = env_time();
                    timer.value = chennel->sendbuf.buf[chennel->sendbuf.sendpos];
                    min_heapify_push(antenna->timer, timer);
                    chennel->sendbuf.sendpos ++;
                }else {
                    antenna->sending ++;
                    break;
                }

            }else {
                antenna->channelbuf.rpos ++;
                chennel = NULL;
            }
        }

        while (antenna->timer->pos > 0 && antenna->timer->array[1].key <= env_time())
        {
            timer = min_heapify_pop(antenna->timer);
            msgpack_ptr pkt = (msgpack_ptr)timer.value;
            if (pkt->chennel->sendbuf.buf[pkt->head.serial] != NULL){
                ret = antenna->device->send(antenna->device->socket, &pkt->chennel->addr, 
                        (void*)&(pkt->head), MSG_HEAD_SIZE + pkt->head.size);

                timer.key = env_time();
                timer.value = pkt;
                min_heapify_push(antenna->timer, timer);

                if (ret < 0){
                    antenna->sending ++;
                    break;
                }
            }else {
                free(pkt);
            }
        }
    }

    if ((antenna->timer->pos > 0)){
        return antenna->timer->array[1].key - env_time();
    }

    return 0;

}


#endif //__ANTENNA_H__