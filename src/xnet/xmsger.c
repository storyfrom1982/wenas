#include "xmsger.h"

#include "xtree.h"
#include "xbuf.h"


enum {
    XMSG_PACK_ACK = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_PING = 0x02,
    XMSG_PACK_PONG = 0x04,
    XMSG_PACK_ONL = 0x08,
    XMSG_PACK_BYE = 0x10,
};


#define PACK_HEAD_SIZE              16
// #define PACK_BODY_SIZE              1280 // 1024 + 256
#define PACK_BODY_SIZE              64
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_SERIAL_RANGE           64

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_RESEND_LIMIT       1
#define XCHANNEL_FEEDBACK_TIMES     1000

typedef struct xhead {
    uint8_t type;
    uint8_t sn; // serial number 包序列号
    uint8_t ack; // 确认收到了序列号为当前 ACK 的包
    uint8_t acks; // 确认收到了这个 ACK 序列号之前的所有包
    uint8_t key; // 校验码
    uint8_t flag; // 扩展标志，是否附带了 ACK
    uint8_t resend; // 这个包的重传计数
    uint8_t sid; // 多路复用的流 ID
    uint16_t range; // 一个消息的分包数量，从 1 到 range
    uint16_t len; // 当前分包装载的数据长度
    uint32_t cid; // 连接通道 ID
}*xhead_ptr;

typedef struct xpack {
    bool is_flushing;
    uint64_t timer;
    uint64_t timestamp; //计算往返耗时
    xmessage_ptr msg;
    xchannel_ptr channel;
    struct xpack *prev, *next;
    struct xhead head;
    uint8_t body[PACK_BODY_SIZE];
}*xpack_ptr;

struct xpacklist {
    uint8_t len;
    struct xpack head;
};

typedef struct serialbuf {
    uint8_t range, spos, rpos, wpos;
    struct xpack *buf[1];
}*serialbuf_ptr;

typedef struct sserialbuf {
    uint8_t range, spos, rpos, wpos;
    struct xpack buf[1];
}*sserialbuf_ptr;

struct xchannel {

    uint8_t local_key;
    uint8_t remote_key;

    uint8_t serial_range;
    uint8_t serial_number;
    uint64_t timestamp;

    uint16_t back_times;
    uint64_t back_delay;
    uint64_t back_range;

    bool keepalive;
    bool breaker;
    bool connected; // 用peer_cid来标志是否连接
    __atom_size pos, len;
    __atom_size pack_in_pipe;
    struct __xipaddr addr;
    void *userctx;
    xmsger_ptr msger;
    struct xhead ack;
    serialbuf_ptr recvbuf;
    sserialbuf_ptr sendbuf;
    struct xpacklist flushinglist;
    struct xchannellist *worklist;

    xmessage_ptr send_ptr;
    xmessage_ptr *msglist_tail, *streamlist_tail;
    struct xmessage streams[3];
    xchannel_ptr prev, next;
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head;
}*xchannellist_ptr;

struct xmsger {
    int sock;
    xtree peers;
    __atom_size pos, len;
    __atom_size msglistlen;
    __atom_size sendable;
    __xmutex_ptr mtx;
    __atom_bool running;
    //socket可读或者有数据待发送时为true
    __atom_bool working;
    __atom_bool readable;
    __atom_bool listening;
    xmsgercb_ptr callback;
    xpipe_ptr mpipe;
    __xprocess_ptr mpid;
    struct xchannellist send_list, recv_list;
};

#define __serialbuf_wpos(b)         ((b)->wpos & ((b)->range - 1))
#define __serialbuf_rpos(b)         ((b)->rpos & ((b)->range - 1))
#define __serialbuf_spos(b)         ((b)->spos & ((b)->range - 1))

#define __serialbuf_recvable(b)     ((uint8_t)((b)->spos - (b)->rpos))
#define __serialbuf_sendable(b)     ((uint8_t)((b)->wpos - (b)->spos))

#define __serialbuf_readable(b)     ((uint8_t)((b)->wpos - (b)->rpos))
#define __serialbuf_writable(b)     ((uint8_t)((b)->range - (b)->wpos + (b)->rpos))

#define XMSG_KEY    'x'
#define XMSG_VAL    'X'

#define __ring_list_take_out(rlist, rnode) \
    (rnode)->prev->next = (rnode)->next; \
    (rnode)->next->prev = (rnode)->prev; \
    (rlist)->len--

#define __ring_list_put_into_end(rlist, rnode) \
    (rnode)->next = &(rlist)->head; \
    (rnode)->prev = (rlist)->head.prev; \
    (rnode)->prev->next = (rnode); \
    (rnode)->next->prev = (rnode); \
    (rlist)->len++

#define __ring_list_put_into_head(rlist, rnode) \
    (rnode)->next = (rlist)->head.next; \
    (rnode)->prev = &(rlist)->head; \
    (rnode)->prev->next = (rnode); \
    (rnode)->next->prev = (rnode); \
    (rlist)->len++

#define __ring_list_move_to_end(rlist, rnode) \
    (rnode)->prev->next = (rnode)->next; \
    (rnode)->next->prev = (rnode)->prev; \
    (rnode)->next = &(rlist)->head; \
    (rnode)->prev = (rlist)->head.prev; \
    (rnode)->prev->next = (rnode); \
    (rnode)->next->prev = (rnode)

#define __ring_list_move_to_head(rlist, rnode) \
    (rnode)->prev->next = (rnode)->next; \
    (rnode)->next->prev = (rnode)->prev; \
    (rnode)->next = (rlist)->head.next; \
    (rnode)->prev = &(rlist)->head; \
    (rnode)->prev->next = (rnode); \
    (rnode)->next->prev = (rnode)

#define __xchannel_put_into_list(rlist, rnode) \
    __ring_list_put_into_end((rlist), (rnode)); \
    (rnode)->worklist = (rlist)

#define __xchannel_take_out_list(rnode) \
    __ring_list_take_out((rnode)->worklist, (rnode)); \
    (rnode)->worklist = NULL

#define __xchannel_move_to_end(rnode) \
    __ring_list_move_to_end((rnode)->worklist, (rnode))



