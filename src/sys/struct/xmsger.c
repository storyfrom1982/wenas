#include "xmsger.h"


enum {
    XMSG_PACK_BYE = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_ACK = 0x02,
    XMSG_PACK_PING = 0x04,
    XMSG_PACK_PONG = 0x08,
    XMSG_PACK_FINAL = 0x10
};

enum {
    XMSG_DGRAM = 0,
    XMSG_STREAM
};

#define PACK_HEAD_SIZE              16
// #define PACK_BODY_SIZE              1280 // 1024 + 256
#define PACK_BODY_SIZE              64
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_WINDOW_RANGE           16

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_RESEND_LIMIT       2

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


#define XMSG_CMD_SIZE       64

typedef struct xmsg {
    uint32_t type;
    uint32_t sid;
    xchannel_ptr channel;
    size_t wpos, rpos, len, range;
    struct xmsg *next;
    void *data;
    uint8_t cmd[XMSG_CMD_SIZE];
}*xmsg_ptr;


typedef struct xmsgbuf {
    // struct xmsg msg;
    // uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xmsgpack *buf[1];
}*xmsgbuf_ptr;


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
    // xpipe_ptr msgqueue;
    // xmsg_ptr msg;
    xmsg_ptr send_ptr, recycle_ptr;
    xmsg_ptr *msglist_pptr, *streamlist_pptr;
    struct xmsg rmessage, rstream;

    // TODO 用2个单向链表管理 message 和 stream
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head, end;
}*xchannellist_ptr;

struct xmsger {
    int sock;
    __atom_size pos, len;
    // 每次创建新的channel时加一
    uint32_t cid;
    xtree peers;
    struct __xipaddr addr;
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
    struct xchannellist send_queue, recv_queue;
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

#define __sizeof_ptr    sizeof(void*)

static inline xmsgpack_ptr malloc_pack(xchannel_ptr channel, uint8_t type)
{
    xmsgpack_ptr pack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
    // 设置包类型
    pack->head.type = type;
    // 默认值为 0，当消息类型为 XMSG_PACK_MSG 时，会设置 pack_range >= 1
    pack->head.pack_range = 0;
    // 设置对方 cid
    pack->head.cid = channel->peer_cid;
    // 设置校验码
    pack->head.x = XMSG_VAL ^ channel->peer_key;
    // 设置是否附带 ACK 标志
    pack->head.y = 0;
    return pack;
}

// TODO 需要换成 宏 来实现
static inline void xchannel_enqueue(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    queue->len ++;
}

static inline void xchannel_dequeue(xchannellist_ptr queue, xchannel_ptr channel)
{
    channel->prev->next = channel->next;
    channel->next->prev = channel->prev;
    queue->len --;
}

// 每次接收到一个完整的消息，都要更新保活
static inline void xchannel_update(xchannel_ptr channel)
{
    if (channel->next != &channel->msger->recv_queue.end){
        channel->update = __xapi->clock();
        xchannel_dequeue(&channel->msger->recv_queue, channel);
        xchannel_enqueue(&channel->msger->recv_queue, channel);
    }
}

// 每一次不连续的发送，都会进行两次次队列切换，保活到发送，发送到保活
static inline void xchannel_keep_alive(xchannel_ptr channel)
{
    if (channel->sending){
        channel->sending = false;
        xchannel_dequeue(&channel->msger->send_queue, channel);
        xchannel_enqueue(&channel->msger->recv_queue, channel);
    }
}

static inline void xchannel_keep_sending(xchannel_ptr channel)
{
    if (!channel->sending){
        channel->sending = true;
        xchannel_dequeue(&channel->msger->recv_queue, channel);
        xchannel_enqueue(&channel->msger->send_queue, channel);
    }
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    channel->send_ptr = NULL;
    channel->connected = false;
    channel->sending = false;
    channel->breaker = false;
    channel->update = __xapi->clock();
    channel->msger = msger;
    channel->addr = *addr;
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
        if (++msger->cid == 0){
            msger->cid = 1;
        }
    }
    channel->cid = msger->cid;
    channel->key = msger->cid % 255;
    // 未建立连接前，使用默认 cid 和默认 key
    channel->peer_cid = 0;
    channel->peer_key = XMSG_KEY;

    // 所有新创建的连接，都先进入接收队列，同时出入保活状态，检测到超时就会被释放
    xchannel_enqueue(&msger->recv_queue, channel);
    return channel;
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    __atom_sub(channel->msger->len, channel->len - channel->pos);
    if (channel->sending){
        xchannel_dequeue(&channel->msger->send_queue, channel);
    }else {
        xchannel_dequeue(&channel->msger->recv_queue, channel);
    }
    // TODO 释放 msg queue 中的资源
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_free exit\n");
}

static inline void xchannel_push(xchannel_ptr channel, xmsgpack_ptr pack)
{
    pack->channel = channel;
    pack->comfirmed = false;
    pack->flushing = false;
    // 再将 unit 放入缓冲区 
    pack->head.sn = channel->sendbuf->wpos;
    // 设置校验码
    pack->head.x = XMSG_VAL ^ channel->peer_key;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = pack;
    __atom_add(channel->sendbuf->wpos, 1);
}

