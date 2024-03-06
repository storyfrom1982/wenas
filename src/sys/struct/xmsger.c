#include "xmsger.h"

#include "xtree.h"
#include "xbuf.h"


enum {
    XMSG_PACK_ACK = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_BYE = 0x02,
    XMSG_PACK_PING = 0x04,
    XMSG_PACK_HELLO = 0x08
};


#define PACK_HEAD_SIZE              16
// #define PACK_BODY_SIZE              1280 // 1024 + 256
#define PACK_BODY_SIZE              64
#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_WINDOW_RANGE           16

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_RESEND_LIMIT       2

#define XCHANNEL_FEEDBACK_TIMES     100

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
    // TODO 多个 peer cid 会发生冲突，使用 本地 cid 与 peer cid 拼接出一个唯一 cid
    // uint32_t rcid;
    // uint32_t lcid;
    uint32_t cid; // 连接通道 ID
}*xhead_ptr;

#define XMSG_CMD_SIZE       64

typedef struct xmessage {
    uint32_t type;
    uint32_t sid;
    size_t wpos, rpos, len, range;
    void *data;
    xchannel_ptr channel;
    struct xmessage *next;
    uint8_t cmd[XMSG_CMD_SIZE];
}*xmessage_ptr;

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
    struct xpack head, end;
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
    uint32_t cid;
    uint8_t key;
    uint32_t peer_cid;
    uint8_t peer_key;
    uint32_t window;
    uint64_t timestamp;

    uint8_t back_times;
    uint64_t back_delay;
    uint64_t back_range;

    bool ping;
    bool breaker;
    bool connected; // 用peer_cid来标志是否连接
    __atom_size pos, len;
    struct __xipaddr addr;
    void *usercontext;
    xmsger_ptr msger;
    struct xhead ack;
    serialbuf_ptr recvbuf;
    sserialbuf_ptr sendbuf;
    struct xpacklist flushinglist;
    struct xchannellist *worklist;
    __atom_size msglist_len;
    xmessage_ptr send_ptr;
    xmessage_ptr *msglist_tail, *streamlist_tail;
    struct xmessage streams[3];
    xchannel_ptr prev, next;
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head, end;
}*xchannellist_ptr;

struct xmsger {
    int sock;
    bool writable;
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
    __atom_bool listening;
    xmsgercb_ptr callback;
    xpipe_ptr mpipe, rpipe;
    __xprocess_ptr mpid, rpid;
    struct xchannellist send_list, recv_list;

    __atom_size rpack_malloc_count;
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

#define __xchannel_enqueue(que, ch) \
    (ch)->next = &((que)->end); \
    (ch)->prev = (que)->end.prev; \
    (ch)->next->prev = (ch); \
    (ch)->prev->next = (ch); \
    (ch)->worklist = (que); \
    (que)->len++

#define __xchannel_dequeue(ch) \
    (ch)->prev->next = (ch)->next; \
    (ch)->next->prev = (ch)->prev; \
    (ch)->worklist->len--

#define __xchannel_keep_alive(ch) \
    (ch)->prev->next = (ch)->next; \
    (ch)->next->prev = (ch)->prev; \
    (ch)->next = &((ch)->worklist->end); \
    (ch)->prev = (ch)->worklist->end.prev; \
    (ch)->next->prev = (ch); \
    (ch)->prev->next = (ch)


void* xchannel_context(xchannel_ptr channel)
{
    if (channel){
        return channel->usercontext;
    }
    return NULL;
}

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

    return msg;
}

static inline void xchannel_send_message(xchannel_ptr channel, xmessage_ptr msg)
{    
    // 更新待发送计数
    __atom_add(msg->channel->len, msg->len);
    __atom_add(msg->channel->msger->len, msg->len);

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
        __xchannel_dequeue(channel);
        __xchannel_enqueue(&channel->msger->send_list, channel);
    }
}