static inline xmessage_ptr new_message(xchannel_ptr channel, uint8_t type, uint8_t sid, void *data, uint64_t len)
{
    xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
    if (msg == NULL){
        return NULL;
    }

    msg->rpos = 0;
    msg->wpos = 0;
    msg->type = type;
    msg->sid = sid;
    msg->channel = channel;
    msg->next = NULL;

    if (data == NULL){
        msg->len = XMSG_CMD_SIZE;
        msg->data = msg->cmd;
    }else {
        msg->len = len;
        msg->data = data;
    }

    msg->range = (msg->len / PACK_BODY_SIZE);
    if (msg->range * PACK_BODY_SIZE < msg->len){
        // 有余数，增加一个包
        msg->range ++;
    }

    __xlogd("new_message >>>>-------------------> range: %u\n", msg->range);

    return msg;
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr, uint8_t serial_range)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xbreak(channel == NULL);

    channel->msger = msger;
    channel->addr = *addr;
    channel->serial_range = serial_range;
    channel->timestamp = __xapi->clock();

    channel->local_key = channel->timestamp % XMSG_KEY;
    channel->serial_number = channel->timestamp % channel->serial_range;
    
    channel->send_ptr = NULL;
    channel->connected = false;
    channel->breaker = false;

    channel->remote_key = XMSG_KEY;
    channel->back_delay = 200000000UL;
    channel->ack.key = (XMSG_VAL ^ XMSG_KEY);

    channel->recvbuf = (serialbuf_ptr) calloc(1, sizeof(struct serialbuf) + sizeof(xpack_ptr) * channel->serial_range);
    __xbreak(channel->recvbuf == NULL);
    channel->recvbuf->range = channel->serial_range;

    channel->sendbuf = (sserialbuf_ptr) calloc(1, sizeof(struct sserialbuf) + sizeof(struct xpack) * channel->serial_range);
    __xbreak(channel->sendbuf == NULL);
    channel->sendbuf->range = channel->serial_range;
    channel->sendbuf->rpos = channel->sendbuf->spos = channel->sendbuf->wpos = channel->serial_number;

    channel->flushinglist.len = 0;
    channel->flushinglist.head.prev = &channel->flushinglist.head;
    channel->flushinglist.head.next = &channel->flushinglist.head;

    // 所有新创建的连接，都先进入接收队列，同时出入保活状态，检测到超时就会被释放
    __xchannel_put_into_list(&msger->recv_list, channel);

    return channel;

    Clean:

    if (channel){
        if (channel->recvbuf){
            free(channel->recvbuf);
        }        
        if (channel->sendbuf){
            free(channel->sendbuf);
        }
        free(channel);
    }

    return NULL;
}

static inline void xchannel_clear(xchannel_ptr channel)
{
    __xlogd("xchannel_clear enter\n");
    // 先释放待确认的消息，避免发了一半的消息，要是先被释放，更新 rpos 就会导致崩溃的 BUG
    while(__serialbuf_recvable(channel->sendbuf) > 0)
    {
        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_rpos(channel->sendbuf)];
        __xlogd("xchannel_clear pack type %u\n", pack->head.type);
        if (pack->msg != NULL){
            pack->msg->rpos += pack->head.len;
            if (pack->msg->rpos == pack->msg->len){
                if (pack->msg->type == XMSG_PACK_MSG){
                    free(pack->msg->data);
                }
                free(pack->msg);
            }
        }
        // 更新读下标，否则会不可写入
        channel->sendbuf->rpos++;
    }
    
    // 后释放 send ptr，以免更新 rpos 时崩溃
    while (channel->send_ptr != NULL)
    {
        xmessage_ptr next = channel->send_ptr->next;
        // 减掉不能发送的数据长度，有的 msg 可能已经发送了一半，所以要减掉 wpos
        channel->len -= channel->send_ptr->len - channel->send_ptr->wpos;
        channel->msger->len -= channel->send_ptr->len - channel->send_ptr->wpos;
        if (channel->send_ptr->type == XMSG_PACK_MSG){
            free(channel->send_ptr->data);
        }
        free(channel->send_ptr);
        channel->send_ptr = next;
    }
    // 清空冲洗队列
    channel->flushinglist.len = 0;
    channel->flushinglist.head.prev = &channel->flushinglist.head;
    channel->flushinglist.head.next = &channel->flushinglist.head;
    // 刷新发送队列
    while(__serialbuf_sendable(channel->sendbuf) > 0)
    {
        // 减掉发送缓冲区的数据
        channel->len -=  channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->msger->len -= channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->sendbuf->spos++;
    }

    __xlogd("xchannel_clear chnnel len: %lu pos: %lu\n", channel->len, channel->pos);
    __xlogd("xchannel_clear exit\n");
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    xchannel_clear(channel);
    __xchannel_take_out_list(channel);
    for (int i = 0; i < 3; ++i){
        if (channel->streams[i].data != NULL){
            // TODO 会导致 on_msg_from_peer 中释放内存崩溃
            free(channel->streams[i].data);
        }
    }
    // 缓冲区里的包有可能不是连续的，所以不能顺序的清理，如果使用 rpos 就会出现内存泄漏的 BUG
    for (int i = 0; i < channel->recvbuf->range; ++i){
        if (channel->recvbuf->buf[i] != NULL){
            free(channel->recvbuf->buf[i]);
            // 这里要置空，否则重复调用这个函数，会导致崩溃
            channel->recvbuf->buf[i] = NULL;
        }
    }
    free(channel->recvbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_free exit\n");
}

static inline void xchannel_serial_cmd(xchannel_ptr channel, uint8_t type)
{
    xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
    *(uint8_t*)(pack->body) = channel->local_key;
    *(uint8_t*)(pack->body + 1) = channel->serial_range;
    *(uint8_t*)(pack->body + 2) = channel->serial_number;
    pack->head.len = 3;
    pack->msg = NULL;
    pack->head.type = type;
    pack->head.sid = 0;
    pack->head.range = 1;
    pack->channel = channel;
    pack->is_flushing = false;
    pack->head.resend = 0;
    pack->head.flag = 0;
    pack->head.key = (XMSG_VAL ^ channel->remote_key);
    pack->head.sn = channel->sendbuf->wpos;
    __atom_add(channel->sendbuf->wpos, 1);
    channel->len += pack->head.len;
    channel->msger->len += pack->head.len;
    channel->msger->sendable++;
    // 加入发送队列，并且从待回收队列中移除
    if(channel->worklist != &channel->msger->send_list) {
        __xchannel_take_out_list(channel);
        __xchannel_put_into_list(&channel->msger->send_list, channel);
    }
}