static inline bool xchannel_pull(xchannel_ptr channel, xmsgpack_ptr ack)
{
    __xlogd("xchannel_pull >>>>------------> range: %u sn: %u rpos: %u upos: %u wpos %u\n",
           ack->head.acks, ack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 xmsgpack
    if (__transbuf_inuse(channel->sendbuf) > 0 && ((uint8_t)(ack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        __xlogd("xchannel_pull >>>>------------> in range\n");

        uint8_t index;
        xmsgpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        if (ack->head.ack == ack->head.acks){
            __xlogd("xchannel_pull >>>>------------> in serial\n");
            do {
                //TODO 计算往返延时
                //TODO 统计丢包率
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                // 检测 rpos 越界，调试用
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                pack = channel->sendbuf->buf[index];

                // 数据已送达，从待发送数据中减掉这部分长度
                __atom_add(channel->pos, pack->head.pack_size);
                __atom_add(channel->msger->pos, pack->head.pack_size);
                if (channel->recycle_ptr != NULL){
                    __xlogd("xchannel_pull >>>>------------> msg.rpos: %lu msg.len: %lu pack size: %lu\n", channel->recycle_ptr->rpos, channel->recycle_ptr->len, pack->head.pack_size);
                    // 更新已经到达对端的数据计数
                    channel->recycle_ptr->rpos += pack->head.pack_size;
                    if (channel->recycle_ptr->rpos == channel->recycle_ptr->len){
                        // 把已经传送到对端的 msg 交给发送线程处理
                        __xbreak(xpipe_write(channel->msger->spipe, &channel->recycle_ptr, __sizeof_ptr) != __sizeof_ptr);
                        channel->recycle_ptr = channel->recycle_ptr->next;
                        if (channel->recycle_ptr == NULL){
                            xchannel_keep_alive(channel);
                        }
                    }
                }
                __xlogd("xchannel_pull >>>>------------------------------------> channel len: %lu msger len %lu\n", channel->len - 0, channel->msger->len - 0);

                if (!pack->comfirmed){
                    pack->comfirmed = true;
                }else {
                    // 这里可以统计收到重复 ACK 的次数
                }

                // 从定时队列中移除
                if (pack->flushing){
                    pack->next->prev = pack->prev;
                    pack->prev->next = pack->next;
                }

                // 释放内存
                free(pack);

                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                __atom_add(channel->sendbuf->rpos, 1);

                // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
            } while (channel->sendbuf->rpos != ack->head.ack);

            // 判断是否有消息要发送
            if(channel->send_ptr != NULL 
                // 消息可以一次性发送完成
                && (channel->send_ptr->range <= __transbuf_writable(channel->sendbuf) 
                // 发送缓冲区不少于一半可写空间
                || __transbuf_writable(channel->sendbuf) >= (channel->sendbuf->range >> 1))){
                // 通知发送线程进行分片
                __xbreak(xpipe_write(channel->msger->spipe, &channel->send_ptr, __sizeof_ptr) != __sizeof_ptr);
            }

        } else {

            // __logi("xchannel_pull recv interval ack: %u acks: %u", ack->head.ack, ack->head.acks);
            index = ack->head.ack & (PACK_WINDOW_RANGE - 1);
            pack = channel->sendbuf->buf[index];
            pack->comfirmed = true;

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            index = channel->sendbuf->rpos;
            while (index != ack->head.sn) {
                pack = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                if (!pack->comfirmed){
                    // TODO 计算已经发送但还没收到ACK的pack的个数
                    // TODO 这里需要处理超时断开连接的逻辑
                    __xlogd("xchannel_pull >>>>------------------------------------> resend pack: %u\n", pack->head.sn);
                    int result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
                    if (result != PACK_HEAD_SIZE + pack->head.pack_size){
                        __xlogd("xchannel_pull >>>>------------------------> send failed\n");
                        break;
                    }
                }
                index++;
            }
        }

    }else {

        __xlogd("xchannel_pull >>>>------------------------------------> out of range\n");

    }

    channel->update = __xapi->clock();

    return true;

    Clean:

    return false;
}

static inline void xchannel_send(xchannel_ptr channel, xmsgpack_ptr pack)
{
    __xlogd("xchannel_send >>>>------------------------> enter\n");

    int result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);

    // 判断发送是否成功
    if (result == PACK_HEAD_SIZE + pack->head.pack_size){
        // 缓冲区下标指向下一个待发送 pack
        channel->sendbuf->upos++;

        __xlogd("xchannel_send >>>>------------------------> send msg range %lu\n", channel->send_ptr->range);
        // 判断是否为一个 msg 的最后一个 pack
        if (channel->send_ptr->range == 0){
            // TODO 判断 stream 队列是否为空
            // 指向下一个 msg
            channel->send_ptr = channel->send_ptr->next;
        }

        __xlogd("xchannel_send >>>>------------------------> send_ptr %p\n", channel->send_ptr);

        // 判断当前 msg 是否为当前连接的消息队列中的最后一个消息
        if (channel->send_ptr == NULL){
            __xlogd("xchannel_send >>>>------------------------> enqueue flushing\n");
            // 冲洗一次
            pack->flushing = true;
            // __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
            // 记录当前时间
            pack->timestamp = __xapi->clock();
            // 加入冲洗队列
            pack->next = &channel->msger->flushlist.end;
            pack->prev = channel->msger->flushlist.end.prev;
            pack->next->prev = pack;
            pack->prev->next = pack;
            channel->msger->flushlist.len ++;
        }

    }else {
        __xlogd("xchannel_send >>>>------------------------> send failed\n");
    }

    __xlogd("xchannel_send >>>>------------------------> exit\n");
}

static inline bool xchannel_recv(xchannel_ptr channel, xmsgpack_ptr unit)
{
    __xlogd("xchannel_recv >>>>------------> enter\n");
    __xlogd("xchannel_recv >>>>------------> SN: %u rpos: %u wpos: %u\n",
           unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);

    if (unit->head.y == 1){
        __xlogd("xchannel_recv >>>>------------> MSG and ACK\n");
        // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
        xchannel_pull(channel, unit);
    }else {
        // 只有在接收到对端的消息时，才更新超时时间戳
        channel->update = __xapi->clock();
    }

    channel->ack = unit->head;
    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.pack_size = 0;
    uint16_t index = unit->head.sn & (PACK_WINDOW_RANGE - 1);

    // 如果收到连续的 PACK
    if (unit->head.sn == channel->msgbuf->wpos){

        __xlogd("xchannel_recv >>>>------------> serial\n");

        // 保存 PACK
        channel->msgbuf->buf[index] = unit;
        // 更新最大连续 SN
        channel->msgbuf->wpos++;

        // 收到连续的 ACK 就不会回复的单个 ACK 确认了        
        channel->ack.acks = channel->msgbuf->wpos;
        // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
        channel->ack.ack = channel->ack.acks;

        // 如果之前有为按顺序到达的 PACK 需要更新
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
            //TODO
            assert(__transbuf_writable(channel->msgbuf) > 0);
            channel->msgbuf->wpos++;
            // 这里需要更新将要回复的最大连续 ACK
            channel->ack.acks = channel->msgbuf->wpos;
            // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
            channel->ack.ack = channel->ack.acks;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->msgbuf->wpos - unit->head.sn) > (uint8_t)(unit->head.sn - channel->msgbuf->wpos)){

            __xlogd("xchannel_recv >>>>------------------------> early\n");

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack = unit->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.acks = channel->msgbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->msgbuf->wpos - 1;
            
            if (channel->msgbuf->buf[index] == NULL){
                // 这个 PACK 首次到达，保存 PACK
                channel->msgbuf->buf[index] = unit;
            }else {
                // 这个 PACK 重复到达，释放 PACK
                free(unit);
            }
            
        }else {

            __xlogd("xchannel_recv >>>>------------------------> again\n");
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            channel->ack.ack = channel->msgbuf->wpos - 1;
            channel->ack.acks = channel->ack.ack;
            // 释放 PACK
            free(unit);
        }
    }


    if (__transbuf_usable(channel->sendbuf) > 0){

        // 取出当前要发送的 pack
        xmsgpack_ptr pack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
        __xlogd("xchannel_recv >>>>------------> ACK and MSG\n");
        pack->head.y = 1;
        pack->head.ack = channel->ack.ack;
        pack->head.acks = channel->ack.acks;
        xchannel_send(channel, pack);

    }else {

        __xlogd("xchannel_send >>>>------------> ACK\n");
        if ((__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&channel->ack, PACK_HEAD_SIZE)) != PACK_HEAD_SIZE){
            __xlogd("xchannel_send >>>>------------------------> failed\n");
        }
    }


    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        // 交给接收线程来处理 pack
        __xbreak(xpipe_write(channel->msger->rpipe, channel->msgbuf->buf[index], __sizeof_ptr) != __sizeof_ptr);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }

    __xlogd("xchannel_recv >>>>------------> exit\n");

    return true;

    Clean:

    return false;
}