static inline bool xchannel_recv_message(xchannel_ptr channel)
{
    if (__serialbuf_readable(channel->recvbuf) > 0){
        uint8_t index = __serialbuf_rpos(channel->recvbuf);
        while (channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)] != NULL)
        {
            // 交给接收线程来处理 pack
            if (xpipe_write(channel->msger->rpipe, &channel->recvbuf->buf[index], __sizeof_ptr) != __sizeof_ptr) {
                return false;
            }
            // 收到一个完整的消息，需要判断是否需要更新保活
            if (channel->recvbuf->buf[index]->head.range == 1 && channel->worklist == &channel->msger->recv_list){
                // 判断队列是否有多个成员
                if (channel->worklist->len > 1){
                    __xchannel_keep_alive(channel);
                }
                // 更新时间戳
                channel->timestamp = __xapi->clock();
            }
            channel->recvbuf->buf[index] = NULL;
            channel->recvbuf->rpos++;
            index = __serialbuf_rpos(channel->recvbuf);
        }
    }

    return true;
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr, bool ping)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xbreak(channel == NULL);
    channel->send_ptr = NULL;
    channel->connected = false;
    channel->breaker = false;
    channel->ping = ping;
    channel->back_delay = 200000000UL;
    channel->timestamp = __xapi->clock();
    channel->msger = msger;
    channel->addr = *addr;

    channel->recvbuf = (serialbuf_ptr) calloc(1, sizeof(struct serialbuf) + sizeof(xpack_ptr) * PACK_WINDOW_RANGE);
    __xbreak(channel->recvbuf == NULL);
    channel->recvbuf->range = PACK_WINDOW_RANGE;

    channel->sendbuf = (sserialbuf_ptr) calloc(1, sizeof(struct sserialbuf) + sizeof(struct xpack) * PACK_WINDOW_RANGE);
    __xbreak(channel->sendbuf == NULL);
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
    channel->ack.cid = channel->peer_cid;
    channel->ack.key = (XMSG_VAL ^ XMSG_KEY);

    channel->flushinglist.len = 0;
    channel->flushinglist.head.prev = NULL;
    channel->flushinglist.end.next = NULL;
    channel->flushinglist.head.next = &channel->flushinglist.end;
    channel->flushinglist.end.prev = &channel->flushinglist.head;

    // 所有新创建的连接，都先进入接收队列，同时出入保活状态，检测到超时就会被释放
    __xchannel_enqueue(&msger->recv_list, channel);

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
    xmessage_ptr next;
    while (channel->send_ptr != NULL)
    {
        next = channel->send_ptr->next;
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
    channel->flushinglist.head.next = &channel->flushinglist.end;
    channel->flushinglist.end.prev = &channel->flushinglist.head;
    while(__serialbuf_sendable(channel->sendbuf) > 0)
    {
        // 减掉发送缓冲区的数据
        channel->len -=  channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->msger->len -= channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->sendbuf->spos++;
    }
    while(__serialbuf_recvable(channel->sendbuf) > 0)
    {
        // 释放待接收的消息
        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_rpos(channel->sendbuf)];
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
    while(__serialbuf_readable(channel->recvbuf) > 0)
    {
        // 释放接受缓冲区的数据
        free(channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)]);
        channel->recvbuf->rpos++;
        __atom_sub(channel->msger->rpack_malloc_count, 1);
    }
    __xlogd("xchannel_clear chnnel len: %lu pos: %lu\n", channel->len, channel->pos);
    __xlogd("xchannel_clear exit\n");
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    xchannel_clear(channel);
    __xchannel_dequeue(channel);
    free(channel->recvbuf);
    free(channel->sendbuf);
    for (int i = 0; i < 3; ++i){
        if (channel->streams[i].data != NULL){
            free(channel->streams[i].data);
        }
    }
    free(channel);
    __xlogd("xchannel_free exit\n");
}

static inline void xchannel_serial_read(xchannel_ptr channel, xpack_ptr rpack)
{
    // 只处理 sn 在 rpos 与 spos 之间的 xpack
    if (__serialbuf_recvable(channel->sendbuf) > 0 && ((uint8_t)(rpack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos))){

        xpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        if (rpack->head.ack == rpack->head.acks){

            __xlogd("xchannel_serial_read >>>>-----------> (%u) SERIAL ACKS: %u\n", channel->peer_cid, rpack->head.acks);

            uint8_t index = __serialbuf_rpos(channel->sendbuf);

            // 这里曾经使用 do while 方式，造成了收到重复的 ACK，导致 rpos 越界的 BUG
            // 连续的 acks 必须至少比 rpos 大 1
            while (channel->sendbuf->rpos != rpack->head.ack) {

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

                    __xlogd("xchannel_serial_read >>>>-----------> (%u) BACK DEALY: %lu\n", channel->peer_cid, channel->back_delay);

                    // 从定时队列中移除
                    if (pack->is_flushing){
                        pack->next->prev = pack->prev;
                        pack->prev->next = pack->next;
                        channel->flushinglist.len--;
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
                }

                __atom_add(channel->sendbuf->rpos, 1);

                // 更新索引
                index = __serialbuf_rpos(channel->sendbuf);

                // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
            }

            // 判断是否需要保活
            if (channel->send_ptr == NULL && !channel->ping){
                // 不需要保活，加入等待超时队列
                __xchannel_dequeue(channel);
                __xchannel_enqueue(&channel->msger->recv_list, channel);
            }

        } else {

            __xlogd("xchannel_serial_read >>>>-----------> (%u) OUT OF OEDER ACK: %u\n", channel->peer_cid, rpack->head.ack);

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
                if (pack->is_flushing){
                    pack->next->prev = pack->prev;
                    pack->prev->next = pack->next;
                    channel->flushinglist.len--;
                }
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
                        // 同一个包入队两次会造成死循环的 BUG
                        if (!pack->is_flushing){
                            // 将待重传的包设置为冲洗状态
                            pack->is_flushing = true;
                            pack->next = &channel->flushinglist.end;
                            pack->prev = channel->flushinglist.end.prev;
                            pack->next->prev = pack;
                            pack->prev->next = pack;
                            channel->flushinglist.len ++;
                            // 设置重传时间点
                            pack->timer = __xapi->clock() + ((uint8_t)(channel->sendbuf->spos - index)) * channel->back_delay;
                        }
                        
                        __xlogd("xchannel_serial_read >>>>---------------------------------------------------------> (%u) RESEND SN: %u\n", channel->peer_cid, pack->head.sn);
                        if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){
                            channel->msger->writable = true;
                        }else {
                            __xlogd("xchannel_serial_read >>>>------------------------> SEND FAILED\n");
                            channel->msger->writable = false;
                            break;
                        }
                    }
                }

                index++;
            }
        }

    }else {

        __xlogd("xchannel_serial_read >>>>-----------> (%u) OUT OF RANGE: %u\n", channel->peer_cid, rpack->head.sn);

    }
}