static inline void xchannel_serial_msg(xchannel_ptr channel)
{
    xmessage_ptr msg = channel->send_ptr;

    // 每次只缓冲一个包，尽量使发送速度均匀
    if (msg != NULL && __serialbuf_writable(channel->sendbuf) > 0){

        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
        pack->msg = msg;
        if (msg->len - msg->wpos < PACK_BODY_SIZE){
            pack->head.len = msg->len - msg->wpos;
        }else{
            pack->head.len = PACK_BODY_SIZE;
        }

        pack->head.type = msg->type;
        pack->head.sid = msg->sid;
        pack->head.range = msg->range;
        mcopy(pack->body, msg->data + msg->wpos, pack->head.len);
        msg->wpos += pack->head.len;
        msg->range --;

        pack->channel = channel;
        pack->is_flushing = false;
        pack->head.resend = 0;
        pack->head.flag = 0;
        pack->head.key = (XMSG_VAL ^ channel->remote_key);
        pack->head.sn = channel->sendbuf->wpos;
        __atom_add(channel->sendbuf->wpos, 1);
        channel->msger->sendable++;

        // 判断消息是否全部写入缓冲区
        if (msg->wpos == msg->len){
            // 更新当前消息
            channel->send_ptr = channel->send_ptr->next;
            msg = channel->send_ptr;
            channel->msger->msglistlen--;
        }
    }
}

static inline void xchannel_input_message(xchannel_ptr channel, xmessage_ptr msg)
{    
    // 更新待发送计数
    __atom_add(msg->channel->len, msg->len);
    __atom_add(msg->channel->msger->len, msg->len);
    channel->msger->msglistlen++;

    // 判断是否有正在发送的消息
    if (channel->send_ptr == NULL){ // 当前没有消息在发送
        // 第一个消息的 next 将成为队尾标志，所以 next 必须设置成 NULL
        msg->next = NULL;
        // 设置新消息为当前正在发送的消息
        channel->send_ptr = msg;

        if (msg->sid == 0){ // 新成员是消息
            // 加入消息队列
            channel->msglist_tail = &(msg->next);
        }else { // 新成员是流
            // 加入流队列
            channel->streamlist_tail = &(msg->next);
        }
        
    }else { // 当前有消息正在发送

        if (msg->sid == 0){ // 新成员是消息

            if (channel->send_ptr->sid == 0){ // 当前正在发送的是消息队列
                // 将新成员的 next 指向消息队列尾部
                msg->next = (*(channel->msglist_tail));
                // 将新成员加入到消息队列尾部
                *(channel->msglist_tail) = msg;
                // 将消息队列的尾部指针指向新成员
                channel->msglist_tail = &(msg->next);

            }else { // 当前正在发送的是流队列
                // 将新消息成员的 next 指向当前发送队列的队首
                msg->next = channel->send_ptr;
                // 设置新消息成员为队首
                channel->send_ptr = msg;
                // 将消息队列的尾部指针指向新成员
                channel->msglist_tail = &(msg->next);
            }

        }else { // 新成员是流

            if (channel->send_ptr->sid == 0){ // 如果当前正在发送的是消息队列
                
                if (*(channel->msglist_tail) == NULL){ // 如果消息队列的后面没有连接着流队列

                    // 将新成员的 next 设置为 NULL
                    msg->next = NULL;
                    // 将流队列连接到消息队列的尾部
                    *(channel->msglist_tail) = msg;
                    // 将流队列的尾部指针指向新成员
                    channel->streamlist_tail = &(msg->next);

                }else { // 消息队列已经和流队列相衔接，直接将新成员加入到流队列尾部

                    // 先设置新成员的 next 为 NULL
                    msg->next = NULL;
                    // 将新的成员加入到流队列的尾部
                    *(channel->streamlist_tail) = msg;
                    // 将流队列的尾部指针指向新成员
                    channel->streamlist_tail = &(msg->next);
                }
                
            }else { // 当前正在发送的是流队列

                // 直接将新成员加入到流队列尾部
                // 先设置新成员的 next 为 NULL
                msg->next = NULL;
                // 将新的成员加入到流队列的尾部
                *(channel->streamlist_tail) = msg;
                // 将流队列的尾部指针指向新成员
                channel->streamlist_tail = &(msg->next);
            }
        }
    }

    // 加入发送队列，并且从待回收队列中移除
    if(channel->worklist != &channel->msger->send_list) {
        __xchannel_take_out_list(channel);
        __xchannel_put_into_list(&channel->msger->send_list, channel);
    }

    if (__serialbuf_sendable(channel->sendbuf) == 0){
        // 确保将待发送数据写入了缓冲区
        xchannel_serial_msg(channel);
    }
}

static inline void xchannel_output_message(xchannel_ptr channel)
{
    if (__serialbuf_readable(channel->recvbuf) > 0){

        xmessage_ptr msg;
        // 索引已接收的包
        xpack_ptr pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        do {

            // 索引对应的消息
            msg = &(channel->streams[pack->head.sid]);
            
            if (pack->head.type == XMSG_PACK_MSG){
                // 如果当前消息的数据为空，证明这个包是新消息的第一个包
                if (msg->data == NULL){
                    // 收到消息的第一个包，为当前消息分配资源，记录消息的分包数
                    msg->range = pack->head.range;
                    // 分配内存
                    msg->data = malloc(msg->range * PACK_BODY_SIZE);
                    // 写索引置零
                    msg->wpos = 0;
                }
                mcopy(msg->data + msg->wpos, pack->body, pack->head.len);
                msg->wpos += pack->head.len;
                msg->range--;
                if (msg->range == 0){
                    // 通知用户已收到一个完整的消息
                    channel->msger->callback->on_msg_from_peer(channel->msger->callback, channel, msg->data, msg->wpos);
                    msg->data = NULL;
                }
            }

            // 收到一个完整的消息，需要判断是否需要更新保活
            if (pack->head.range == 1 && channel->worklist == &channel->msger->recv_list){
                // 更新时间戳
                channel->timestamp = __xapi->clock();
                // 判断队列是否有多个成员
                if (channel->worklist->len > 1){
                    // 将更新后的成员移动到队尾
                    __xchannel_move_to_end(channel);
                }
            }
            // 处理玩的缓冲区置空
            channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)] = NULL;
            // 更新读索引
            channel->recvbuf->rpos++;
            // 释放资源
            free(pack);
            // 索引下一个缓冲区
            pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        }while (pack != NULL); // 所用已接收包被处理完之后，接收缓冲区为空
    }
}

