#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"

#include <sys/struct/xheap.h>
#include <sys/struct/xtree.h>
#include <sys/struct/xbuf.h>



enum {
    XMSG_PACK_BYE = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_ACK = 0x02,
    XMSG_PACK_PING = 0x04,
    XMSG_PACK_PONG = 0x08,
    XMSG_PACK_FINAL = 0x10
};


#define PACK_HEAD_SIZE              16
// #define PACK_BODY_SIZE              1280 // 1024 + 256
#define PACK_BODY_SIZE              32
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_WINDOW_RANGE           16

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;
typedef struct xchannellist* xchannellist_ptr;
typedef struct xmsglistener* xmsglistener_ptr;


typedef struct xmsghead {
    uint8_t type;
    uint8_t sn; //serial number 包序列号
    uint8_t ack; //ack 单个包的 ACK
    uint8_t acks; //acks 这个包和它之前的所有包的 ACK
    uint8_t resend, x, y, z; //这个包的重传计数
    uint32_t cid;
    uint16_t pack_range; // message fragment count 消息分包数量
    uint16_t pack_size; // message fragment length 这个分包的长度
}*xmsghead_ptr;

typedef struct xmsgpack {
    bool comfirmed;
    bool flushing;
    struct xheapnode ts;
    uint64_t timestamp; //计算往返耗时
    xchannel_ptr channel;
    struct xmsgpack *prev, *next;
    struct xmsghead head;
    uint8_t body[PACK_BODY_SIZE];
}*xmsgpack_ptr;

struct xpacklist {
    uint8_t len;
    struct xmsgpack head, end;
};

