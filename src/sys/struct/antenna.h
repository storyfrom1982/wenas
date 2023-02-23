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

typedef struct msgpack {
    struct msghead head;
    uint8_t msgbody[MSG_PAYLOAD_SIZE];
}*msgpack_ptr;


typedef struct msgbuf {
    uint8_t rpos, wpos;
    struct msgpack *buf[MSG_GROUP_SIZE];
}*msgbuf_ptr;

typedef struct msgaddr {
    void *byte;
    size_t size;
}*msgaddr_ptr;

typedef struct msgchannel* msgchannel_ptr;

typedef struct msgchannel_listener {
    void (*connected)(void *ctx, msgchannel_ptr channel);
    void (*disconnected)(void *ctx, msgchannel_ptr channel);
    void (*message_arrived)(void *ctx, msgchannel_ptr channel, void *data, size_t size);
    void (*update_status)(void *ctx, msgchannel_ptr channel);
}*msgchannel_listener_ptr;

typedef struct msgchannel {
    struct msgaddr addr;
    struct msgbuf sendbuf, recvbuf;
    msgchannel_listener_ptr listener;
}msgchannel_t;

typedef struct physics_socket {
    int socket;
    size_t (*send)(int socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*receive)(int socket, msgaddr_ptr addr, void *data, size_t size);
}physics_socket_ptr;

typedef struct antenna {
    heap_t *timer;
    __tree peers;
    physics_socket_ptr device;
}*antenna_ptr;

typedef struct antenna** antenna_pptr;

static inline antenna_ptr antenna_create(physics_socket_ptr device)
{
    antenna_ptr antenna = (antenna_ptr)malloc(sizeof(struct antenna));
    antenna->device = device;
    antenna->timer = heap_create(1024);
    return antenna;
}

static inline void antenna_release(antenna_pptr pptr)
{

}

static inline void antenna_connect(antenna_ptr antenna, msgaddr_ptr addr, msgchannel_listener_ptr listener, void *ctx)
{
    
}

static inline void antenna_disconnect(antenna_ptr antenna, msgchannel_ptr pptr)
{

}

static inline void antenna_send_message(antenna_ptr antenna, msgchannel_ptr ptr, void *data, size_t size)
{

}


#endif //__ANTENNA_H__