static inline void xchannel_send_pack(xchannel_ptr channel)
{
    if (__serialbuf_sendable(channel->sendbuf) > 0){

        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)];
        if (channel->ack.flag != 0){
            // __xlogd("xchannel_send_pack >>>>-----------------------------------------------------------> SEND PACK && ACK\n");
            // 携带 ACK
            pack->head.flag = channel->ack.flag;
            pack->head.ack = channel->ack.ack;
            pack->head.acks = channel->ack.acks;

        }else {
            // __xlogd("xchannel_send_pack >>>>-----------------------------------------------------------> SEND PACK\n");
        }

        // 判断发送是否成功
        if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){

            // 发送成功才能清空标志
            channel->ack.flag = 0;

            // 数据已发送，从待发送数据中减掉这部分长度
            __atom_add(channel->pos, pack->head.len);
            __atom_add(channel->msger->pos, pack->head.len);
            channel->msger->sendable--;

            // 缓冲区下标指向下一个待发送 pack
            __atom_add(channel->sendbuf->spos, 1);

            // 记录当前时间
            pack->timestamp = __xapi->clock();
            channel->timestamp = pack->timestamp;
            pack->timer = pack->timestamp + channel->back_delay * 1.5;
            __ring_list_put_into_end(&channel->flushinglist, pack);

            // 如果有待发送数据，确保 sendable 会大于 0
            xchannel_serial_msg(channel);

        }else {

            __xlogd("xchannel_send_pack >>>>------------------------> send failed\n");
        }
    }
}

static inline void xchannel_send_ack(xchannel_ptr channel)
{
    // __xlogd("xchannel_send_pack >>>>---------------------------------> SEND ACK\n");
    if ((__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&channel->ack, PACK_HEAD_SIZE)) == PACK_HEAD_SIZE){
        channel->ack.flag = 0;
    }else {
        __xlogd("xchannel_serial_pack >>>>------------------------> failed\n");
    }
}

static inline void xchannel_send_final(xmsger_ptr msger, __xipaddr_ptr addr, xpack_ptr rpack)
{
    __xlogd("xchannel_send_pack >>>>-----------------------------------------------------------> SEND FINAL ACK\n");
    rpack->head.type = XMSG_PACK_ACK;
    rpack->head.flag = XMSG_PACK_BYE;
    // 设置 acks，通知发送端已经接收了所有包
    rpack->head.acks = rpack->head.sn;
    rpack->head.ack = rpack->head.acks;
    // 使用默认的校验码
    rpack->head.key = (XMSG_VAL ^ XMSG_KEY);
    // 要设置包长度，对方要校验长度
    rpack->head.len = 0;
    rpack->head.range = 1;
    if (__xapi->udp_sendto(msger->sock, addr, (void*)&rpack->head, PACK_HEAD_SIZE) != PACK_HEAD_SIZE){
        __xlogd("xchannel_send_ack >>>>------------------------> failed\n");
    }
}

static inline void xchannel_serial_ack(xchannel_ptr channel, xpack_ptr rpack)
{
    // 只处理 sn 在 rpos 与 spos 之间的 xpack
    if (__serialbuf_recvable(channel->sendbuf) > 0 && ((uint8_t)(rpack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos))){

        xpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        // 如果没有丢包，第一个顺序到达的 acks 一定等于 ack，但是如果这个包丢了，acks 就会不等于 ack
        // 所以每次都要检测 rpos 是否等于 asks

        uint8_t index = __serialbuf_rpos(channel->sendbuf);

        // 这里曾经使用 do while 方式，造成了收到重复的 ACK，导致 rpos 越界的 BUG
        // 连续的 acks 必须至少比 rpos 大 1
        while (channel->sendbuf->rpos != rpack->head.acks) {

            pack = &channel->sendbuf->buf[index];

            if (pack->timestamp != 0){
                // 累计新的一次往返时长
                channel->back_range += (__xapi->clock() - pack->timestamp);
                pack->timestamp = 0;

                if (channel->back_times < XCHANNEL_FEEDBACK_TIMES){
                    // 更新累计次数
                    channel->back_times++;
                }else {
                    // 已经到达累计次数，需要减掉一次平均时长
                    channel->back_range -= channel->back_delay;
                }
                // 重新计算平均时长
                channel->back_delay = channel->back_range / channel->back_times;

                __ring_list_take_out(&channel->flushinglist, pack);

                // 判断是否有未发送的消息
                if (channel->send_ptr == NULL){
                    // 判断是否有待重传的包，再判断是否需要保活
                    if (channel->flushinglist.len == 0 && !channel->keepalive){
                        // 不需要保活，加入等待超时队列
                        __xchannel_take_out_list(channel);
                        __xchannel_put_into_list(&channel->msger->recv_list, channel);
                    }
                }
            }

            if (pack->head.type == XMSG_PACK_MSG){
                // 更新已经到达对端的数据计数
                pack->msg->rpos += pack->head.len;
                if (pack->msg->rpos == pack->msg->len){
                    // 把已经传送到对端的 msg 交给发送线程处理
                    channel->msger->callback->on_msg_to_peer(channel->msger->callback, channel, pack->msg->data, pack->msg->len);
                    free(pack->msg);
                }
            }else if (rpack->head.flag == XMSG_PACK_PONG){
                // 有能会重复收到这个 ACK
                if (!channel->connected){
                    channel->connected = true;
                    channel->msger->callback->on_channel_from_peer(channel->msger->callback, channel);
                }
            }

            __atom_add(channel->sendbuf->rpos, 1);

            // 更新索引
            index = __serialbuf_rpos(channel->sendbuf);

            if (__serialbuf_sendable(channel->sendbuf) > 0 && __serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 2)){
                xchannel_send_pack(channel);
            }

            // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
        }

        if (rpack->head.ack != rpack->head.acks){

            // __xlogd("xchannel_serial_ack >>>>-----------> (%u) OUT OF OEDER ACK: %u\n", channel->peer_cid, rpack->head.ack);

            pack = &channel->sendbuf->buf[rpack->head.ack & (channel->sendbuf->range - 1)];

            if (pack->timestamp != 0){
                // 累计新的一次往返时长
                channel->back_range += (__xapi->clock() - pack->timestamp);
                pack->timestamp = 0;
                if (channel->back_times < XCHANNEL_FEEDBACK_TIMES){
                    // 更新累计次数
                    channel->back_times++;
                }else {
                    // 已经到达累计次数，需要减掉一次平均时长
                    channel->back_range -= channel->back_delay;
                }
                // 重新计算平均时长
                channel->back_delay = channel->back_range / channel->back_times;
                // 从定时队列中移除
                __ring_list_take_out(&channel->flushinglist, pack);
            }

            // 使用临时变量
            uint8_t index = channel->sendbuf->rpos;
            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            while (index != rpack->head.ack) {
                // 取出落后的包
                pack = &channel->sendbuf->buf[(index & (channel->sendbuf->range - 1))];
                // 判断这个包是否已经接收过
                if (pack->timestamp != 0){
                    // 判断是否进行了重传
                    if (pack->head.resend == 0){
                        pack->head.resend = 1;
                        // 判断重传的包是否带有 ACK
                        if (pack->head.flag != 0){
                            // 更新 ACKS
                            // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                            pack->head.acks = channel->recvbuf->wpos;
                        }
                        __xlogd("xchannel_serial_ack >>>>>>>>>>-----------------------------------------> RESEND PACK: %u\n", pack->head.sn);
                        if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){
                            pack->timer = __xapi->clock() + channel->back_delay * 1.5;
                        }else {
                            __xlogd("xchannel_serial_ack >>>>------------------------> send failed\n");
                        }
                    }
                }

                index++;
            }
        }

    }else {

        // __xlogd("xchannel_serial_ack >>>>-----------> (%u) OUT OF RANGE: %u\n", channel->peer_cid, rpack->head.sn);

    }
}