static inline void xchannel_serial_cmd(xchannel_ptr channel, uint8_t type)
{
    xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
    if (type == XMSG_PACK_HELLO){
        *(uint32_t*)(pack->body) = channel->cid;
        *(uint32_t*)(pack->body + 4) = channel->sendbuf->range;
    }else {
        *(uint64_t*)(pack->body) = __xapi->clock();
    }
    pack->head.len = sizeof(uint64_t);
    pack->msg = NULL;
    pack->head.type = type;
    pack->head.sid = 0;
    pack->head.range = 1;
    pack->channel = channel;
    pack->is_flushing = false;
    pack->head.resend = 0;
    pack->head.flag = 0;
    pack->head.cid = channel->peer_cid;
    pack->head.key = (XMSG_VAL ^ channel->peer_key);
    pack->head.sn = channel->sendbuf->wpos;
    __atom_add(channel->sendbuf->wpos, 1);
    channel->len += pack->head.len;
    channel->msger->len += pack->head.len;
    // 加入发送队列，并且从待回收队列中移除
    if(channel->worklist != &channel->msger->send_list) {
        __xchannel_dequeue(channel);
        __xchannel_enqueue(&channel->msger->send_list, channel);
    }
}

static inline void xchannel_serial_write(xchannel_ptr channel)
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
        pack->head.cid = channel->peer_cid;
        pack->head.key = (XMSG_VAL ^ channel->peer_key);
        pack->head.sn = channel->sendbuf->wpos;
        __atom_add(channel->sendbuf->wpos, 1);

        // 判断消息是否全部写入缓冲区
        if (msg->wpos == msg->len){
            // 更新当前消息
            channel->send_ptr = channel->send_ptr->next;
            msg = channel->send_ptr;
        }
    }
}

static inline void xchannel_send_pack(xchannel_ptr channel, xpack_ptr pack)
{
    // 判断发送是否成功
    if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){

        channel->msger->writable = true;

        // 数据已发送，从待发送数据中减掉这部分长度
        __atom_add(channel->pos, pack->head.len);
        __atom_add(channel->msger->pos, pack->head.len);

        // 缓冲区下标指向下一个待发送 pack
        __atom_add(channel->sendbuf->spos, 1);

        // 记录当前时间
        pack->timestamp = __xapi->clock();
        channel->timestamp = pack->timestamp;

        __xlogd("xchannel_send_pack >>>>-------------------------------> (%u) TYPE: %u SN: %u\n", channel->peer_cid, pack->head.type, pack->head.sn);
        // 判断当前 msg 是否为当前连接的消息队列中的最后一个消息
        if (pack->head.range == 1 && __serialbuf_sendable(channel->sendbuf) == 0){
            __xlogd("xchannel_send_pack >>>>-------------------------------> (%u) SET FLUSH PACK: %u\n", channel->peer_cid, pack->head.sn);
            // 设置冲洗状态
            pack->is_flushing = true;
            // 缓冲中有几个待确认的 PACK 就加上几个平均往返时长，再加上一个自身的往返时长
            pack->timer = pack->timestamp + __serialbuf_recvable(channel->sendbuf) * channel->back_delay;
            pack->next = &channel->flushinglist.end;
            pack->prev = channel->flushinglist.end.prev;
            pack->next->prev = pack;
            pack->prev->next = pack;
            channel->flushinglist.len ++;
        }

    }else {
        
        __xlogd("xchannel_send_pack >>>>------------------------> send failed\n");
        channel->msger->writable = false;
    }
}