typedef struct xmsgpackbuf {
    uint8_t range;
    __atom_size upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgpackbuf_ptr;

enum {
    XMSG_DGRAM = 0,
    XMSG_STREAM
};

typedef struct xmsg {
    uint64_t type; //XMSG_DGRAM or XMSG_STREAM
    xchannel_ptr channel; //如果 channel 是 NULL，这个 msg 是一个创建连接的 PING
    size_t wpos, rpos, len, range;
    void *addr;
    uint8_t data[1];
}*xmsg_ptr;


typedef struct xmsgbuf {
    // struct xmsg msg;
    uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgbuf_ptr;


typedef struct file_ctx {
    xchannel_ptr channel;
    size_t pos, len;
    __xfile_ptr fp;
}file_ctx_t;


struct xchannel {
    uint8_t key;
    uint32_t cid;
    uint32_t peer_cid;
    uint32_t peer_key;
    xchannel_ptr prev, next;
    __atom_bool sending;
    __atom_bool breaker;
    __atom_bool connected;
    // __atom_bool termination;
    __atom_size pos, len;
    uint64_t timestamp;
    uint64_t update;
    struct xmsghead ack;
    xmsgbuf_ptr msgbuf;
    // xchannellist_ptr queue;
    struct __xipaddr addr;
    xmsgpackbuf_ptr sendbuf;
    xmsger_ptr msger;
    xpipe_ptr msgqueue;
    xmsg_ptr msg;

    // TODO 用2个单向链表管理 message 和 stream
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head, end;
}*xchannellist_ptr;


struct xmsglistener {
    void *ctx;
    // TODO 使用cid替换channel指针
    void (*onConnectionToPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onConnectionFromPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onDisconnection)(struct xmsglistener*, xchannel_ptr channel);
    void (*onMessageFromPeer)(struct xmsglistener*, xchannel_ptr channel, xmsg_ptr msg);
    void (*onMessageToPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg);
};


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      10000000000ULL

struct xmsger {
    int sock;
    __atom_size pos, len;
    // 每次创建新的channel时加一
    uint32_t cid;
    xtree peers;
    //所有待重传的 pack 的定时器，不区分链接
    xtree timers;
    __xmutex_ptr mtx;
    __atom_bool running;
    //socket可读或者有数据待发送时为true
    __atom_bool working;
    __atom_bool readable;
    xmsglistener_ptr listener;
    // xmsgsocket_ptr msgsock;
    // xtask_ptr mainloop_task;
    // __atom_size tasklen;
    // __atom_bool connection_buf_lock;
    // xmsgpackbuf_ptr connection_buf;
    xpipe_ptr mpipe, spipe, rpipe;
    __xprocess_ptr mpid, rpid, spid;
    // 定时队列是自然有序的，不需要每次排序，所以使用链表效率最高
    struct xpacklist flushlist;
    struct xchannellist squeue, timed_queue;
};


#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - 1 - (b)->wpos + (b)->rpos))

#define XMSG_KEY    'x'
#define XMSG_VAL    'X'

static inline void xmsger_enqueue_channel(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    queue->len ++;
    channel->sending = true;
}

static inline void xmsger_dequeue_channel(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->prev->next = channel->next;
    channel->next->prev = channel->prev;
    queue->len --;
    channel->sending = false;
}

static inline void xchannel_pause(xchannel_ptr channel)
{
    xmsger_dequeue_channel(&channel->msger->squeue, channel);
    xmsger_enqueue_channel(&channel->msger->timed_queue, channel);
}

static inline void xchannel_resume(xchannel_ptr channel)
{
    xmsger_dequeue_channel(&channel->msger->timed_queue, channel);
    xmsger_enqueue_channel(&channel->msger->squeue, channel);
}


//xchannel_update_buf
// static inline void xchannel_pull(xchannel_ptr channel, xmsgpack_ptr ack)

static inline void xchannel_free_msg(xmsg_ptr msg)
{
    free(msg);
}

static inline xmsgpack_ptr make_pack(xchannel_ptr channel, uint8_t type)
{
    xmsgpack_ptr pack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    // 设置包类型
    pack->head.type = type;
    // 设置消息封包数量
    pack->head.pack_range = 0;
    // 设置对方 cid
    pack->head.cid = channel->peer_cid;
    // 设置校验码
    pack->head.x = XMSG_VAL ^ channel->peer_key;
    // 设置是否附带 ACK 标志
    pack->head.y = 0;
    return pack;
}

// static inline void xchannel_send_msg(xchannel_ptr channel, xmsg_ptr msg)
// {
//     __xlogd("xchannel_send_msg data enter\n");
//     while (msg->wpos < msg->len)
//     {
//         if (__transbuf_writable(channel->sendbuf) == 0){
//             break;
//         }
//         xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//         if (msg->len - msg->wpos < PACK_BODY_SIZE){
//             unit->head.pack_size = msg->len - msg->wpos;
//         }else{
//             unit->head.pack_size = PACK_BODY_SIZE;
//         }
//         unit->head.pack_range = msg->range;
//         mcopy(unit->body, msg->addr + msg->wpos, unit->head.pack_size);
//         xchannel_push(channel, unit);
//         msg->wpos += unit->head.pack_size;
//         msg->range --;
//         __xlogd("xchannel_send_msg data -------------------------------------------------------------- len: %lu wpos: %lu range %u\n", msg->len, msg->wpos, msg->range);
//     }
//     __xlogd("xchannel_send_msg data exit -------------------------------------------------------------- len: %lu wpos: %lu range %u\n", msg->len, msg->wpos, msg->range);
// }

// extern int64_t xchannel_send(xchannel_ptr channel, xmsghead_ptr ack);

// extern void xchannel_recv(xchannel_ptr channel, xmsgpack_ptr unit);

// static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr)
// {
//     xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
//     channel->connected = false;
//     // channel->bye = false;
//     channel->breaker = false;
//     channel->update = __xapi->clock();
//     channel->msger = msger;
//     channel->addr = *addr;
//     channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
//     channel->msgbuf->range = PACK_WINDOW_RANGE;
//     channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
//     channel->sendbuf->range = PACK_WINDOW_RANGE;
//     channel->msgqueue = xpipe_create(8 * __sizeof_ptr);
//     channel->peer_cid = 0;
//     while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
//         if (++msger->cid == 0){
//             msger->cid = 1;
//         }
//     }
//     channel->cid = msger->cid;
//     channel->key = msger->cid % 255;

//     // TODO 是否要在创建连接时就加入发送队列，因为这时连接中还没有消息要发送
//     xmsger_enqueue_channel(&msger->squeue, channel);
//     return channel;
// }

// static inline void xchannel_free(xchannel_ptr channel)
// {
//     __xlogd("xchannel_free enter\n");
//     __atom_sub(channel->msger->len, channel->len - channel->pos);
//     if (channel->sending){
//         xmsger_dequeue_channel(&channel->msger->squeue, channel);
//     }else {
//         xmsger_dequeue_channel(&channel->msger->timed_queue, channel);
//     }
//     xpipe_free(&channel->msgqueue);
//     free(channel->msgbuf);
//     free(channel->sendbuf);
//     free(channel);
//     __xlogd("xchannel_free exit\n");
// }

// static inline void xmsger_run(xmsger_ptr messenger)
// {
//     messenger->mainloop_task = xtask_run(xmsger_loop, messenger);
// }



static inline void xmsger_disconnect(xmsger_ptr mtp, xchannel_ptr channel)
{
    __xlogd("xmsger_disconnect enter");
    xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    unit->head.type = XMSG_PACK_BYE;
    unit->head.pack_size = 0;
    unit->head.pack_range = 1;
    //主动发送 BEY，要设置 channel 主动状态。
    __set_true(channel->breaker);
    //向对方发送 BEY，对方回应后，再移除 channel。
    // __set_true(channel->bye);
    //主动断开的一端，发送第一个 BEY，并且启动超时重传。
    // xchannel_push(channel, unit);
    __xlogd("xmsger_disconnect exit");
}

//ping 需要发送加密验证，如果此时链接已经断开，ping 消息可以进行重连。
static inline void xmsger_ping(xchannel_ptr channel)
{
    __xlogd("xmsger_ping enter");
    __xlogd("xmsger_ping exit");
}

extern bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, const char *addr, uint16_t port);
extern xmsger_ptr xmsger_create(xmsglistener_ptr listener);
extern void xmsger_free(xmsger_ptr *pptr);

// static inline void xmsger_send(xmsger_ptr mtp, xchannel_ptr channel, void *data, size_t size)
// {
//     // 非阻塞发送，当网络可发送时，通知用户层。
//     char *msg_data;
//     size_t msg_size;
//     size_t msg_count = size / (XMSG_MAXIMUM_LENGTH);
//     size_t last_msg_size = size - (msg_count * XMSG_MAXIMUM_LENGTH);

//     uint16_t unit_count;
//     uint16_t last_unit_size;
//     uint16_t last_unit_id;

//     for (int x = 0; x < msg_count; ++x){
//         msg_size = XMSG_MAXIMUM_LENGTH;
//         msg_data = ((char*)data) + x * XMSG_MAXIMUM_LENGTH;
//         for (size_t y = 0; y < XMSG_PACK_RANGE; y++)
//         {
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = PACK_BODY_SIZE;
//             unit->head.pack_range = XMSG_PACK_RANGE - y;
//             xchannel_push(channel, unit);
//         }
//     }

//     if (last_msg_size){
//         msg_data = ((char*)data) + (size - last_msg_size);
//         msg_size = last_msg_size;

//         unit_count = msg_size / PACK_BODY_SIZE;
//         last_unit_size = msg_size - unit_count * PACK_BODY_SIZE;
//         last_unit_id = 0;

//         if (last_unit_size > 0){
//             last_unit_id = 1;
//         }

//         for (size_t y = 0; y < unit_count; y++){
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (y * PACK_BODY_SIZE), PACK_BODY_SIZE);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = PACK_BODY_SIZE;
//             unit->head.pack_range = (unit_count + last_unit_id) - y;
//             xchannel_push(channel, unit);
//         }

//         if (last_unit_id){
//             // xmsgpack_ptr unit = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
//             xmsgpack_ptr unit = make_pack(channel, XMSG_PACK_MSG);
//             mcopy(unit->body, msg_data + (unit_count * PACK_BODY_SIZE), last_unit_size);
//             // unit->head.type = XMSG_PACK_MSG;
//             unit->head.pack_size = last_unit_size;
//             unit->head.pack_range = last_unit_id;
//             xchannel_push(channel, unit);
//         }
//     }
// }

//make_channel(addr, 验证数据) //channel 在外部线程创建
//clear_channel(ChannelPtr) //在外部线程销毁


#endif //__XMSGER_H__