static inline void xchannel_serial_pack(xchannel_ptr channel, xpack_ptr *rpack)
{
    xpack_ptr pack = *rpack;

    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.flag = pack->head.type;

    uint16_t index = pack->head.sn & (channel->recvbuf->range - 1);

    // 如果收到连续的 PACK
    if (pack->head.sn == channel->recvbuf->wpos){        

        pack->channel = channel;
        // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
        if (pack->head.flag != 0){
            // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
            xchannel_serial_ack(channel, pack);
        }
        // 保存 PACK
        channel->recvbuf->buf[index] = pack;
        *rpack = NULL;
        // 更新最大连续 SN
        channel->recvbuf->wpos++;

        // 收到连续的 ACK 就不会回复的单个 ACK 确认了
        channel->ack.acks = channel->recvbuf->wpos;
        // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
        channel->ack.ack = channel->ack.acks;

        // 如果提前到达的 PACK 需要更新
        while (channel->recvbuf->buf[__serialbuf_wpos(channel->recvbuf)] != NULL 
                // 判断缓冲区是否正好填满，避免首位相连造成死循环的 BUG
                && __serialbuf_writable(channel->recvbuf) > 0)
        {
            channel->recvbuf->wpos++;
            // 这里需要更新将要回复的最大连续 ACK
            channel->ack.acks = channel->recvbuf->wpos;
            // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
            channel->ack.ack = channel->ack.acks;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->recvbuf->wpos - pack->head.sn) > (uint8_t)(pack->head.sn - channel->recvbuf->wpos)){

            // __xlogd("xchannel_serial_pack >>>>-----------> (%u) EARLY: %u\n", channel->peer_cid, pack->head.sn);

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack = pack->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.acks = channel->recvbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->recvbuf->wpos - 1;
            
            if (channel->recvbuf->buf[index] == NULL){
                pack->channel = channel;
                // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
                if (pack->head.flag != 0){
                    // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
                    xchannel_serial_ack(channel, pack);
                }
                // 这个 PACK 首次到达，保存 PACK
                channel->recvbuf->buf[index] = pack;
                *rpack = NULL;
            }else {
                // 重复到达的 PACK
            }
            
        }else {

            // __xlogd("xchannel_serial_pack >>>>-----------> (%u) AGAIN: %u\n", channel->peer_cid, pack->head.sn);
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.acks = channel->recvbuf->wpos;
            channel->ack.ack = channel->ack.acks;
            // 重复到达的 PACK
        }
    }

    xchannel_output_message(channel);
}

xpack_ptr first_pack()
{
    return (xpack_ptr)calloc(1, sizeof(struct xpack));;
}

xpack_ptr next_pack()
{
    return (xpack_ptr)calloc(1, sizeof(struct xpack));;
}