static inline void xchannel_send_ack(xchannel_ptr channel, xhead_ptr head)
{
    if (__serialbuf_sendable(channel->sendbuf) > 0){
        // 取出当前要发送的 pack
        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)];
        pack->head.flag = head->type;
        pack->head.ack = head->ack;
        pack->head.acks = head->acks;
        __xlogd("xchannel_send_ack >>>>-------------------------------> (%u) MSG: %u ACK: %u ACKS: %u\n", channel->peer_cid, pack->head.sn, pack->head.ack, pack->head.acks);
        xchannel_send_pack(channel, pack);

    }else {
        __xlogd("xchannel_send_ack >>>>-------------------------------> (%u) ACK: %u ACKS: %u\n", channel->peer_cid, head->ack, head->acks);
        if ((__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)head, PACK_HEAD_SIZE)) == PACK_HEAD_SIZE){
            channel->msger->writable = true;
        }else {
            __xlogd("xchannel_send_ack >>>>------------------------> failed\n");
            channel->msger->writable = false;
        }
    }
}

static inline bool xchannel_recv_pack(xchannel_ptr channel, xpack_ptr *rpack)
{
    xpack_ptr pack = *rpack;

    if (pack->head.flag != XMSG_PACK_ACK){
        // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
        xchannel_serial_read(channel, pack);
    }

    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.flag = pack->head.type;

    uint16_t index = pack->head.sn & (channel->recvbuf->range - 1);

    // 如果收到连续的 PACK
    if (pack->head.sn == channel->recvbuf->wpos){

        __xlogd("xchannel_recv_pack >>>>-----------> (%u) SERIAL: %u\n", channel->peer_cid, pack->head.sn);

        pack->channel = channel;
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
                && channel->recvbuf->buf[__serialbuf_wpos(channel->recvbuf)] != pack)
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

            __xlogd("xchannel_recv_pack >>>>-----------> (%u) EARLY: %u\n", channel->peer_cid, pack->head.sn);

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack = pack->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.acks = channel->recvbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->recvbuf->wpos - 1;
            
            if (channel->recvbuf->buf[index] == NULL){
                pack->channel = channel;
                // 这个 PACK 首次到达，保存 PACK
                channel->recvbuf->buf[index] = pack;
                *rpack = NULL;
            }else {
                // 重复到达的 PACK
            }
            
        }else {

            __xlogd("xchannel_recv_pack >>>>-----------> (%u) AGAIN: %u\n", channel->peer_cid, pack->head.sn);
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.acks = channel->recvbuf->wpos;
            channel->ack.ack = channel->ack.acks;
            // 重复到达的 PACK
        }
    }

    xchannel_send_ack(channel, &channel->ack);

    return xchannel_recv_message(channel);
}


static void* recv_loop(void *ptr)
{
    __xlogd("recv_loop enter\n");

    xmessage_ptr msg;
    xpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        __xlogd("recv_loop >>>>-----> listen enter\n");
        __set_true(msger->listening);
        __xapi->udp_listen(msger->sock);
        __set_false(msger->listening);
        __set_true(msger->readable);
        __xapi->mutex_notify(msger->mtx);
        __xlogd("recv_loop >>>>-----> listen exit\n");
        
        while(xpipe_read(msger->rpipe, &pack, __sizeof_ptr) == __sizeof_ptr)
        {
            if (pack == NULL){
                break;
            }

            msg = &(pack->channel->streams[pack->head.sid]);

            if (pack->head.type == XMSG_PACK_MSG){
                if (msg->data == NULL){
                    __xlogd("recv_loop mssage range: %u\n", pack->head.range);
                    // 收到消息的第一个包，创建 msg，记录范围
                    msg->range = pack->head.range;
                    msg->data = malloc(msg->range * PACK_BODY_SIZE);
                    msg->wpos = 0;
                }
                mcopy(msg->data + msg->wpos, pack->body, pack->head.len);
                msg->wpos += pack->head.len;
                msg->range--;
                if (msg->range == 0){
                    msger->callback->on_msg_from_peer(msger->callback, pack->channel, msg->data, msg->wpos);
                    msg->data = NULL;
                }
            }

            // 这里释放的是接收到的 PACK
            free(pack);
            
            __atom_sub(msger->rpack_malloc_count, 1);
        }
    }

    __xlogd("recv_loop exit\n");