static void* send_loop(void *ptr)
{
    xmsg_ptr msg;
    xmsgpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        __xbreak(xpipe_read(msger->spipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        __xlogd("send_loop >>>>-------------> spipe writable: %lu\n", xpipe_readable(msger->rpipe));

        __xlogd("send_loop >>>>-------------> send_ptr->next: %p\n", msg->channel->send_ptr->next);
        
        // 判断消息是否全部送达
        if (msg->rpos == msg->len){
            // 通知用户，消息已经送达
            msg->channel->msger->listener->onMessageToPeer(msg->channel->msger->listener, msg->channel, msg->data);
            free(msg);
            continue;
        }
        
        while (msg->wpos < msg->len)
        {
            __xlogd("send_loop >>>>-------------> wpos: %lu len: %lu\n", msg->wpos, msg->len);
            if (__transbuf_writable(msg->channel->sendbuf) == 0){
                break;
            }
            pack = malloc_pack(msg->channel, msg->type);
            if (msg->len - msg->wpos < PACK_BODY_SIZE){
                pack->head.pack_size = msg->len - msg->wpos;
            }else{
                pack->head.pack_size = PACK_BODY_SIZE;
            }
            pack->head.pack_range = msg->range;
            mcopy(pack->body, msg->data + msg->wpos, pack->head.pack_size);
            msg->wpos += pack->head.pack_size;
            __xlogd("send_loop ############################# send range %lu\n", msg->range);
            msg->range --;
            __xlogd("send_loop ############################# -- send range %lu\n", msg->range);
            xchannel_push(msg->channel, pack);
        }
    }

Clean:    

    return NULL;
}


static void* main_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    int result;
    xmsg_ptr msg;
    int64_t countdown;
    uint64_t duration = UINT32_MAX;
    xmsger_ptr msger = (xmsger_ptr)ptr;
    
    xmsgpack_ptr rpack = NULL;
    xmsgpack_ptr sendpack = NULL;
    xmsgpack_ptr sendunit = NULL;
    xchannel_ptr channel = NULL;
    xchannel_ptr next = NULL;
    void *readable = NULL;

    struct __xipaddr addr;
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 0, &addr));

    while (__is_true(msger->running))
    {
        // readable 是 true 的时候，接收线程一定会阻塞到接收管道上
        // readable 是 false 的时候，接收线程可能在监听 socket，或者正在给 readable 赋值为 true，所以要用原子变量
        if (__is_true(msger->readable)){

            if (rpack == NULL){
                rpack = (xmsgpack_ptr)malloc(sizeof(struct xmsgpack));
                __xbreak(rpack == NULL);
            }

            rpack->head.type = 0;
            rpack->head.pack_size = 0;
            result = __xapi->udp_recvfrom(msger->sock, &addr, &rpack->head, PACK_ONLINE_SIZE);
            __xlogd("xmsger_loop udp_recvfrom %ld\n", result);
            if (result == (rpack->head.pack_size + PACK_HEAD_SIZE)){

                __xlogd("xmsger_loop recv ip: %u port: %u cid: %u msg: %d\n", addr.ip, addr.port, rpack->head.cid, rpack->head.type);

                channel = (xchannel_ptr)xtree_find(msger->peers, &rpack->head.cid, 4);

                if (channel){

                    __xlogd("xmsger_loop fond channel\n");

                    // 协议层验证
                    if (rpack->head.x ^ channel->key == XMSG_VAL){

                        if (rpack->head.type == XMSG_PACK_MSG) {
                            __xlogd("xmsger_loop receive MSG\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.x = XMSG_VAL ^ channel->peer_key;
                            xchannel_recv(channel, rpack);

                        }else if (rpack->head.type == XMSG_PACK_ACK) {
                            __xlogd("xmsger_loop receive ACK\n");
                            xchannel_pull(channel, rpack);

                        }else if (rpack->head.type == XMSG_PACK_BYE){
                            __xlogd("xmsger_loop receive BYE\n");
                            // 判断是否为主动断开的一方
                            if (__is_true(channel->breaker)){
                                __xlogd("xmsger_loop is breaker\n");
                                // 主动方，回复 FINAL 释放连接
                                xmsgpack_ptr spack = malloc_pack(channel, XMSG_PACK_FINAL);
                                xchannel_push(channel, spack);
                                xchannel_recv(channel, rpack);
                                // 释放连接，结束超时重传
                                msger->listener->onDisconnection(msger->listener, channel);

                            }else {
                                __xlogd("xmsger_loop not breaker\n");
                                // 被动方，收到 BEY，需要回一个 BEY，并且启动超时重传
                                xmsgpack_ptr spack = malloc_pack(channel, XMSG_PACK_BYE);
                                // 启动超时重传
                                xchannel_push(channel, spack);
                                xchannel_recv(channel, rpack);

                            }

                        }else if (rpack->head.type == XMSG_PACK_FINAL){
                            __xlogd("xmsger_loop receive FINAL\n");
                            //被动方收到 FINAL，释放连接，结束超时重传。
                            msger->listener->onDisconnection(msger->listener, channel);

                        }else if (rpack->head.type == XMSG_PACK_PING){
                            // TODO 这里不需要回复 PONG，只需要回复 ACK，主动创建连接方发 PING，被动方回复 ACK
                            __xlogd("xmsger_loop receive PING\n");
                            // 回复 PONG
                            xmsgpack_ptr spack = malloc_pack(channel, XMSG_PACK_PONG);
                            xchannel_push(channel, spack);
                            xchannel_recv(channel, rpack);

                        }else if (rpack->head.type == XMSG_PACK_PONG){
                            // 这里要处理收到 PONG 的情况，因为对穿时，需要在这里回复 ACK
                            // 建立连接以后得 PING，不会收到 PONG，因为收到 PING 的一方，直接回复 ACK
                            __xlogd("xmsger_loop receive PONG\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.x = XMSG_VAL ^ channel->peer_key;
                            xchannel_recv(channel, rpack);
                        }

                    }

                } else {

                    __xlogd("xmsger_loop cannot fond channel\n");

                    if (rpack->head.type == XMSG_PACK_PING){
                        // TODO 收到对穿的 PING，比较时间戳，后发送的一方回复 PONG，先发送的一方直接用 cid 做索引

                        __xlogd("xmsger_loop receive PING\n");

                        if (rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){

                            uint32_t peer_cid = *((uint32_t*)(rpack->body));
                            uint64_t timestamp = *((uint64_t*)(rpack->body + 4));

                            channel = (xchannel_ptr)xtree_find(msger->peers, &addr.port, addr.keylen);

                            // 检查是否为建立同一次连接的重复的 PING
                            if (channel != NULL && channel->timestamp != timestamp){
                                __xlogd("xmsger_loop receive PING reconnecting\n");
                                // 不是重复的 PING
                                // 同一个地址，在建立第一次连接的过程中，又发起了第二次连接，所以要释放第一次连接的资源
                                xchannel_free(channel);
                                channel = NULL;
                            }

                            if (channel == NULL){
                                // 创建连接
                                channel = xchannel_create(msger, &addr);
                                channel->peer_cid = peer_cid;
                                channel->peer_key = peer_cid % 255;
                                channel->timestamp = timestamp;
                                __xlogd("xmsger_loop new connections channel: 0x%x ip: %u port: %u cid: %u time: %lu\n", channel, addr.ip, addr.port, peer_cid, timestamp);
                                // 上一次的连接已经被释放，xtree 直接覆盖原来的连接，替换当前的连接
                                xtree_save(msger->peers, &addr.port, addr.keylen, channel);

                                xmsgpack_ptr spack = malloc_pack(channel, XMSG_PACK_PONG);
                                // // 第一次回复 PONG，cid 必须设置为 0
                                spack->head.cid = 0;
                                spack->head.x = XMSG_VAL ^ XMSG_KEY;
                                spack->head.y = 1;
                                *((uint32_t*)(spack->body)) = channel->cid;
                                *((uint64_t*)(spack->body + 4)) = __xapi->clock();
                                spack->head.pack_size = 12;
                                __atom_add(channel->len, spack->head.pack_size);
                                __atom_add(msger->len, spack->head.pack_size);
                                xchannel_push(channel, spack);
                                rpack->head.pack_size = 0;
                                xchannel_recv(channel, rpack);
                            }                        
                        }

                    }else if (rpack->head.type == XMSG_PACK_PONG){
                        __xlogd("xmsger_loop receive PONG\n");
                        // TODO 收到未知 PONG 崩溃
                        channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                        if (channel && rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){
                            xtree_save(msger->peers, &channel->cid, 4, channel);
                            uint32_t cid = *((uint32_t*)(rpack->body));
                            // 设置对端 cid 与 key
                            channel->peer_cid = cid;
                            channel->peer_key = cid % 255;
                            rpack->head.type = XMSG_PACK_ACK;
                            rpack->head.x = XMSG_VAL ^ XMSG_KEY;
                            rpack->head.pack_size = 0;
                            xchannel_recv(channel, rpack);
                            if (__set_true(channel->connected)){
                                //这里是被动建立连接 onConnectionFromPeer
                                channel->msger->listener->onConnectionToPeer(channel->msger->listener, channel);
                            }
                        }else {
                            rpack->head.type = XMSG_PACK_ACK;
                            rpack->head.x = XMSG_VAL ^ XMSG_KEY;
                            __xapi->udp_sendto(msger->sock, &addr, (void*)&(rpack->head), PACK_HEAD_SIZE);
                        }

                    }else if (rpack->head.type == XMSG_PACK_ACK){
                        __xlogd("xmsger_loop receive ACK\n");

                        channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                        if (channel && rpack->head.cid == 0 && rpack->head.x ^ XMSG_KEY == XMSG_VAL){
                            xtree_save(msger->peers, &channel->cid, 4, channel);
                            xchannel_pull(channel, rpack);
                            channel->connected = true;
                            //这里是被动建立连接 onConnectionFromPeer
                            channel->msger->listener->onConnectionFromPeer(channel->msger->listener, channel);
                        }

                    }else if (rpack->head.type == XMSG_PACK_BYE){
                        __xlogd("xmsger_loop receive BYE\n");
                        // 主动方释放连接后，收到了被动方重传的 BEY
                        // 直接 sendto 回复 FINAL
                        rpack->head.type = XMSG_PACK_FINAL;
                        __xapi->udp_sendto(msger->sock, &addr, (void*)&(rpack->head), PACK_HEAD_SIZE);
                    }
                }

                rpack = NULL;

            }else {

                __set_false(msger->readable);
                // 通知接受线程开始监听 socket
                __xbreak(xpipe_write(msger->rpipe, &readable, __sizeof_ptr) != __sizeof_ptr);

            }

        }

        if (xpipe_readable(msger->mpipe) > 0){
            // 连接的发起和开始发送消息，都必须经过这个管道
            __xbreak(xpipe_read(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

            // 判断连接是否存在
            if (msg->channel == NULL){

                // 连接不存在，创建新连接
                __xlogd("xmsger_loop >>>>-------------> create channel to peer\n");

                msg->channel = xchannel_create(msger, (__xipaddr_ptr)msg->data);
                __xbreak(msg->channel == NULL);

                // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                xtree_save(msger->peers, &msg->channel->addr.port, msg->channel->addr.keylen, msg->channel);

                // 所有 pack 统一交给发送线程发出，保持有序，在这里先填充好 PING 的内容
                *((uint32_t*)(msg->data)) = msg->channel->cid;
                // 将此刻时钟做为连接的唯一标识
                *((uint64_t*)(msg->data + 4)) = __xapi->clock();
            }

            // 更新连接的发送计数
            __atom_add(msg->channel->len, msg->len);

            // 重新加入发送队列，并且从待回收队列中移除
            if (!msg->channel->sending){
                xchannel_keep_sending(msg->channel);
            }

            __xlogd("xmsger_loop >>>>-------------> send msg to peer\n");

            // 判断是否有正在发送的消息
            if (msg->channel->send_ptr == NULL){ // 当前没有消息在发送

                // 设置新消息为当前正在发送的消息
                msg->channel->send_ptr = msg;
                // 队列尾部成员的 next 必须是 NULL
                msg->channel->send_ptr->next = NULL;
                // 将待回收指针指向当前消息
                msg->channel->recycle_ptr = msg->channel->send_ptr;

                if (msg->sid == 0){ // 新成员是消息

                    // 加入消息队列
                    msg->channel->msglist_pptr = &(msg->channel->send_ptr);

                }else { // 新成员是流

                    // 加入流队列
                    msg->channel->streamlist_pptr = &(msg->channel->send_ptr);
                }

                // 将消息放入发送管道，交给发送线程进行分片
                __xbreak(xpipe_write(msger->spipe, &msg, __sizeof_ptr) != __sizeof_ptr);
                
            }else { // 当前有消息正在发送

                if (msg->sid == 0){ // 新成员是消息

                    if (msg->channel->send_ptr->sid == 0){ // 当前正在发送的是消息队列

                        // 将新成员的 next 指向消息队列尾部成员的 next
                        msg->next = (*(msg->channel->msglist_pptr))->next;
                        // 将新成员加入到消息队列尾部
                        (*(msg->channel->msglist_pptr))->next = msg;
                        // 将消息队列的尾部指针指向新成员
                        msg->channel->msglist_pptr = &msg;

                    }else { // 当前正在发送的是流队列

                        // 将新消息成员的 next 指向当前发送队列的队首
                        msg->next = msg->channel->send_ptr;
                        // 设置新消息成员为队首
                        msg->channel->send_ptr = msg;
                        // 将消息队列的尾部指针指向新成员
                        msg->channel->msglist_pptr = &msg;
                    }

                }else { // 新成员是流

                    if (msg->channel->send_ptr->sid == 0){ // 如果当前正在发送的是消息队列
                        
                        if ((*(msg->channel->msglist_pptr))->next == NULL){ // 如果消息队列的后面没有连接着流队列

                            // 将新成员的 next 指向发送队列尾部成员的 next
                            msg->next = (*(msg->channel->msglist_pptr))->next;
                            // 将流队列的头部与消息队列的尾部相连接
                            (*(msg->channel->msglist_pptr))->next = msg;
                            // 将流队列的尾部指针指向新成员
                            msg->channel->streamlist_pptr = &msg;

                        }else { // 消息队列已经和流队列相衔接，直接将新成员加入到流队列尾部

                            // 先设置新成员的 next 为 NULL
                            msg->next = (*(msg->channel->streamlist_pptr))->next;
                            // 将新的成员加入到流队列的尾部
                            (*(msg->channel->streamlist_pptr))->next = msg;
                            // 将流队列的尾部指针指向新成员
                            msg->channel->streamlist_pptr = &msg;
                        }
                        
                    }else { // 当前正在发送的是流队列

                        // 直接将新成员加入到流队列尾部
                        // 先设置新成员的 next 为 NULL
                        msg->next = (*(msg->channel->streamlist_pptr))->next;
                        // 将新的成员加入到流队列的尾部
                        (*(msg->channel->streamlist_pptr))->next = msg;
                        // 将流队列的尾部指针指向新成员
                        msg->channel->streamlist_pptr = &msg;
                    }
                }
            }
        }

        // 判断冲洗列表长度
        if (msger->flushlist.len > 0){

            __xlogd("xmsger_loop >>>>-------------> flushing\n");

            // 取出第一个 pack
            sendpack = msger->flushlist.head.next;
            // __xlogd("xmsger_loop >>>>-------------> %X:%X\n", sendpack, &msger->flushlist.end);
            // goto Clean;            

            // 计算是否需要重传
            if ((countdown = 100000000UL - (__xapi->clock() - sendpack->timestamp)) > 0) {
                __xlogd("xmsger_loop >>>>-------------> duration %lu countdown %lu\n", duration, countdown);
                // 未超时
                if (duration > countdown){
                    // 超时时间更近，更新休息时间
                    // TODO 这个休息时长，要减掉从这时到需要休息时期间耗费的时间
                    duration = countdown;
                }

            }else {

                __xlogd("xmsger_loop >>>>-------------> %X:%X\n", sendpack, &msger->flushlist.end);

                // 需要重传
                while (sendpack != &msger->flushlist.end)
                {
                    // 当前 pack 的 next 指针有可能会改变，所以要先记录下来
                    xmsgpack_ptr next = sendpack->next;

                    if ((countdown = 100000000UL - (__xapi->clock() - sendpack->timestamp)) > 0) {
                        // 未超时
                        if (duration > countdown){
                            // 超时时间更近，更新休息时间
                            duration = countdown;
                        }
                        // 最近的重传时间还没有到，后面的 pack 也不需要重传
                        break;
                    }

                    if (sendpack->head.resend > XCHANNEL_RESEND_LIMIT){
                        __xlogd("xmsger_loop >>>>-------------> this channel (%u) has timed out\n", sendpack->channel->cid);
                        sendpack->prev->next = sendpack->next;
                        sendpack->next->prev = sendpack->prev;
                        msger->flushlist.len--;
                        if (channel->peer_cid == 0){
                            xtree_take(msger->peers, &sendpack->channel->addr.port, sendpack->channel->addr.keylen);
                        }else {
                            xtree_take(msger->peers, &sendpack->channel->peer_cid, 4);
                        }
                        xchannel_free(sendpack->channel);
                        msger->listener->onChannelTimeout(msger->listener, sendpack->channel);
                        sendpack = next;
                        continue;
                    }

                    result = __xapi->udp_sendto(sendpack->channel->msger->sock, &sendpack->channel->addr, (void*)&(sendpack->head), PACK_HEAD_SIZE + sendpack->head.pack_size);
                    __xlogd("xmsger_loop >>>>-------------> resend 1\n");

                    // 判断发送是否成功
                    if (result == PACK_HEAD_SIZE + sendpack->head.pack_size){
                        // 记录重传次数
                        sendpack->head.resend++;
                        // 暂时移出冲洗列表
                        sendpack->prev->next = sendpack->next;
                        sendpack->next->prev = sendpack->prev;

                        // 重传之后，将包再次加入到冲洗列表的队尾
                        // 因为，所有正常到达对端的包都会很快被移出冲洗列表
                        // 所以，如果这个包依然未能到达对端，也很快就可以被再次重传
                        // 只是把包放到列表的最后面，不需要增加延迟，如果增加了延迟，就会像 TCP 一样
                        sendpack->next = &msger->flushlist.end;
                        sendpack->prev = msger->flushlist.end.prev;
                        sendpack->next->prev = sendpack;
                        sendpack->prev->next = sendpack;

                    }else {
                        __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                        goto Clean;
                    }

                    sendpack = next;
                }

                // 判断是否冲洗队列中的所有包都超时
                if (sendpack == &msger->flushlist.end){
                    // 延迟一百毫秒后重传
                    duration = 1000 * MICRO_SECONDS;
                }
            }            
        }

        // 判断待发送队列中是否有内容
        if (msger->send_queue.len > 0){

            // __xlogd("xmsger_loop >>>>-------------> send channel enter\n");

            // TODO 如能能更平滑的发送
            // 从头开始，每个连接发送一个 pack
            channel = msger->send_queue.head.next;

            // __xlogd("xmsger_loop >>>>-------------> send channel 1\n");

            while (channel != &msger->send_queue.end)
            {
                // __xlogd("xmsger_loop >>>>-------------> send channel 2\n");
                // TODO 如能能更平滑的发送，这里是否要循环发送，知道清空缓冲区？
                // 判断缓冲区中是否有可发送 pack
                if (__transbuf_usable(channel->sendbuf) > 0){
                    // __xlogd("xmsger_loop >>>>-------------> send channel 3\n");
                    xchannel_send(channel, channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)]);

                }

                //TODO 确认发送过程中，连接不会被移出队列
                channel = channel->next;
            }

            // __xlogd("xmsger_loop >>>>-------------> send channel exit\n");

        }

        // 处理超时
        if (msger->recv_queue.len > 0){
            
            __xlogd("xmsger_loop >>>>-------------> check recv list\n");

            channel = msger->recv_queue.head.next;
            // 10 秒钟超时
            while (channel != &msger->recv_queue.end){
                next = channel->next;

                if (__xapi->clock() - channel->update > NANO_SECONDS * 10){
                    xtree_take(msger->peers, &channel->peer_cid, 4);
                    xchannel_free(channel);
                    msger->listener->onChannelTimeout(msger->listener, channel);
                }else {
                    // 队列的第一个连接没有超时，后面的连接就都没有超时
                    break;
                }

                channel = next;
            }
        }
    
        // 判断是否有待发送数据和待接收数据
        if (__is_false(msger->readable)){
            
            // 判断是否设置了休眠时间，得知是否有定时事件待处理
            if (duration < UINT32_MAX){

                __xapi->mutex_lock(msger->mtx);
                // 主动发送消息时，会通过判断这个值来检测主线程是否在工作
                __set_false(msger->working);
                // 休息一段时间
                __xapi->mutex_timedwait(msger->mtx, duration);
                // 设置工作状态
                __set_true(msger->working);
                // 设置最大睡眠时间，如果有需要定时重传的 pack，这个时间值将会被设置为，最近的重传时间
                duration = UINT32_MAX;
                __xapi->mutex_unlock(msger->mtx);

            }else if (msger->pos == msger->len){
                // 没有数据可收发，可以休眠任意时长
                __xlogd("xmsger_loop >>>>-------------> noting to do\n");

                __xapi->mutex_lock(msger->mtx);
                __set_false(msger->working);
                __xapi->mutex_timedwait(msger->mtx, duration);
                __set_true(msger->working);
                duration = UINT32_MAX;
                __xapi->mutex_unlock(msger->mtx);

                __xlogd("xmsger_loop >>>>-------------> start working\n");
            }

        }    
    
    }

Clean:    


    if (rpack != NULL){
        free(rpack);
    }

    __xlogd("xmsger_loop exit\n");
    return NULL;
}


static void* recv_loop(void *ptr)
{
    xmsg_ptr msg;
    xmsgpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        __xapi->udp_listen(msger->sock);
        __set_true(msger->readable);
        __xapi->mutex_notify(msger->mtx);
        
        while(xpipe_read(msger->rpipe, &pack, __sizeof_ptr) == __sizeof_ptr)
        {
            if (pack == NULL){
                break;
            }

            msg = &pack->channel->rmessage;
            // PING 和 PONG BYE FINAL 的 pack_range 都设置为 0
            if (pack->head.type == XMSG_PACK_MSG && pack->head.pack_range > 0){
                if (msg->data == NULL){
                    // 收到消息的第一个包，创建 msg，记录范围
                    msg->range = pack->head.pack_range;
                    assert(msg->range != 0 && msg->range <= XMSG_PACK_RANGE);
                    msg->data = (uint8_t*)malloc(msg->range * PACK_BODY_SIZE);
                    msg->wpos = 0;
                }
                __xlogd("recv_loop >>>>--------------------------------------------------------------------> pos %u range %u\n", msg->wpos, pack->head.pack_range);
                mcopy(msg->data + msg->wpos, pack->body, pack->head.pack_size);
                msg->wpos += pack->head.pack_size;
                msg->range--;
                if (msg->range == 0){
                    pack->channel->msger->listener->onMessageFromPeer
                        (pack->channel->msger->listener, 
                        pack->channel, msg->data, msg->wpos);
                    free(msg->data);
                    msg->data = NULL;
                    if (!pack->channel->sending){
                        // 不在发送队列才需要更新保活
                        xchannel_update(pack->channel);
                    }
                }
            }

            free(pack);
        }
    }

Clean:

    return NULL;
}


bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size)
{
    __xlogd("xmsger_send enter\n");

    __xbreak(channel == NULL);

    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_MSG;
    msg->channel = channel;
    msg->rpos = 0;
    msg->wpos = 0;
    msg->len = size;
    msg->data = data;

    msg->range = (msg->len / PACK_BODY_SIZE);
    if (msg->range * PACK_BODY_SIZE < msg->len){
        // 有余数，增加一个包
        msg->range ++;
    }

    __atom_add(msger->len, size);
    __atom_add(channel->len, size);

    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    if (__is_false(msger->working)){
        __xlogd("xmsger_send notify\n");
        __xapi->mutex_notify(msger->mtx);
    }

    __xlogd("xmsger_send exit\n");

    return true;

Clean:

    if (msg){
        free(msg);
    }
    __xlogd("xmsger_send failed\n");
    return false;
}

bool xmsger_ping(xmsger_ptr msger, xchannel_ptr channel)
{
    __xlogd("xmsger_ping channel 0x%X enter", channel);

    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_PING;
    msg->channel = channel;
    msg->rpos = 0;
    msg->wpos = 0;
    msg->range = 1;
    msg->len = XMSG_CMD_SIZE;
    msg->data = msg->cmd;
    __xbreak(msg->data == NULL);

    *(uint64_t*)msg->data = __xapi->clock();

    __atom_add(msger->len, msg->len);
    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    if (__is_false(msger->working)){
        __xlogd("xmsger_ping channel 0x%X notify", channel);
        __xapi->mutex_notify(msger->mtx);
    }    

    __xlogd("xmsger_ping channel 0x%X exit", channel);

    return true;

Clean:

    if (msg){
        if (msg->data){
            free(msg->data);
        }
        free(msg);
    }
    __xlogd("xmsger_ping channel 0x%X failed", channel);

    return false;

    __xlogd("xmsger_ping exit");
}

bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel)
{
    __xlogd("xmsger_disconnect channel 0x%X enter", channel);

    // 避免重复调用
    if (!__set_true(channel->breaker)){
        __xlogd("xmsger_disconnect channel 0x%X repeated calls", channel);
        return true;
    }

    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_BYE;
    msg->channel = channel;
    msg->rpos = 0;
    msg->wpos = 0;
    msg->range = 1;
    msg->len = XMSG_CMD_SIZE;
    msg->data = msg->cmd;

    *(uint64_t*)msg->data = __xapi->clock();

    __atom_add(msger->len, msg->len);
    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    if (__is_false(msger->working)){
        __xlogd("xmsger_disconnect channel 0x%X notify", channel);
        __xapi->mutex_notify(msger->mtx);
    }    

    __xlogd("xmsger_disconnect channel 0x%X exit", channel);

    return true;

Clean:

    if (msg){
        if (msg->data){
            free(msg->data);
        }
        free(msg);
    }
    __xlogd("xmsger_disconnect channel 0x%X failed", channel);

    return false;

}

bool xmsger_connect(xmsger_ptr msger, const char *host, uint16_t port)
{
    __xlogd("xmsger_connect enter\n");

    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_PING;
    msg->channel = NULL;
    msg->rpos = 0;
    msg->wpos = 0;    
    msg->range = 1;
    msg->len = XMSG_CMD_SIZE;
    msg->data = msg->cmd;

    __xbreak(!__xapi->udp_make_ipaddr(host, port, (__xipaddr_ptr)msg->data));
    
    // 更新 msger 的发送计数
    __atom_add(msger->len, msg->len);
    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    if (__is_false(msger->working)){
        __xlogd("xmsger_connect notify\n");
        __xapi->mutex_notify(msger->mtx);
    }

    __xlogd("xmsger_connect exit\n");

    return true;

Clean:

    if (msg){
        if (msg->data){
            free(msg->data);
        }
        free(msg);
    }
    __xlogd("xmsger_connect failed\n");
    return false;
}