static void* main_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    // int result;
    int64_t delay;
    uint64_t timer = UINT32_MAX;
    xmsger_ptr msger = (xmsger_ptr)ptr;
    
    void *readable = NULL;
    xchannel_ptr channel = NULL, next_channel;

    struct __xipaddr addr;
    __xbreak(!__xapi->udp_host_to_ipaddr(NULL, 0, &addr));

    xmessage_ptr msg;
    xpack_ptr spack = NULL;
    xpack_ptr rpack = first_pack();
    // xpack_ptr rpack = (xpack_ptr)malloc(sizeof(struct xpack));
    __xbreak(rpack == NULL);
    rpack->head.len = 0;

    while (__is_true(msger->running))
    {
        // __xlogd("main_loop >>>>-----> recvfrom\n");
        // readable 是 true 的时候，接收线程一定会阻塞到接收管道上
        // readable 是 false 的时候，接收线程可能在监听 socket，或者正在给 readable 赋值为 true，所以要用原子变量
        while (__xapi->udp_recvfrom(msger->sock, &addr, &rpack->head, PACK_ONLINE_SIZE) == (rpack->head.len + PACK_HEAD_SIZE)){

            // __xlogd("main_loop (IP:%X) >>>>>---------------------------------> RECV: TYPE(%u) FLAG[%u:%u:%u] SN: %u\n", 
            //         *(uint64_t*)(&addr.port), rpack->head.type, rpack->head.flag, rpack->head.ack, rpack->head.acks, rpack->head.sn);

            channel = (xchannel_ptr)xtree_find(msger->peers, &addr.port, 6);

            if (channel){

                // 协议层验证
                if ((rpack->head.key ^ channel->local_key) == XMSG_VAL){

                    if (rpack->head.type == XMSG_PACK_MSG) {
                        xchannel_serial_pack(channel, &rpack);
                        if (channel->ack.flag != 0){
                            if (__serialbuf_sendable(channel->sendbuf) > 0 && __serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 1)){
                                xchannel_send_pack(channel);
                            }else {
                                xchannel_send_ack(channel);
                            }
                        }

                    }else if (rpack->head.type == XMSG_PACK_ACK){
                        xchannel_serial_ack(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_ONL){
                        xchannel_serial_pack(channel, &rpack);
                        if (channel->ack.flag != 0){
                            xchannel_send_ack(channel);
                        }

                    }else if (rpack->head.type == XMSG_PACK_PONG){
                        // 主动端收到了 PONG
                        if (!channel->connected){
                            // 更新连接状态
                            channel->connected = true;
                            // 取出同步参数
                            uint8_t remote_key = *((uint8_t*)(rpack->body));
                            uint8_t serial_range = *((uint8_t*)(rpack->body + 1));
                            uint8_t serial_number = *((uint8_t*)(rpack->body + 2));
                            // 同步序列号
                            channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                            // 设置校验码
                            channel->remote_key = remote_key;
                            // __xlogd("xmsger_loop >>>>-------------> RECV PONG: SN(%u) KEY(%u) ts(%lu)\n", serial_number, channel->remote_key, remote_key);
                            // 生成 ack 的校验码
                            channel->ack.key = (XMSG_VAL ^ channel->remote_key);
                            // 通知用户建立连接
                            msger->callback->on_channel_to_peer(msger->callback, channel);
                        }
                        // 更新接收缓冲区和 ACK
                        xchannel_serial_pack(channel, &rpack);
                        // 回复 ACK
                        if (channel->ack.flag != 0){
                            xchannel_send_ack(channel);
                        }

                    }else if (rpack->head.type == XMSG_PACK_BYE){
                        __xlogd("on_channel_break %p\n", channel);
                        msger->callback->on_channel_break(msger->callback, channel);
                        // 被动端收到 BYE，删除索引
                        xtree_take(msger->peers, &addr.port, 6);
                        xchannel_free(channel);
                        // 回复最后的 ACK
                        xchannel_send_final(msger, &addr, rpack);

                    }else if (rpack->head.type == XMSG_PACK_PING){

                        // 取出同步参数
                        uint8_t remote_key = *((uint8_t*)(rpack->body));
                        uint8_t serial_range = *((uint8_t*)(rpack->body + 1));
                        uint8_t serial_number = *((uint8_t*)(rpack->body + 2));

                        // 对端重连
                        if (channel->connected){
                            void *userctx = channel->userctx;
                            __xlogd("reconnnected >>>>---------------------> channel %p\n", channel);
                            // msger->callback->on_channel_reconnected(msger->callback, channel);
                            // 创建连接
                            xchannel_ptr new_channel = xchannel_create(msger, &addr, serial_range);
                            __xbreak(new_channel == NULL);
                            new_channel->userctx = userctx;
                            new_channel->send_ptr = channel->send_ptr;
                            new_channel->msglist_tail = channel->msglist_tail;
                            new_channel->streamlist_tail = channel->streamlist_tail;
                            // // 移除超时的连接
                            xtree_take(msger->peers, &channel->addr.port, 6);
                            xchannel_free(channel);
                            channel = new_channel;
                            xtree_save(msger->peers, &channel->addr.port, 6, channel);
                        }

                        channel->connected = true;
                        // 同步序列号
                        channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                        // 设置校验码
                        channel->remote_key = remote_key;
                        // __xlogd("xmsger_loop >>>>-------------> RECV PING: SN(%u) REMOTE KEY(%u) LOCAL KEY(%u) ts(%lu)\n", serial_number, channel->remote_key, channel->local_key, channel->timestamp);
                        // 生成 ack 的校验码
                        channel->ack.key = (XMSG_VAL ^ channel->remote_key);
                        // 建立索引
                        xtree_save(msger->peers, &addr.port, 6, channel);
                        // 更新接收缓冲区和 ACK
                        xchannel_serial_pack(channel, &rpack);
                        // 生成 PONG
                        xchannel_serial_cmd(channel, XMSG_PACK_PONG);
                        // 发送 PONG 和更新的 ACK
                        xchannel_send_pack(channel);
                    }

                }else if ((rpack->head.key ^ XMSG_KEY) == XMSG_VAL){

                    if (rpack->head.type == XMSG_PACK_ACK && rpack->head.flag == XMSG_PACK_BYE){
                        msger->callback->on_channel_break(msger->callback, channel);
                        // 主动端收到 BYE 的 ACK，移除索引
                        xtree_take(msger->peers, &addr.port, 6);
                        xchannel_free(channel);

                    }else {}
                }

            } else {

                if (rpack->head.type == XMSG_PACK_PING){
                    // 收到对方发起的 PING
                    if ((rpack->head.key ^ XMSG_KEY) == XMSG_VAL){
                        // 取出同步参数
                        uint8_t remote_key = *((uint8_t*)(rpack->body));
                        uint8_t serial_range = *((uint8_t*)(rpack->body + 1));
                        uint8_t serial_number = *((uint8_t*)(rpack->body + 2));
                        // 创建连接
                        channel = xchannel_create(msger, &addr, serial_range);
                        __xbreak(channel == NULL);
                        // 同步序列号
                        channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                        // 设置校验码
                        channel->remote_key = remote_key;
                        // __xlogd("xmsger_loop >>>>-------------> RECV PING: SN(%u) REMOTE KEY(%u) LOCAL KEY(%u) ts(%lu)\n", serial_number, channel->remote_key, channel->local_key, channel->timestamp);
                        // 生成 ack 的校验码
                        channel->ack.key = (XMSG_VAL ^ channel->remote_key);
                        // 建立索引
                        xtree_save(msger->peers, &addr.port, 6, channel);
                        // 更新接收缓冲区和 ACK
                        xchannel_serial_pack(channel, &rpack);
                        // 生成 PONG
                        xchannel_serial_cmd(channel, XMSG_PACK_PONG);
                        // 发送 PONG 和更新的 ACK
                        xchannel_send_pack(channel);

                    }else {}

                }else if (rpack->head.type == XMSG_PACK_BYE){

                    // 无法校验，回复最后的 ACK
                    xchannel_send_final(msger, &addr, rpack);

                }else {}
            }

            if (rpack == NULL){
                // rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                rpack = next_pack();
                __xbreak(rpack == NULL);
            }

            rpack->head.len = 0;
        }

        if (xpipe_readable(msger->mpipe) > 0){
            // 连接的发起和开始发送消息，都必须经过这个管道
            __xbreak(xpipe_read(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

            // 判断连接是否存在
            if (msg->type == XMSG_PACK_MSG){

                xchannel_input_message(msg->channel, msg);

            }else {

                if (msg->type == XMSG_PACK_PING){

                    xlinekv_t params = xl_parser(msg->data);
                    xline_ptr addr = xl_find(&params, "addr");
                    void *ctx = xl_find_ptr(&params, "ctx");
                    xmessage_ptr resendmsg = xl_find_ptr(&params, "resendmsg");
                    free(params.body);

                    channel = xchannel_create(msger, (__xipaddr_ptr)(__xl_b2o(addr)), PACK_SERIAL_RANGE);
                    __xbreak(channel == NULL);
                    channel->keepalive = true;
                    __xlogd("xmsger_loop >>>>-------------> create channel to peer SN(%u) KEY(%u)\n", channel->serial_number, channel->local_key);
                    channel->userctx = ctx;
                    while (resendmsg != NULL){
                        xchannel_input_message(msg->channel, resendmsg);
                        resendmsg = resendmsg->next;
                    }
                    // 建立连接时，先用对方的 IP&PORT&local_cid 三元组作为本地索引，在收到回复的 HELLO 时，换成 local_cid 做为索引
                    xtree_save(msger->peers, &channel->addr.port, 6, channel);
                    // 先用本端的 cid 作为对端 channel 的索引，重传这个 HELLO 就不会多次创建连接
                    xchannel_serial_cmd(channel, XMSG_PACK_PING);

                }else {

                    if (msg->channel != NULL){
                        channel = msg->channel;
                        // __xlogd("xmsger_loop (%u) >>>>-------------> break channel(%u)\n", channel->cid, channel->peer_cid);
                        // 换成对端 cid 作为当前的索引，对端不需要再持有本端的 cid，收到 BYE 时直接回复 ACK 即可
                        xchannel_clear(channel);
                        xchannel_serial_cmd(channel, XMSG_PACK_BYE);
                    }
                }

                free(msg);
            }
        }

        // 判断待发送队列中是否有内容
        if (msger->send_list.len > 0){

            // TODO 如能能更平滑的发送
            // 从头开始，每个连接发送一个 pack
            channel = msger->send_list.head.next;

            while (channel != &msger->send_list.head)
            {
                next_channel = channel->next;

                // 留一半缓冲区，给回复 ACK 时候，如果有数据待发送，可以与 ACK 一起发送
                if (__serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 3)){
                    xchannel_send_pack(channel);
                }

                if (channel->flushinglist.len > 0){

                    spack = channel->flushinglist.head.next;

                    while (spack != &channel->flushinglist.head)
                    {
                        xpack_ptr next_pack = spack->next;

                        if ((delay = (spack->timer - __xapi->clock())) > 0) {
                            // 未超时
                            if (timer > delay){
                                // 超时时间更近，更新休息时间
                                timer = delay;
                            }

                        }else {

                            if (spack->head.resend > XCHANNEL_RESEND_LIMIT){
                                __xlogd("on_channel_break %p\n", channel);
                                if (channel->send_ptr){
                                    channel->send_ptr->wpos = channel->send_ptr->rpos = 0;
                                }
                                msger->callback->on_channel_timeout(msger->callback, channel, channel->send_ptr);
                                // // 移除超时的连接
                                xtree_take(msger->peers, &channel->addr.port, 6);
                                xchannel_free(channel);
                                break;

                            }else {

                                // 判断重传的包是否带有 ACK
                                if (spack->head.flag != 0){
                                    // 更新 ACKS
                                    // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                    spack->head.acks = channel->recvbuf->wpos;
                                }

                                __xlogd("xmsger_loop >>>>>>>>>>-------------------------------------------> RESEND PACK: %u\n", spack->head.sn);

                                // 判断发送是否成功
                                if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(spack->head), PACK_HEAD_SIZE + spack->head.len) == PACK_HEAD_SIZE + spack->head.len){
                                    // 记录重传次数
                                    spack->head.resend++;
                                    spack->timer = __xapi->clock() + channel->back_delay * 1.5;
                                    // 列表中如果只有一个成员，就不能更新包的位置
                                    if (channel->flushinglist.len > 1){
                                        // 重传之后的包放入队尾
                                        __ring_list_move_to_end(&channel->flushinglist, spack);
                                    }
                                }else {
                                    __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                                }

                                timer = channel->back_delay;
                            }
                        }

                        spack = next_pack;
                    }

                    // __xlogd("xmsger_loop >>>>------------------------> flusing exit\n");

                }else if (channel->keepalive && channel->pos == channel->len && __serialbuf_readable(channel->sendbuf) == 0){

                    if ((delay = (NANO_SECONDS * 9) - (__xapi->clock() - channel->timestamp)) > 0) {
                        // 未超时
                        if (timer > delay){
                            // 超时时间更近，更新休息时间
                            timer = delay;
                        }

                    }else {
                        xchannel_serial_cmd(channel, XMSG_PACK_ONL);
                        // 更新时间戳
                        channel->timestamp = __xapi->clock();

                    }
                }

                channel = next_channel;
            }
        }

        // 处理超时
        if (msger->recv_list.len > 0){
            
            channel = msger->recv_list.head.next;

            while (channel != &msger->recv_list.head){
                next_channel = channel->next;
                // 10 秒钟超时
                if (__xapi->clock() - channel->timestamp > NANO_SECONDS * 10){
                    __xlogd("on_channel_break 2\n");
                    msger->callback->on_channel_break(msger->callback, channel);
                    // // 移除超时的连接
                    xtree_take(msger->peers, &channel->addr.port, 6);
                    xchannel_free(channel);
                }else {
                    // 队列的第一个连接没有超时，后面的连接就都没有超时
                    break;
                }

                channel = next_channel;
            }
        }

        // if (__set_false(msger->readable)){
        //     // 通知接受线程开始监听 socket
        //     __xbreak(xpipe_write(msger->rpipe, &readable, __sizeof_ptr) != __sizeof_ptr);
        // }

        // 判断休眠条件
        // 没有待发送的包
        // 没有待发送的消息
        // 网络接口不可读
        // 接收线程已经在监听
        if (msger->sendable == 0 && xpipe_readable(msger->mpipe) == 0){
            // 如果有待重传的包，会设置冲洗定时
            // 如果需要发送 PING，会设置 PING 定时
            if (__xapi->mutex_trylock(msger->mtx)){
                // __xlogd("main_loop >>>>-----> nothig to do sendable: %lu\n", msger->sendable);
                if (msger->sendable == 0 && xpipe_readable(msger->mpipe) == 0){
                    __xapi->mutex_notify(msger->mtx);
                    __xapi->mutex_timedwait(msger->mtx, timer);
                    timer = 10000000000UL; // 10 秒
                }
                // __xlogd("main_loop >>>>-----> start working\n");
                __xapi->mutex_unlock(msger->mtx);
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


static inline bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t len, uint8_t sid)
{
    __xlogd("xmsger_send enter\n");

    __xbreak(channel == NULL);

    xmessage_ptr msg = new_message(channel, XMSG_PACK_MSG, sid, data, len);

    __xbreak(msg == NULL);

    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    __xapi->mutex_notify(msger->mtx);

    __xlogd("xmsger_send exit\n");

    return true;

Clean:

    __xlogd("xmsger_send failed\n");

    if (msg){
        free(msg);
    }
    
    return false;
}

bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size)
{
    return xmsger_send(msger, channel, data, size, 0);
}