Clean:

    return NULL;
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
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 0, &addr));

    xmessage_ptr msg;
    xpack_ptr spack = NULL;
    xpack_ptr rpack = first_pack();
    __atom_add(msger->rpack_malloc_count, 1);
    // xpack_ptr rpack = (xpack_ptr)malloc(sizeof(struct xpack));
    __xbreak(rpack == NULL);
    rpack->head.len = 0;

    while (__is_true(msger->running))
    {
        // __xlogd("main_loop >>>>-----> recvfrom\n");
        // readable 是 true 的时候，接收线程一定会阻塞到接收管道上
        // readable 是 false 的时候，接收线程可能在监听 socket，或者正在给 readable 赋值为 true，所以要用原子变量
        while (__xapi->udp_recvfrom(msger->sock, &addr, &rpack->head, PACK_ONLINE_SIZE) == (rpack->head.len + PACK_HEAD_SIZE)){

            channel = (xchannel_ptr)xtree_find(msger->peers, &rpack->head.cid, 4);

            if (channel){

                __xlogd("xmsger_loop fond channel %u >>>>----> TYPE: %u  SN: %u\n", rpack->head.cid, rpack->head.type, rpack->head.sn);

                // 协议层验证
                if ((rpack->head.key ^ channel->key) == XMSG_VAL){

                    channel->timestamp = __xapi->clock();

                    if (rpack->head.type == XMSG_PACK_ACK){
                        __xlogd("xmsger_loop >>>>--------------> RECV ACK: %u ACKS: %u\n", rpack->head.ack, rpack->head.acks);
                        xchannel_serial_read(channel, rpack);
                    
                    }else if (rpack->head.type == XMSG_PACK_MSG) {
                        __xlogd("xmsger_loop >>>>--------------> RECV MSG: %u SN: %u\n", rpack->head.flag, rpack->head.sn);
                        __xbreak(!xchannel_recv_pack(channel, &rpack));

                    }else if (rpack->head.type == XMSG_PACK_PING){
                        __xlogd("xmsger_loop receive PING\n");
                        __xbreak(!xchannel_recv_pack(channel, &rpack));
                        
                    }else if (rpack->head.type == XMSG_PACK_HELLO){
                        __xlogd("xmsger_loop receive HELLO\n");
                        __xbreak(!xchannel_recv_pack(channel, &rpack));
                        
                    }else if (rpack->head.type == XMSG_PACK_BYE){
                        __xlogd("xmsger_loop receive BYE\n");
                        __xbreak(!xchannel_recv_pack(channel, &rpack));
                        xtree_take(msger->peers, &channel->cid, 4);
                        xchannel_free(channel);
                    }
                }

            } else {

                __xlogd("xmsger_loop cannot fond channel >>>>--------> TYPE: %u SN: %u  ACK: %u ACKS: %u\n", rpack->head.type, rpack->head.sn, rpack->head.ack, rpack->head.acks);

                if (rpack->head.type == XMSG_PACK_HELLO){

                    __xlogd("xmsger_loop receive HELLO\n");

                    if ((rpack->head.key ^ XMSG_KEY) == XMSG_VAL){

                        __xlogd("xmsger_loop receive HELLO NEW\n");

                        // 这里收到的是对方发起的 HELLO
                        uint32_t peer_cid = *((uint32_t*)(rpack->body));
                        uint32_t window = *((uint32_t*)(rpack->body + 4));

                        channel = (xchannel_ptr)xtree_find(msger->peers, &addr.port, addr.keylen);

                        if (channel == NULL){

                            // 这里是对端发起的 HELLO
                            // 回复 HELLO，等待对端回复的 ACK，接收到对端的 ACK，连接建立完成

                            // 创建连接
                            channel = xchannel_create(msger, &addr, false);
                            __xbreak(channel == NULL);

                            // 设置 peer cid
                            // 虽然已经设置了对端的 cid，但是对端无法通过 cid 索引到 channel，因为这时还是 addr 作为索引
                            channel->window = window;
                            channel->peer_cid = peer_cid;
                            channel->peer_key = peer_cid % 255;
                            channel->ack.cid = channel->peer_cid;
                            channel->ack.key = (XMSG_VAL ^ channel->peer_key);

                            xtree_save(msger->peers, &addr.port, addr.keylen, channel);
                            xchannel_serial_cmd(channel, XMSG_PACK_HELLO);
                            __xbreak(!xchannel_recv_pack(channel, &rpack));

                        }else { // 这里是对穿的 HELLO

                            __xlogd("xmsger_loop receive HELLO PUCHING\n");

                            // 对端会一直发重复送这个 HELLO，直到收到一个 ACK 为止

                            // 设置 peer cid 和校验码
                            channel->window = window;
                            channel->peer_cid = peer_cid;
                            channel->peer_key = peer_cid % 255;
                            channel->ack.cid = channel->peer_cid;
                            channel->ack.key = (XMSG_VAL ^ channel->peer_key);

                            // 后发起的一方负责 PING
                            if (channel->cid > peer_cid){
                                if (channel->ping){
                                    channel->ping = false;
                                    __xchannel_dequeue(channel);
                                    __xchannel_enqueue(&msger->recv_list, channel);
                                }
                            }

                            channel->connected = true;
                            msger->callback->on_channel_to_peer(msger->callback, channel);                            

                            // 各自回复 ACK，通知对端连接建立完成
                            __xbreak(!xchannel_recv_pack(channel, &rpack));
                            // 这里不上报消息，因为我们之前发出了 HEELO，所以要等到这个 HELLO 的 ACK 到来时，再上报这个消息，通知连接已建立
                        }

                    } else {

                        __xlogd("xmsger_loop receive HELLO RESULT\n");

                        // 这里收到的是对方回复的 HELLO

                        // 移除 ipaddr 索引，建立 cid 索引，连接建立完成
                        channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                        if (channel && (rpack->head.key ^ channel->key) == XMSG_VAL){
                            // 开始使用 cid 作为索引
                            xtree_save(msger->peers, &channel->cid, 4, channel);
                            // 读取连接ID和校验码
                            uint32_t peer_cid = *((uint32_t*)(rpack->body));
                            uint32_t window = *((uint32_t*)(rpack->body + 4));
                            // 设置连接校验码
                            channel->window = window;
                            channel->peer_cid = peer_cid;
                            channel->peer_key = peer_cid % 255;
                            // 设置ACK的校验码
                            channel->ack.cid = channel->peer_cid;
                            channel->ack.key = (XMSG_VAL ^ channel->peer_key);
                            // // 收到了对端回复的 HELLO，需要更新发送缓冲区，所以这里要伪造一个 ACK
                            // rpack->head.flag = XMSG_PACK_HELLO;
                            // rpack->head.acks = 1;
                            // rpack->head.ack = rpack->head.acks;

                            channel->connected = true;
                            msger->callback->on_channel_to_peer(msger->callback, channel);

                            // // 排序 ACK，更新发送缓冲区的读索引
                            // xchannel_serial_read(channel, rpack);
                            // 排序接收缓冲区，更新写索引
                            __xbreak(!xchannel_recv_pack(channel, &rpack));
                        }
                    }


                }else if (rpack->head.type == XMSG_PACK_BYE){

                    __xlogd("xmsger_loop receive BYE\n");
                    // 连接已经释放了，现在用默认的校验码
                    rpack->head.cid = 0;
                    rpack->head.key = (XMSG_VAL ^ XMSG_KEY);
                    rpack->head.flag = rpack->head.type;
                    rpack->head.type = XMSG_PACK_ACK;
                    rpack->head.acks = rpack->head.sn + 1;
                    rpack->head.ack = rpack->head.acks;
                    if ((__xapi->udp_sendto(msger->sock, &addr, (void*)&rpack->head, PACK_HEAD_SIZE)) == PACK_HEAD_SIZE){
                        msger->writable = true;
                    }else {
                        __xlogd("xchannel_send_ack >>>>------------------------> failed\n");
                        msger->writable = false;
                    }

                }else if (rpack->head.type == XMSG_PACK_ACK){
                    
                    __xlogd("xmsger_loop receive ACK\n");

                    if (rpack->head.flag == XMSG_PACK_HELLO){

                        channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                        // HELLO 的 ACK 都是生成的校验码
                        if (channel && (rpack->head.key ^ channel->key) == XMSG_VAL){
                            __xlogd("xmsger_loop receive ACK >>>>--------> channel: %u\n", channel->peer_cid);
                            // 开始使用 cid 作为索引
                            xtree_save(msger->peers, &channel->cid, 4, channel);
                            // 停止发送 HELLO
                            xchannel_serial_read(channel, rpack);
                            channel->connected = true;
                            msger->callback->on_channel_from_peer(msger->callback, channel);
                        }

                    }else if (rpack->head.flag == XMSG_PACK_BYE){

                        __xlogd("xmsger_loop receive ACK BEY\n");
                        channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                        // BYE 的 ACK 有可能是默认校验码
                        if (channel && ((rpack->head.key ^ channel->key) == XMSG_VAL || (rpack->head.key ^ XMSG_KEY) == XMSG_VAL)){
                            xchannel_free(channel);
                            // rpack 接着用
                        }
                    }
                }
            }

            if (rpack == NULL){
                // rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                rpack = next_pack();
                __atom_add(msger->rpack_malloc_count, 1);
                __xbreak(rpack == NULL);
            }

            rpack->head.len = 0;
        }

        if (__set_false(msger->readable)){
            // 通知接受线程开始监听 socket
            __xbreak(xpipe_write(msger->rpipe, &readable, __sizeof_ptr) != __sizeof_ptr);
        }

        if (xpipe_readable(msger->mpipe) > 0){
            // 连接的发起和开始发送消息，都必须经过这个管道
            __xbreak(xpipe_read(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

            // 判断连接是否存在
            if (msg->type == XMSG_PACK_MSG){

                xchannel_send_message(msg->channel, msg);

            }else {

                if (msg->type == XMSG_PACK_HELLO){

                    __xlogd("xmsger_loop >>>>-------------> create channel to peer\n");
                    channel = xchannel_create(msger, (__xipaddr_ptr)msg->data, true);
                    __xbreak(channel == NULL);
                    channel->usercontext = (void*)(*(uint64_t*)(((uint8_t*)(msg->data) + sizeof(struct __xipaddr))));
                    // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                    xtree_save(msger->peers, &channel->addr.port, channel->addr.keylen, channel);
                    xchannel_serial_cmd(channel, XMSG_PACK_HELLO);

                }else {

                    if (msg->channel != NULL){
                        __xbreak(xtree_take(msg->channel->msger->peers, &msg->channel->cid, 4) == NULL);
                        __xbreak(xtree_save(msg->channel->msger->peers, &msg->channel->addr.port, msg->channel->addr.keylen, msg->channel) == NULL);
                        xchannel_clear(msg->channel);
                        xchannel_serial_cmd(msg->channel, XMSG_PACK_BYE);
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

            while (channel != &msger->send_list.end)
            {
                next_channel = channel->next;

                if (channel->pos != channel->len){
                    xchannel_serial_write(channel);
                }
                // TODO 如能能更平滑的发送，这里是否要循环发送，知道清空缓冲区？
                // 判断缓冲区中是否有可发送 pack
                if (__serialbuf_sendable(channel->sendbuf) > 0){
                    xchannel_send_pack(channel, &channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)]);
                }

                if (channel->flushinglist.len > 0){

                    // __xlogd("xmsger_loop >>>>------------------------> flusing enter\n");

                    spack = channel->flushinglist.head.next;

                    while (spack != &channel->flushinglist.end)
                    {
                        if ((delay = (spack->timer - __xapi->clock())) > 0) {
                            // 未超时
                            if (timer > delay){
                                // 超时时间更近，更新休息时间
                                timer = delay;
                            }

                        }else {

                            if (spack->head.resend > channel->sendbuf->range){

                                __xlogd("xmsger_loop >>>>----------------------------------------------------------> (%u) SEND TIMEDOUT\n", channel->peer_cid);
                                msger->callback->on_channel_break(msger->callback, channel);

                            }else {

                                __xlogd("xmsger_loop >>>>---------------------------------------------------------> (%u) RESEND SN: %u\n", channel->peer_cid, spack->head.sn);

                                // 判断发送是否成功
                                if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(spack->head), PACK_HEAD_SIZE + spack->head.len) == PACK_HEAD_SIZE + spack->head.len){

                                    msger->writable = true;

                                    // 记录重传次数
                                    spack->head.resend++;
                                    spack->timer += channel->back_delay;

                                }else {

                                    __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                                    msger->writable = false;
                                }

                                timer = channel->back_delay;
                            }
                        }

                        spack = spack->next;
                    }

                    // __xlogd("xmsger_loop >>>>------------------------> flusing exit\n");

                }else if (channel->ping && channel->pos == channel->len && __serialbuf_readable(channel->sendbuf) == 0){

                    if ((delay = (NANO_SECONDS * 9) - (__xapi->clock() - channel->timestamp)) > 0) {
                        // 未超时
                        if (timer > delay){
                            // 超时时间更近，更新休息时间
                            timer = delay;
                        }

                    }else {
                        __xlogd("xmsger_loop >>>>---------------------------------------------------------> (%u) SEND PING\n", channel->peer_cid);
                        xchannel_serial_cmd(channel, XMSG_PACK_PING);
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

            while (channel != &msger->recv_list.end){
                next_channel = channel->next;
                // 10 秒钟超时
                if (__xapi->clock() - channel->timestamp > NANO_SECONDS * 10){
                    __xlogd("xmsger_loop >>>>----------------------------------------------------------> (%u) RECV TIEMD OUT\n", channel->peer_cid);
                    msger->callback->on_channel_break(msger->callback, channel);
                }else {
                    // 队列的第一个连接没有超时，后面的连接就都没有超时
                    break;
                }

                channel = next_channel;
            }
        }

        // __xlogd("main_loop >>>>-----> pos:%lu=%lu readable: %u listening: %u\n", msger->pos, msger->len, msger->readable, msger->listening);

        // 判断休眠条件
        // 没有待发送的包
        // 没有待发送的消息
        // 网络接口不可读
        // 接收线程已经在监听
        if (msger->pos == msger->len
            && xpipe_readable(msger->mpipe) == 0 
            && __is_false(msger->readable) 
            && __is_true(msger->listening)){
            // 如果有待重传的包，会设置冲洗定时
            // 如果需要发送 PING，会设置 PING 定时
            if (__xapi->mutex_trylock(msger->mtx)){
                __xlogd("main_loop >>>>-----> nothig to do\n");
                if (msger->pos == msger->len 
                    && xpipe_readable(msger->mpipe) == 0 
                    && __is_false(msger->readable) 
                    && __is_true(msger->listening)){
                    __xapi->mutex_timedwait(msger->mtx, timer);
                    timer = 10000000000UL; // 10 秒
                }
                __xlogd("main_loop >>>>-----> start working\n");
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
    if (!__set_true(channel->breaker)){
        __xlogd("xmsger_disconnect channel 0x%X repeated calls\n", channel);
        return true;
    }

    xmessage_ptr msg = new_message(channel, XMSG_PACK_BYE, 0, NULL, 0);

    __xbreak(msg == NULL);

    *(uint64_t*)msg->data = __xapi->clock();

    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    // 确保主线程一定会被唤醒
    __xapi->mutex_lock(msger->mtx);
    __set_true(msger->readable);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_unlock(msger->mtx);

    __xlogd("xmsger_disconnect channel 0x%X exit\n", channel);

    return true;

Clean:

    __xlogd("xmsger_disconnect channel 0x%X failed", channel);

    if (msg){
        free(msg);
    }

    return false;

}

bool xmsger_connect(xmsger_ptr msger, void *context, const char *host, uint16_t port)
{
    __xlogd("xmsger_connect enter\n");

    xmessage_ptr msg = new_message(NULL, XMSG_PACK_HELLO, 0, NULL, 0);

    __xbreak(msg == NULL);

    __xbreak(!__xapi->udp_make_ipaddr(host, port, (__xipaddr_ptr)msg->data));

    *(uint64_t*)(((uint8_t*)(msg->data) + sizeof(struct __xipaddr))) = (uint64_t)context;
    
    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    // 确保主线程一定会被唤醒
    __xapi->mutex_lock(msger->mtx);
    __set_true(msger->readable);
    __xapi->mutex_notify(msger->mtx);
    __xapi->mutex_unlock(msger->mtx);

    __xlogd("xmsger_connect exit\n");

    return true;

Clean:

    __xlogd("xmsger_connect failed\n");

    if (msg){
        free(msg);
    }
    return false;
}

xmsger_ptr xmsger_create(xmsgercb_ptr callback)
{
    __xlogd("xmsger_create enter\n");

    __xbreak(callback == NULL);

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));
    
    msger->running = true;
    msger->writable = true;
    msger->callback = callback;

    msger->sock = __xapi->udp_open();
    __xbreak(msger->sock < 0);
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 9256, &msger->addr));
    __xbreak(__xapi->udp_bind(msger->sock, &msger->addr) == -1);
    __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", 9256, &msger->addr));

    msger->send_list.len = 0;
    msger->send_list.head.prev = NULL;
    msger->send_list.end.next = NULL;
    msger->send_list.head.next = &msger->send_list.end;
    msger->send_list.end.prev = &msger->send_list.head;

    msger->recv_list.len = 0;
    msger->recv_list.head.prev = NULL;
    msger->recv_list.end.next = NULL;
    msger->recv_list.head.next = &msger->recv_list.end;
    msger->recv_list.end.prev = &msger->recv_list.head;

    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->peers = xtree_create();
    __xbreak(msger->peers == NULL);

    msger->mtx = __xapi->mutex_create();
    __xbreak(msger->mtx == NULL);

    msger->mpipe = xpipe_create(sizeof(void*) * 1024, "SEND PIPE");
    __xbreak(msger->mpipe == NULL);

    msger->rpipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(msger->rpipe == NULL);

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

        if (msger->rpipe){
            __xlogd("xmsger_free break rpipe\n");
            __xlogd("xmsger_free break pipe enter %lu\n", xpipe_readable(msger->rpipe));
            xpipe_break(msger->rpipe);
        }

        if (msger->sock > 0){
            int sock = __xapi->udp_open();
            for (int i = 0; i < 10; ++i){
                __xapi->udp_sendto(sock, &msger->addr, &i, sizeof(int));
            }
            __xapi->udp_close(sock);
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
            __xlogd("xmsger_free msg pipe\n");
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

        if (msger->rpipe){
            __xlogd("xmsger_free recv pipe enter %lu\n", xpipe_readable(msger->rpipe));
            while (xpipe_readable(msger->rpipe) > 0){
                xpack_ptr pack;
                xpipe_read(msger->rpipe, &pack, __sizeof_ptr);
                if (pack){
                    __atom_sub(msger->rpack_malloc_count, 1);
                    free(pack);
                }
            }
            __xlogd("xmsger_free recv pipe exit %lu\n", xpipe_readable(msger->rpipe));
            xpipe_free(&msger->rpipe);
        }

        __xlogd("xmsger_free clear >>>>>>>--------------------------> %lu\n", msger->rpack_malloc_count);

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