xmsger_ptr xmsger_create(xmsglistener_ptr listener)
{
    __xlogd("xmsger_create enter\n");

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));
    
    msger->running = true;
    msger->listener = listener;

    msger->sock = __xapi->udp_open();
    __xbreak(msger->sock < 0);
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 9256, &msger->addr));
    __xbreak(__xapi->udp_bind(msger->sock, &msger->addr) == -1);
    __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", 9256, &msger->addr));

    msger->send_queue.len = 0;
    msger->send_queue.head.prev = NULL;
    msger->send_queue.end.next = NULL;
    msger->send_queue.head.next = &msger->send_queue.end;
    msger->send_queue.end.prev = &msger->send_queue.head;

    msger->recv_queue.len = 0;
    msger->recv_queue.head.prev = NULL;
    msger->recv_queue.end.next = NULL;
    msger->recv_queue.head.next = &msger->recv_queue.end;
    msger->recv_queue.end.prev = &msger->recv_queue.head;
    // TODO 这些链表里的数据都要释放
    msger->flushlist.len = 0;
    msger->flushlist.head.prev = NULL;
    msger->flushlist.end.next = NULL;
    msger->flushlist.head.next = &msger->flushlist.end;
    msger->flushlist.end.prev = &msger->flushlist.head;
    msger->flushlist.end.timestamp = UINT64_MAX;

    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->peers = xtree_create();
    __xbreak(msger->peers == NULL);

    msger->mtx = __xapi->mutex_create();
    __xbreak(msger->mtx == NULL);

    msger->mpipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->mpipe == NULL);

    msger->spipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->spipe == NULL);

    msger->rpipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->rpipe == NULL);    

    msger->spid = __xapi->process_create(send_loop, msger);
    __xbreak(msger->spid == NULL);

    msger->rpid = __xapi->process_create(recv_loop, msger);
    __xbreak(msger->rpid == NULL);

    msger->mpid = __xapi->process_create(main_loop, msger);
    __xbreak(msger->mpid == NULL);

    __xlogd("xmsger_create exit\n");

    return msger;