bool xmsger_send_stream(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size)
{
    return xmsger_send(msger, channel, data, size, 1);
}

bool xmsger_send_file(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size)
{
    return xmsger_send(msger, channel, data, size, 2);
}

bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel)
{
    __xlogd("xmsger_disconnect channel 0x%X enter\n", channel);

    // 避免重复调用
    if (!__set_false(channel->connected)){
        __xlogd("xmsger_disconnect channel 0x%X repeated calls\n", channel);
        return true;
    }

    xmessage_ptr msg = new_message(channel, XMSG_PACK_BYE, 0, NULL, 0);

    __xcheck(msg == NULL);

    *(uint64_t*)msg->data = __xapi->clock();

    __xcheck(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    // 确保主线程一定会被唤醒
    __xapi->mutex_lock(msger->mtx);
    __set_true(msger->readable);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_unlock(msger->mtx);

    __xlogd("xmsger_disconnect channel 0x%X exit\n", channel);

    return true;

XClean:

    __xlogd("xmsger_disconnect channel 0x%X failed", channel);

    if (msg){
        free(msg);
    }

    return false;

}

bool xmsger_connect(xmsger_ptr msger, __xipaddr_ptr addr, void *peerctx)
{
    __xlogd("xmsger_connect enter\n");

    xlinekv_t params = xl_maker(1024);
    __xcheck(params.body == NULL);

    xl_add_bin(&params, "addr", addr, sizeof(struct __xipaddr));
    xl_add_ptr(&params, "ctx", peerctx);

    xmessage_ptr msg = new_message(NULL, XMSG_PACK_PING, 0, params.body, params.wpos);
    __xcheck(msg == NULL);
    
    __xcheck(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    // 确保主线程一定会被唤醒
    __xapi->mutex_lock(msger->mtx);
    __set_true(msger->readable);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_unlock(msger->mtx);

    __xlogd("xmsger_connect exit\n");

    return true;

XClean:

    __xlogd("xmsger_connect failed\n");

    if (msg){
        free(msg);
    }
    return false;
}

extern bool xmsger_reconnect(xmsger_ptr msger, __xipaddr_ptr addr, void *peerctx, xmessage_ptr resendmsg)
{
    __xlogd("xmsger_connect enter\n");

    xlinekv_t params = xl_maker(1024);
    __xcheck(params.body == NULL);

    xl_add_bin(&params, "addr", addr, sizeof(struct __xipaddr));
    xl_add_ptr(&params, "ctx", peerctx);
    xl_add_ptr(&params, "resendmsg", resendmsg);

    xmessage_ptr msg = new_message(NULL, XMSG_PACK_PING, 0, params.body, params.wpos);
    __xcheck(msg == NULL);
    
    __xcheck(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    // 确保主线程一定会被唤醒
    __xapi->mutex_lock(msger->mtx);
    __set_true(msger->readable);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_unlock(msger->mtx);

    __xlogd("xmsger_connect exit\n");

    return true;

XClean:

    __xlogd("xmsger_connect failed\n");

    if (params.body){
        free(params.body);
    }

    if (msg){
        free(msg);
    }
    return false;    
}

void xmsger_notify(xmsger_ptr msger, uint64_t timing)
{
    __xapi->mutex_lock(msger->mtx);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_timedwait(msger->mtx, timing);
    __xapi->mutex_unlock(msger->mtx);
}

xmsger_ptr xmsger_create(xmsgercb_ptr callback, int sock)
{
    __xlogd("xmsger_create enter\n");

    __xbreak(callback == NULL);

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));

    msger->sock = sock;
    msger->running = true;
    msger->callback = callback;
    msger->sendable = 0;

    msger->send_list.len = 0;
    msger->send_list.head.prev = &msger->send_list.head;
    msger->send_list.head.next = &msger->send_list.head;

    msger->recv_list.len = 0;
    msger->recv_list.head.prev = &msger->recv_list.head;
    msger->recv_list.head.next = &msger->recv_list.head;

    msger->peers = xtree_create();
    __xbreak(msger->peers == NULL);

    msger->mtx = __xapi->mutex_create();
    __xbreak(msger->mtx == NULL);

    msger->mpipe = xpipe_create(sizeof(void*) * 1024, "SEND PIPE");
    __xbreak(msger->mpipe == NULL);

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

        if (msger->mpid){
            __xlogd("xmsger_free main process\n");
            __xapi->process_free(msger->mpid);
        }

        // channel free 会用到 rpipe，所以将 tree clear 提前
        if (msger->peers){
            __xlogd("xmsger_free clear peers\n");
            xtree_clear(msger->peers, free_channel);
            __xlogd("xmsger_free peers\n");
            xtree_free(&msger->peers);
        }

        if (msger->mpipe){
            __xlogd("xmsger_free msg pipe: %lu\n", xpipe_readable(msger->mpipe));
            while (xpipe_readable(msger->mpipe) > 0){
                xmessage_ptr msg;
                xpipe_read(msger->mpipe, &msg, __sizeof_ptr);
                if (msg){
                    if (msg->type == XMSG_PACK_MSG){
                        free(msg->data);
                    }
                    free(msg);
                }
            }
            xpipe_free(&msger->mpipe);
        }

        if (msger->mtx){
            __xlogd("xmsger_free mutex\n");
            __xapi->mutex_free(msger->mtx);
        }

        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}

void* xmsger_get_channel_ctx(xchannel_ptr channel)
{
    return channel->userctx;
}

void xmsger_set_channel_ctx(xchannel_ptr channel, void *ctx)
{
    channel->userctx = ctx;
}

__xipaddr_ptr xmsger_get_channel_ipaddr(xchannel_ptr channel)
{
    return &channel->addr;
}