Clean:

    xmsger_free(&msger);
    __xlogd("xmsger_create failed\n");
    return NULL;
}

static void free_channel(void *val)
{
    __xlogd("free_channel >>>>------------> 0x%x\n", val);
    xchannel_free((xchannel_ptr)val);
}

void xmsger_free(xmsger_ptr *pptr)
{
    __xlogd("xmsger_free enter\n");

    if (pptr && *pptr){

        xmsger_ptr msger = *pptr;
        *pptr = NULL;

        __set_false(msger->running);

        if (msger->mtx){
            __xapi->mutex_broadcast(msger->mtx);
        }

        if (msger->mpipe){
            __xlogd("xmsger_free break mpipe\n");
            xpipe_break(msger->mpipe);
        }

        if (msger->spipe){
            __xlogd("xmsger_free break spipe\n");
            xpipe_break(msger->spipe);
        }

        if (msger->rpipe){
            __xlogd("xmsger_free break rpipe\n");
            xpipe_break(msger->rpipe);
        }

        if (msger->sock > 0){
            int sock = __xapi->udp_open();
            __xapi->udp_sendto(sock, &msger->addr, &sock, sizeof(int));
            __xapi->udp_close(sock);
        }        

        if (msger->spid){
            __xlogd("xmsger_free send process\n");
            __xapi->process_free(msger->spid);
        }

        if (msger->rpid){
            __xlogd("xmsger_free recv process\n");
            __xapi->process_free(msger->rpid);
        }

        if (msger->mpid){
            __xlogd("xmsger_free main process\n");
            __xapi->process_free(msger->mpid);
        }        

        if (msger->mpipe){
            __xlogd("xmsger_free send pipe\n");
            xpipe_free(&msger->mpipe);
        }

        if (msger->spipe){
            __xlogd("xmsger_free recv pipe\n");
            xpipe_free(&msger->spipe);
        }        

        if (msger->peers){
            __xlogd("xmsger_free clear peers\n");
            xtree_clear(msger->peers, free_channel);
            __xlogd("xmsger_free peers\n");
            xtree_free(&msger->peers);
        }

        if (msger->mtx){
            __xlogd("xmsger_free mutex\n");
            __xapi->mutex_free(msger->mtx);
        }

        if (msger->sock > 0){
            __xapi->udp_close(msger->sock);
        }

        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}