#include "xmsger.h"

#include "xtree.h"
#include "xbuf.h"

#include "xlib/avlmini.h"


enum {
    XMSG_PACK_ACK = 0x00,
    XMSG_PACK_MSG = 0x01,
    XMSG_PACK_PING = 0x02,
    XMSG_PACK_PONG = 0x04,
    XMSG_PACK_ONL = 0x08,
    XMSG_PACK_BYE = 0x10,
};


#define PACK_HEAD_SIZE              64

#ifdef __XDEBUG__
# define PACK_BODY_SIZE              64
#else
// 1024 + 256
# define PACK_BODY_SIZE              1280
#endif

#define PACK_ONLINE_SIZE            ( PACK_BODY_SIZE + PACK_HEAD_SIZE )
#define PACK_SERIAL_RANGE           64

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAXIMUM_LENGTH         ( PACK_BODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_RESEND_LIMIT       10
#define XCHANNEL_RESEND_STEPPING    1.5
#define XCHANNEL_FEEDBACK_TIMES     1000

typedef struct xhead {
    uint8_t sn; // serial number 包序列号
    uint8_t type; // 包类型
    uint8_t flag; // 标志着是否附带了 ACK 和属于那种类型的 ACK
    uint8_t ack; // 确认收到了序列号为当前 ACK 的包
    uint8_t acks; // 确认收到了这个 ACK 序列号之前的所有包
    uint8_t key; // 校验码
    uint8_t resend; // 这个包的重传计数
    uint8_t sid; // 多路复用的流 ID
    uint16_t range; // 一个消息的分包数量，从 1 到 range
    uint16_t len; // 当前分包装载的数据长度
    uint16_t cid; // 对端 CID
    uint16_t flags; // 扩展标志位
    uint8_t bytes[48];
}*xhead_ptr;

typedef struct xmsg* xmsg_ptr;

typedef struct xpack {
    uint64_t ts; // 计算往返耗时
    uint64_t delay; // 累计超时时长
    xmsg_ptr msg;
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

struct __xcid{
    union{
        uint64_t index;
        struct {
            uint16_t cid;
            uint16_t port;
            uint32_t ip;
        };
    };
};

struct xchannel {

    struct avl_node node;

    uint8_t local_key;
    uint8_t remote_key;

    uint8_t serial_range;
    uint8_t serial_number;
    uint64_t timestamp;

    uint16_t back_times;
    uint64_t back_delay;
    uint64_t back_range;

    char ip[__XAPI_IP_STR_LEN];
    uint16_t port;
    uint16_t rcid;
    struct __xcid lcid;
    struct __xipaddr addr;
    struct xchannel_ctx *ctx;

    bool keepalive;
    bool connected;
    __atom_size pos, len;

    xmsger_ptr msger;
    struct xhead ack;
    serialbuf_ptr recvbuf;
    sserialbuf_ptr sendbuf;
    struct xpacklist flushlist;
    struct xchannellist *worklist;

    xlmsg_ptr xkv;
    xmsg_ptr msg_head, *msg_tail;
    xmsg_ptr smsg;
    xchannel_ptr prev, next;
};

//channellist
typedef struct xchannellist {
    size_t len;
    struct xchannel head;
}*xchannellist_ptr;

struct xmsger {
    __atom_bool running;
    int sock;
    struct __xipaddr addr;
    uint16_t cid;
    xtree chcache;
    struct avl_tree peers;
    uint64_t timer;
    __atom_size pos, len;
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



#define XMSG_FLAG_RECV          0x01
#define XMSG_FLAG_SEND          0x02
#define XMSG_FLAG_CONNECT       0x04
#define XMSG_FLAG_DISCONNECT    0x08

typedef struct xmsg {
    uint32_t flag;
    uint32_t streamid;
    uint64_t sendpos, recvpos, range;
    struct xchannel *channel;
    struct xchannel_ctx *ctx;
    struct xmsg *prev, *next;
    struct xlmsg *lkv;
}*xmsg_ptr;

// xmsg_ptr xmsg_maker(uint32_t flag, xchannel_ptr channel, struct xchannel_ctx *ctx, xlinekv_ptr xkv);
// void xmsg_free(xmsg_ptr msg);

static inline void xmsg_fixed(xmsg_ptr msg)
{
    if (msg && msg->lkv){
        msg->recvpos = msg->sendpos = 0;
        msg->range = (msg->lkv->wpos / PACK_BODY_SIZE);
        if (msg->range * PACK_BODY_SIZE < msg->lkv->wpos){
            // 有余数，增加一个包
            msg->range ++;
        }
    }
}

xmsg_ptr xmsg_maker(uint32_t flag, xchannel_ptr channel, struct xchannel_ctx *ctx, xlmsg_ptr xkv)
{
    xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    if (msg){
        msg->flag = flag;
        msg->channel = channel;
        msg->ctx = ctx;
        msg->lkv = xkv;
        msg->prev = msg->next = NULL;
        xmsg_fixed(msg);
    }
    return msg;
}

void xmsg_free(xmsg_ptr msg)
{
    __xlogd("xmsg_free >>>>>>>>>> enter\n");
    if (msg){
        if (msg->lkv){
            xl_free(msg->lkv);
        }
        __xlogd("xmsg_free >>>>>>>>>> free\n");
        free(msg);
    }
    __xlogd("xmsg_free >>>>>>>>>> exit\n");
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, uint8_t serial_range)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xbreak(channel == NULL);

    channel->connected = false;

    channel->msger = msger;
    channel->serial_range = serial_range;
    channel->timestamp = __xapi->clock();

    channel->local_key = channel->timestamp % XMSG_KEY;
    channel->serial_number = channel->timestamp % channel->serial_range;

    channel->remote_key = XMSG_KEY;
    channel->back_delay = 300000000UL;
    channel->ack.key = (XMSG_VAL ^ XMSG_KEY);

    channel->recvbuf = (serialbuf_ptr) calloc(1, sizeof(struct serialbuf) + sizeof(xpack_ptr) * channel->serial_range);
    __xbreak(channel->recvbuf == NULL);
    channel->recvbuf->range = channel->serial_range;

    channel->sendbuf = (sserialbuf_ptr) calloc(1, sizeof(struct sserialbuf) + sizeof(struct xpack) * channel->serial_range);
    __xbreak(channel->sendbuf == NULL);
    channel->sendbuf->range = channel->serial_range;
    channel->sendbuf->rpos = channel->sendbuf->spos = channel->sendbuf->wpos = channel->serial_number;

    channel->flushlist.len = 0;
    channel->flushlist.head.prev = &channel->flushlist.head;
    channel->flushlist.head.next = &channel->flushlist.head;

    // 所有新创建的连接，都先进入接收队列，同时出入保活状态，检测到超时就会被释放
    __xchannel_put_into_list(&msger->recv_list, channel);

    channel->msg_tail = &channel->msg_head;

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

    xmsg_ptr msg, next;
    // 如果有未确认的 msg，就清理待确认队列
    if (channel->smsg != NULL){
        msg = channel->smsg;
    }else {
        // 如果没有待确认 msg，就清理发送队列
        msg = channel->msg_head;
    }
    // 重置消息发送队列
    channel->smsg = channel->msg_head = NULL;
    channel->msg_tail = &channel->msg_head;
    // 后释放没有发送出去的 msg
    while (msg != NULL){
        __xlogd("xchannel_clear ------------------------- msg\n");
        next = msg->next;
        // 减掉不能发送的数据长度，有的 msg 可能已经发送了一半，所以要减掉 sendpos
        channel->len -= msg->lkv->wpos - msg->sendpos;
        channel->msger->len -= msg->lkv->wpos - msg->sendpos;
        xl_free(msg->lkv);
        free(msg);
        msg = next;
    }
    // 清空冲洗队列
    channel->flushlist.len = 0;
    channel->flushlist.head.prev = &channel->flushlist.head;
    channel->flushlist.head.next = &channel->flushlist.head;
    // 刷新发送队列
    while(__serialbuf_sendable(channel->sendbuf) > 0){
        // 减掉发送缓冲区的数据
        channel->len -=  channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->msger->len -= channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)].head.len;
        channel->sendbuf->spos++;
    }
    channel->sendbuf->rpos = channel->sendbuf->spos;
    // 缓冲区里的包有可能不是连续的，所以不能顺序的清理，如果使用 rpos 就会出现内存泄漏的 BUG
    for (int i = 0; i < channel->recvbuf->range; ++i){
        if (channel->recvbuf->buf[i] != NULL){
            free(channel->recvbuf->buf[i]);
            // 这里要置空，否则重复调用这个函数，会导致崩溃
            channel->recvbuf->buf[i] = NULL;
        }
    }
    // 释放未完整接收的消息
    if (channel->xkv != NULL){
        xl_free(channel->xkv);
        channel->xkv = NULL;
    }
    __xlogd("xchannel_clear exit\n");
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    xchannel_clear(channel);
    __xchannel_take_out_list(channel);
    free(channel->recvbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_free exit\n");
}

static inline void xchannel_serial_pack(xchannel_ptr channel, uint8_t type)
{
    xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
    *(uint8_t*)(pack->body) = channel->local_key;
    *(uint8_t*)(pack->body + 1) = channel->serial_range;
    *(uint8_t*)(pack->body + 2) = channel->serial_number;
    *(uint16_t*)(pack->body + 3) = channel->lcid.cid;
    pack->head.len = 5;
    pack->msg = NULL;
    pack->head.type = type;
    pack->head.sid = 0;
    pack->head.range = 1;
    pack->channel = channel;
    pack->delay = channel->back_delay;
    pack->head.resend = 0;
    pack->head.flag = 0;
    pack->head.key = (XMSG_VAL ^ channel->remote_key);
    pack->head.sn = channel->sendbuf->wpos;
    pack->head.cid = channel->rcid;
    __atom_add(channel->sendbuf->wpos, 1);
    channel->len += pack->head.len;
    channel->msger->len += pack->head.len;
    // 加入发送队列，并且从待回收队列中移除
    if(channel->worklist != &channel->msger->send_list) {
        __xchannel_take_out_list(channel);
        __xchannel_put_into_list(&channel->msger->send_list, channel);
    }
}

static inline void xchannel_serial_msg(xchannel_ptr channel)
{
    xmsg_ptr msg = channel->msg_head;

    // 每次只缓冲一个包，尽量使发送速度均匀
    if (channel->connected && msg != NULL && __serialbuf_writable(channel->sendbuf) > 0){

        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
        pack->msg = msg;
        if (msg->lkv->wpos - msg->sendpos < PACK_BODY_SIZE){
            pack->head.len = msg->lkv->wpos - msg->sendpos;
        }else{
            pack->head.len = PACK_BODY_SIZE;
        }

        pack->head.type = XMSG_PACK_MSG;
        pack->head.sid = msg->streamid;
        pack->head.range = msg->range;
        mcopy(pack->body, msg->lkv->body + msg->sendpos, pack->head.len);
        msg->sendpos += pack->head.len;
        msg->range --;

        pack->channel = channel;
        pack->delay = channel->back_delay;
        pack->head.resend = 0;
        pack->head.flag = 0;
        pack->head.key = (XMSG_VAL ^ channel->remote_key);
        pack->head.sn = channel->sendbuf->wpos;
        pack->head.cid = channel->rcid;
        __atom_add(channel->sendbuf->wpos, 1);
        // 判断消息是否全部写入缓冲区
        if (msg->sendpos == msg->lkv->wpos){
            // 更新当前消息
            channel->msg_head = channel->msg_head->next;
            if (channel->msg_head == NULL){
                channel->msg_tail = &channel->msg_head;
            }
        }
    }
}

static inline void xchannel_send_msg(xchannel_ptr channel, xmsg_ptr msg)
{
    // 更新待发送计数
    __atom_add(channel->len, msg->lkv->wpos);
    __atom_add(channel->msger->len, msg->lkv->wpos);

    // 判断是否有正在发送的消息
    if ((*(channel->msg_tail)) != NULL){
        // 将新成员的 next 指向消息队列尾部
        (*(channel->msg_tail))->next = msg;
        // 将消息队列的尾部指针指向新成员
        channel->msg_tail = &msg;
        
    }else {

        // 设置新消息为当前正在发送的消息
        channel->msg_head = msg;
        channel->smsg = channel->msg_head;
        xchannel_serial_msg(channel);
    }

    // 加入发送队列，并且从待回收队列中移除
    if(channel->worklist != &channel->msger->send_list) {
        __xchannel_take_out_list(channel);
        __xchannel_put_into_list(&channel->msger->send_list, channel);
    }
}

static inline void xchannel_send_pack(xchannel_ptr channel)
{
    if (__serialbuf_sendable(channel->sendbuf) > 0){

        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)];

        __xlogd("<SEND> TYPE[%u] IP[%X] CID[%u] FLAG[%u:%u:%u] >>>>-------------------> SN[%u]\n", 
            pack->head.type, channel->lcid.index, channel->lcid.cid, pack->head.flag, pack->head.ack, pack->head.acks, pack->head.sn);

        if (channel->ack.flag != 0){
            // 携带 ACK
            pack->head.flag = channel->ack.flag;
            pack->head.ack = channel->ack.ack;
            pack->head.acks = channel->ack.acks;
        }

        // 判断发送是否成功
        if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){

            // 发送成功才能清空标志
            channel->ack.flag = 0;

            // 数据已发送，从待发送数据中减掉这部分长度
            __atom_add(channel->pos, pack->head.len);
            __atom_add(channel->msger->pos, pack->head.len);

            // 缓冲区下标指向下一个待发送 pack
            __atom_add(channel->sendbuf->spos, 1);

            // 记录发送次数
            pack->head.resend = 1;

            // 记录当前时间
            pack->ts = __xapi->clock();
            pack->delay = channel->back_delay * XCHANNEL_RESEND_STEPPING;
            channel->timestamp = pack->ts;
            __ring_list_put_into_end(&channel->flushlist, pack);

            // 如果有待发送数据，确保 sendable 会大于 0
            xchannel_serial_msg(channel);

        }else {

            __xlogd("xchannel_send_pack >>>>------------------------> send failed\n");
        }
    }
}

static inline void xchannel_send_ack(xchannel_ptr channel)
{
    __xlogd("<SEND> TYPE[%u] IP[%X] CID[%u] FLAG[%u:%u:%u]\n", 
        channel->ack.type, channel->lcid.index, channel->lcid.cid, channel->ack.flag, channel->ack.ack, channel->ack.acks);
    if ((__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&channel->ack, PACK_HEAD_SIZE)) == PACK_HEAD_SIZE){
        channel->ack.flag = 0;
    }else {
        __xlogd("xchannel_send_ack >>>>------------------------> failed\n");
    }
}

static inline void xchannel_send_final(xmsger_ptr msger, __xipaddr_ptr addr, xpack_ptr rpack)
{
    __xlogd("xchannel_send_final >>>>-----------------------------------------------------------> SEND FINAL ACK\n");
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

static inline void xchannel_recv_msg(xchannel_ptr channel)
{
    if (__serialbuf_readable(channel->recvbuf) > 0){

        // xmsg_ptr msg;
        // 索引已接收的包
        xpack_ptr pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        do {
            if (pack->head.type == XMSG_PACK_MSG){
                // 如果当前消息的数据为空，证明这个包是新消息的第一个包
                if (channel->xkv == NULL){
                    channel->xkv = xl_create(pack->head.range * PACK_BODY_SIZE);
                    __xbreak(channel->xkv == NULL);
                    // 收到消息的第一个包，为当前消息分配资源，记录消息的分包数
                    // channel->rmsg->range = pack->head.range;
                }
                mcopy(channel->xkv->body + channel->xkv->wpos, pack->body, pack->head.len);
                channel->xkv->wpos += pack->head.len;
                // channel->rmsg->range--;
                if (channel->xkv->size - channel->xkv->wpos < PACK_BODY_SIZE){
                    // 更新消息长度
                    xl_fixed(channel->xkv);
                    xl_printf(&channel->xkv->line);
                    // 通知用户已收到一个完整的消息
                    channel->msger->callback->on_msg_from_peer(channel->msger->callback, channel, channel->xkv);
                    channel->xkv = NULL;
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

Clean:

    return;
}

static inline void xchannel_recv_ack(xchannel_ptr channel, xpack_ptr rpack)
{
    __xlogd("xchannel_recv_ack >>>>-----------> ack[%u:%u] rpos=%u spos=%u ack-rpos=%u spos-rpos=%u\n", 
            rpack->head.ack, rpack->head.acks, channel->sendbuf->rpos, channel->sendbuf->spos, 
            (uint8_t)(rpack->head.ack - channel->sendbuf->rpos), (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos));

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
        while ((uint8_t)(rpack->head.acks - channel->sendbuf->rpos) 
                <= (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos) // 防止后到的 acks 比之前的 acks 和 rpos 小，导致 rpos 越界
                && channel->sendbuf->rpos != rpack->head.acks) {

            pack = &channel->sendbuf->buf[index];

            if (pack->ts != 0){
                // 累计新的一次往返时长
                channel->back_range += (__xapi->clock() - pack->ts);
                pack->ts = 0;

                if (channel->back_times < XCHANNEL_FEEDBACK_TIMES){
                    // 更新累计次数
                    channel->back_times++;
                }else {
                    // 已经到达累计次数，需要减掉一次平均时长
                    channel->back_range -= channel->back_delay;
                }
                // 重新计算平均时长
                channel->back_delay = channel->back_range / channel->back_times;

                __ring_list_take_out(&channel->flushlist, pack);

                // 判断是否有未发送的消息
                if (channel->msg_head == NULL){
                    // 判断是否有待重传的包，再判断是否需要保活
                    if (!channel->keepalive && __serialbuf_sendable(channel->sendbuf) == 0 && channel->flushlist.len == 0){
                        // 不需要保活，加入等待超时队列
                        __xchannel_take_out_list(channel);
                        __xchannel_put_into_list(&channel->msger->recv_list, channel);
                    }
                }
            }

            if (pack->head.type == XMSG_PACK_MSG){
                // 更新已经到达对端的数据计数
                pack->msg->recvpos += pack->head.len;
                if (pack->msg->recvpos == pack->msg->lkv->wpos){
                    __xlogd("xchannel_recv_ack >>>>-----------> SN[%u] TYPE(%u)\n", pack->head.sn, pack->head.type);
                    // 更新待确认队列
                    channel->smsg = pack->msg->next;
                    // 把已经传送到对端的 msg 交给发送线程处理
                    channel->msger->callback->on_msg_to_peer(channel->msger->callback, channel, pack->msg->lkv);
                    pack->msg->lkv = NULL;
                    xmsg_free(pack->msg);
                }
            }else if (rpack->head.type == XMSG_PACK_PONG){
                __xlogd("xchannel_recv_ack >>>>-----------> SN[%u] TYPE(%u)\n", pack->head.sn, pack->head.type);
                // 拼装临时 cid
                struct __xcid cid;
                cid.cid = channel->rcid;
                cid.port = channel->addr.port;
                cid.ip = channel->addr.ip;
                // 移除正在建立连接标志
                xtree_del(channel->msger->chcache, &cid.index, 8);
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

            // __xlogd("xchannel_recv_ack >>>>-----------> (%u) OUT OF OEDER ACK: %u\n", channel->peer_cid, rpack->head.ack);

            pack = &channel->sendbuf->buf[rpack->head.ack & (channel->sendbuf->range - 1)];

            if (pack->ts != 0){
                // 累计新的一次往返时长
                channel->back_range += (__xapi->clock() - pack->ts);
                pack->ts = 0;
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
                __ring_list_take_out(&channel->flushlist, pack);
            }

            // ack 与 rpos 的间隔大于一才进行重传
            if (((rpack->head.ack - channel->sendbuf->rpos) & (channel->sendbuf->range - 1)) > 1){
                // 使用临时变量
                uint8_t index = channel->sendbuf->rpos;
                // 实时重传 rpos 到 SN 之间的所有尚未确认的 SN
                while (index != rpack->head.ack){
                    // 取出落后的包
                    pack = &channel->sendbuf->buf[(index & (channel->sendbuf->range - 1))];
                    // 判断这个包是否已经接收过
                    if (pack->ts != 0){
                        // 判断是否进行了重传
                        if (pack->head.resend < 2){
                            pack->head.resend++;
                            // 判断重传的包是否带有 ACK
                            if (pack->head.flag != 0){
                                // 更新 ACKS
                                // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                pack->head.acks = channel->recvbuf->wpos;
                            }
                            __xlogd("RESEND >>>>>>>>>>------------> PACK: %u\n", pack->head.sn);
                            if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len) == PACK_HEAD_SIZE + pack->head.len){
                                pack->delay *= XCHANNEL_RESEND_STEPPING;
                            }else {
                                __xlogd("xchannel_recv_ack >>>>------------------------> send failed\n");
                            }
                        }
                    }

                    index++;
                }
            }
        }

    }else {

        // __xlogd("xchannel_recv_ack >>>>-----------> (%u) OUT OF RANGE: %u\n", channel->peer_cid, rpack->head.sn);

    }
}

static inline void xchannel_recv_pack(xchannel_ptr channel, xpack_ptr *rpack)
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
            xchannel_recv_ack(channel, pack);
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

            // __xlogd("xchannel_recv_pack >>>>-----------> (%u) EARLY: %u\n", channel->peer_cid, pack->head.sn);

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
                    xchannel_recv_ack(channel, pack);
                }
                // 这个 PACK 首次到达，保存 PACK
                channel->recvbuf->buf[index] = pack;
                *rpack = NULL;
            }else {
                // 重复到达的 PACK
            }
            
        }else {

            // __xlogd("xchannel_recv_pack >>>>-----------> (%u) AGAIN: %u\n", channel->peer_cid, pack->head.sn);
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.acks = channel->recvbuf->wpos;
            channel->ack.ack = channel->ack.acks;
            // 重复到达的 PACK
        }
    }

    xchannel_recv_msg(channel);
}


static inline bool xmsger_process_msg(xmsger_ptr msger, xmsg_ptr msg)
{
    xchannel_ptr channel = msg->channel;

    if (msg->flag == XMSG_FLAG_SEND){

        xchannel_send_msg(msg->channel, msg);

    }else if (msg->flag == XMSG_FLAG_CONNECT){

        channel = xchannel_create(msger, PACK_SERIAL_RANGE);
        if(channel == NULL){
            return false;
        }

        xlmsg_t parser = xl_parser(&msg->lkv->line);
        char *ip = xl_find_word(&parser, "ip");
        uint64_t port = xl_find_uint(&parser, "port");
        channel->ctx = xl_find_ptr(&parser, "ctx");
        xlmsg_ptr req = xl_find_ptr(&parser, "msg");

        mcopy(channel->ip, ip, slength(ip));
        channel->port = port;
        __xapi->udp_host_to_addr(channel->ip, channel->port, &channel->addr);
        channel->lcid.port = channel->addr.port;
        channel->lcid.ip = channel->addr.ip;

        do {
            channel->lcid.cid = msger->cid++;
        }while (avl_tree_add(&msger->peers, channel) != NULL);

        channel->rcid = channel->lcid.cid;

        channel->keepalive = true;

        // 先用本端的 cid 作为对端 channel 的索引，重传这个 HELLO 就不会多次创建连接
        xchannel_serial_pack(channel, XMSG_PACK_PING);

        if (req != NULL){
            xl_printf(&req->line);
            xl_free(msg->lkv);
            msg->lkv = req;
            xmsg_fixed(msg);
            xchannel_send_msg(channel, msg);
            // firstmsg = firstmsg->next;
        }else {
            free(msg);
        }

        __xlogd("xmsger_process_msg >>>>--------> Create channel IP=[%X] PORT=[%u] CID[%u] KEY[%u] SN[%u]\n", 
                                    channel->lcid.index, port, channel->lcid.cid, channel->local_key, channel->serial_number);

    }else if (msg->flag == XMSG_FLAG_DISCONNECT){

        if (msg->channel != NULL){
            channel = msg->channel;
            __xlogd("xmsger_process_msg >>>>--------> Release channel IP=[%X] PORT=[%u] CID[%u] KEY[%u] SN[%u]\n", 
                                        channel->lcid.index, channel->port, channel->lcid.cid, channel->local_key, channel->serial_number);
            xchannel_clear(channel);
            xchannel_serial_pack(channel, XMSG_PACK_BYE);
            // 移除链接池
            avl_tree_remove(&msger->peers, channel);
            // 换成对端 cid 作为当前的索引，因为对端已经不再持有本端的 cid，收到 BYE 时直接回复 ACK 即可
            channel->lcid.cid = channel->rcid;
            // 加入暂存链接池
            xtree_add(msger->chcache, &channel->lcid.index, 8, channel);
        }

        free(msg);
    }

    return true;
}

static inline void xmsger_send_all(xmsger_ptr msger)
{
    // 判断待发送队列中是否有内容
    if (msger->send_list.len > 0){

        int64_t delay;
        xpack_ptr spack;
        xchannel_ptr channel, next_channel;

        // 从头开始，每个连接发送一个 pack
        channel = msger->send_list.head.next;

        while (channel != &msger->send_list.head)
        {
            next_channel = channel->next;

            // readable 是已经写入缓冲区还尚未发送的包
            // 缓冲的包小于缓冲区的8/1时，在这里发送，剩余的可写缓冲区留给回复 ACK 时候，如果有数据待发送，可以与 ACK 一起发送
            if (__serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 3)){
                xchannel_send_pack(channel);
                // 发送缓冲区未到达8/1，且有消息未发送完，主线程不休眠
                if (channel->msg_head != NULL){
                    msger->timer = 0;
                }
            }

            if (channel->flushlist.len > 0){

                spack = channel->flushlist.head.next;

                while (spack != &channel->flushlist.head)
                {
                    xpack_ptr next_pack = spack->next;

                    if ((delay = ((spack->ts + spack->delay) - __xapi->clock())) > 0) {
                        // 未超时
                        if (msger->timer > delay){
                            // 超时时间更近，更新休息时间
                            msger->timer = delay;
                        }
                        // 第一个包未超时，后面的包就都没有超时
                        break;

                    }else {

                        if (spack->delay > NANO_SECONDS * XCHANNEL_RESEND_LIMIT)
                        {
                            if (channel->connected){
                                msger->callback->on_msg_timeout(msger->callback, channel);
                            }else {
                                msger->callback->on_connect_timeout(msger->callback, channel);
                            }
                            // 移除超时的连接
                            avl_tree_remove(&msger->peers, channel);
                            xchannel_free(channel);
                            break;

                        }else {

                            // 超时重传

                            // 判断重传的包是否带有 ACK
                            if (spack->head.flag != 0){
                                // 更新 ACKS
                                // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                spack->head.acks = channel->recvbuf->wpos;
                                // TODO ack 也要更新
                            }

                            __xlogd("RESEND >>>>>>>>>>------------> PACK: %u\n", spack->head.sn);

                            // 判断发送是否成功
                            if (__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(spack->head), PACK_HEAD_SIZE + spack->head.len) == PACK_HEAD_SIZE + spack->head.len){
                                // 记录重传次数
                                spack->head.resend++;
                                // spack->delay *= XCHANNEL_RESEND_STEPPING;
                                // 最后一个待确认包的超时时间加上平均往返时长
                                spack->delay *= XCHANNEL_RESEND_STEPPING;
                                // 列表中如果只有一个成员，就不能更新包的位置
                                if (channel->flushlist.len > 1){
                                    // 重传之后的包放入队尾
                                    __ring_list_move_to_end(&channel->flushlist, spack);
                                }
                            }else {
                                __xlogd(">>>>------------------------> send failed\n");
                            }

                            if (msger->timer > channel->back_delay){
                                msger->timer = channel->back_delay;
                            }
                        }
                    }

                    spack = next_pack;
                }

            }else if (channel->keepalive && channel->pos == channel->len && __serialbuf_readable(channel->sendbuf) == 0){

                if ((delay = (NANO_SECONDS * 9) - (__xapi->clock() - channel->timestamp)) > 0) {
                    // 未超时
                    if (msger->timer > delay){
                        // 超时时间更近，更新休息时间
                        msger->timer = delay;
                    }

                }else {
                    xchannel_serial_pack(channel, XMSG_PACK_ONL);
                    // 更新时间戳
                    channel->timestamp = __xapi->clock();

                }
            }

            channel = next_channel;
        }
    }
}

static inline void xmsger_check_all(xmsger_ptr msger)
{
    if (msger->recv_list.len > 0){
        
        xchannel_ptr next_channel, channel = msger->recv_list.head.next;

        while (channel != &msger->recv_list.head){
            next_channel = channel->next;
            // 10 秒钟超时
            if (__xapi->clock() - channel->timestamp > NANO_SECONDS * 10){
                __xlogd("main_loop (IP:%X) >>>>>---------------------------------> on_disconnect (%lu)\n", *(uint64_t*)(&channel->addr.port), __xapi->clock() - channel->timestamp);
                msger->callback->on_disconnect(msger->callback, channel);
                // 移除超时的连接
                avl_tree_remove(&msger->peers, channel);
                xchannel_free(channel);
            }else {
                // 队列的第一个连接没有超时，后面的连接就都没有超时
                break;
            }

            channel = next_channel;
        }
    }

}

static void* main_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    xmsger_ptr msger = (xmsger_ptr)ptr;

    xmsg_ptr msg;
    xchannel_ptr channel = NULL;

    struct __xcid cid;
    struct __xipaddr addr;
    __xbreak(!__xapi->udp_host_to_addr(NULL, 0, &addr));

    xpack_ptr rpack = (xpack_ptr)malloc(sizeof(struct xpack));
    __xbreak(rpack == NULL);
    rpack->head.len = 0;

    msger->timer = 10000000UL; // 10 毫秒

    while (msger->running)
    {
        while (__xapi->udp_recvfrom(msger->sock, &addr, &rpack->head, PACK_ONLINE_SIZE) == (rpack->head.len + PACK_HEAD_SIZE)){

            cid.cid = rpack->head.cid;
            cid.port = addr.port;
            cid.ip = addr.ip;

            __xlogd("[RECV] TYPE(%u) IP(%X) CID(%u) FLAG(%u:%u:%u) >>>>--------> SN(%u)\n", 
                    rpack->head.type, cid.index, rpack->head.cid, rpack->head.flag, rpack->head.ack, rpack->head.acks, rpack->head.sn);

            channel = avl_tree_find(&msger->peers, &cid);

            if (channel){

                // 协议层验证
                if ((rpack->head.key ^ channel->local_key) == XMSG_VAL){

                    if (rpack->head.type == XMSG_PACK_MSG) {
                        xchannel_recv_pack(channel, &rpack);
                        if (channel->ack.flag != 0){
                            if (__serialbuf_sendable(channel->sendbuf) > 0 && __serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 1)){
                                xchannel_send_pack(channel);
                            }else {
                                xchannel_send_ack(channel);
                            }
                        }
                    }else if (rpack->head.type == XMSG_PACK_ACK){
                        xchannel_recv_ack(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_ONL){
                        xchannel_recv_pack(channel, &rpack);
                        if (channel->ack.flag != 0){
                            xchannel_send_ack(channel);
                        }

                    }else if (rpack->head.type == XMSG_PACK_PONG){
                        if (!channel->connected){
                            channel->connected = true;
                            // 取出同步参数
                            uint8_t remote_key = *((uint8_t*)(rpack->body));
                            uint8_t serial_range = *((uint8_t*)(rpack->body + 1));
                            uint8_t serial_number = *((uint8_t*)(rpack->body + 2));
                            uint16_t rcid = *((uint16_t*)(rpack->body + 3));
                            __xlogd("xmsger_loop >>>>-------------> RECV PONG: REMOTE CID(%u) KEY(%u) SN(%u)\n", rcid, remote_key, serial_number);
                            __xlogd("xmsger_loop >>>>-------------> RECV PONG: LOCAL  CID(%u) KEY(%u) SN(%u)\n", channel->lcid.cid, channel->local_key, channel->serial_number);
                            // 更新 rcid
                            channel->rcid = rcid;
                            // 同步序列号
                            channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                            // 设置校验码
                            channel->remote_key = remote_key;
                            // 生成 ack 的校验码
                            channel->ack.key = (XMSG_VAL ^ channel->remote_key);
                            channel->ack.cid = channel->rcid;
                            // channel->ack.lcid = channel->lcid.cid;
                            // 通知用户建立连接
                            msger->callback->on_connect_to_peer(msger->callback, channel);                            
                        }
                        // 更新接收缓冲区和 ACK
                        xchannel_recv_pack(channel, &rpack);
                        // 回复 ACK
                        if (channel->ack.flag != 0){
                            xchannel_send_ack(channel);
                        }
                        // 发送创建连接时附带的消息
                        if (channel->msg_head){
                            xchannel_serial_msg(channel);
                        }

                    }else if (rpack->head.type == XMSG_PACK_BYE){
                        __xlogd("on_disconnect %p\n", channel);
                        msger->callback->on_disconnect(msger->callback, channel);
                        // 被动端收到 BYE，删除索引
                        avl_tree_remove(&msger->peers, channel);
                        xchannel_free(channel);
                        // 回复最后的 ACK
                        xchannel_send_final(msger, &addr, rpack);

                    }else {
                        __xlogd("main_loop pack type error\n");
                    }

                }else {
                    __xlogd("main_loop pack key error\n");
                }

            } else {

                if (rpack->head.type == XMSG_PACK_PING){
                    // 收到对方发起的 PING
                    if ((rpack->head.key ^ XMSG_KEY) == XMSG_VAL){
                        channel = xtree_find(msger->chcache, &cid.index, 8);
                        // 检测是否在正在建立连接的过程中又发起了同样 cid 的新连接
                        if (rpack->head.resend == 0 && channel != NULL){
                            // 如果 PING 第一次到达，需要释放之前相同 cid 的正在建立连接的 channel
                            xtree_del(msger->chcache, &cid.index, 8);
                            xchannel_free(channel);
                            channel = NULL;
                        }
                        if (channel == NULL){
                            // 取出同步参数
                            uint8_t remote_key = *((uint8_t*)(rpack->body));
                            uint8_t serial_range = *((uint8_t*)(rpack->body + 1));
                            uint8_t serial_number = *((uint8_t*)(rpack->body + 2));
                            uint16_t rcid = *((uint16_t*)(rpack->body + 3));
                            // 创建连接
                            channel = xchannel_create(msger, serial_range);
                            __xbreak(channel == NULL);
                            // 暂存连接，收到 PONG 的 ACK 时清除，以免收到重复的 PING 导致重复创建连接
                            xtree_add(msger->chcache, &cid.index, 8, channel);
                            channel->addr = addr;
                            channel->lcid.port = addr.port;
                            channel->lcid.ip = addr.ip;
                            // 建立索引
                            do {
                                channel->lcid.cid = msger->cid++;
                            }while (avl_tree_add(&msger->peers, channel) != NULL);
                            // 同步序列号
                            channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                            // 设置校验码
                            channel->remote_key = remote_key;
                            // 设置对端 cid
                            channel->rcid = rcid;
                            // 生成 ack 的校验码
                            channel->ack.key = (XMSG_VAL ^ channel->remote_key);
                            channel->ack.cid = rcid;
                            __xlogd("xmsger_loop >>>>-------------> RECV PING: REMOTE CID(%u) KEY(%u) SN(%u)\n", rcid, remote_key, serial_number);
                            __xlogd("xmsger_loop >>>>-------------> RECV PING: LOCAL  CID(%u) KEY(%u) SN(%u)\n", channel->lcid.cid, channel->local_key, channel->serial_number);
                            channel->connected = true;
                            msger->callback->on_connect_from_peer(msger->callback, channel);

                            // 更新接收缓冲区和 ACK
                            xchannel_recv_pack(channel, &rpack);
                            // 生成 PONG
                            xchannel_serial_pack(channel, XMSG_PACK_PONG);
                            // 发送 PONG 和更新的 ACK
                            xchannel_send_pack(channel);

                        }else {

                            xchannel_recv_pack(channel, &rpack);
                            if (channel->ack.flag != 0){
                                xchannel_send_ack(channel);
                            }
                        }

                    }else {}

                }else if (rpack->head.type == XMSG_PACK_BYE){

                    // 被动端收到重复的 BYE，回复最后的 ACK
                    xchannel_send_final(msger, &addr, rpack);

                }else if (rpack->head.type == XMSG_PACK_ACK){
                    
                    if (rpack->head.flag == XMSG_PACK_BYE){
                        // 主动端收到 BYE 的 ACK，移除索引
                        channel = xtree_find(msger->chcache, &cid.index, 8);
                        if (channel){
                            msger->callback->on_disconnect(msger->callback, channel);
                            xtree_del(msger->chcache, &cid.index, 8);
                            xchannel_free(channel);
                        }
                    }

                }else {}
            }

            if (rpack == NULL){
                rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                __xbreak(rpack == NULL);
            }

            rpack->head.len = 0;

            if (__xpipe_read(msger->mpipe, (uint8_t*)&msg, __sizeof_ptr) == __sizeof_ptr){
                __xpipe_notify(msger->mpipe);
                __xbreak(!xmsger_process_msg(msger, msg));
            }            

            // 检查每个连接，如果满足发送条件，就发送一个数据包
            xmsger_send_all(msger);
            // 检查超时连接
            xmsger_check_all(msger);
        }

        if (__xpipe_read(msger->mpipe, (uint8_t*)&msg, __sizeof_ptr) == __sizeof_ptr){
            __xpipe_notify(msger->mpipe);
            __xbreak(!xmsger_process_msg(msger, msg));
        }else {
            __xapi->udp_listen(msger->sock, msger->timer / 1000);
        }

        msger->timer = 10000000UL; // 10 毫秒

        // 检查每个连接，如果满足发送条件，就发送一个数据包
        xmsger_send_all(msger);
        // 检查超时连接
        xmsger_check_all(msger);
    }


    Clean:


    if (rpack != NULL){
        free(rpack);
    }

    __xlogd("xmsger_loop exit\n");

    return NULL;
}

static inline bool xmsger_send(xmsger_ptr msger, xmsg_ptr msg)
{
    return (xpipe_write(msger->mpipe, (uint8_t*)&msg, __sizeof_ptr) == __sizeof_ptr);
}

bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, xlmsg_ptr xkv)
{
    __xlogd("xmsger_send_message enter\n");
    __xcheck(channel == NULL || xkv == NULL);
    xmsg_ptr msg = xmsg_maker(XMSG_FLAG_SEND, channel, channel->ctx, xkv);
    __xcheck(msg == NULL);
    __xcheck((xpipe_write(msger->mpipe, (uint8_t*)&msg, __sizeof_ptr) != __sizeof_ptr));
    __xlogd("xmsger_send_message exit\n");

    return true;

XClean:

    return false;
}

bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel)
{
    __xlogd("xmsger_disconnect channel 0x%X enter\n", channel);

    // 避免重复调用
    if (!__set_false(channel->connected)){
        __xlogd("xmsger_disconnect channel 0x%X repeated calls\n", channel);
        return true;
    }

    xmsg_ptr msg = xmsg_maker(XMSG_FLAG_DISCONNECT, channel, channel->ctx, NULL);
    __xcheck(msg == NULL);
    __xcheck((xpipe_write(msger->mpipe, (uint8_t*)&msg, __sizeof_ptr) != __sizeof_ptr));

    __xlogd("xmsger_disconnect channel 0x%X exit\n", channel);

    return true;

XClean:

    __xlogd("xmsger_disconnect channel 0x%X failed", channel);
    if (msg){
        free(msg);
    }
    return false;

}

bool xmsger_connect(xmsger_ptr msger, const char *ip, uint16_t port, void *ctx, xlmsg_ptr firstmsg)
{
    __xlogd("xmsger_connect enter\n");

    __xlogd("xmsger_connect ip=%s\n", ip);

    xlmsg_ptr xkv = xl_maker();
    __xcheck(xkv == NULL);

    xl_add_word(xkv, "ip", ip);
    xl_add_uint(xkv, "port", port);
    xl_add_ptr(xkv, "ctx", ctx);
    xl_add_ptr(xkv, "msg", firstmsg);

    xmsg_ptr msg = xmsg_maker(XMSG_FLAG_CONNECT, NULL, ctx, xkv);
    __xcheck(msg == NULL);

    __xcheck((xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr));

    __xlogd("xmsger_connect exit\n");

    return true;

XClean:

    __xlogd("xmsger_connect failed\n");
    if (xkv){
        xl_free(xkv);
    }
    if (msg){
        msg->lkv = NULL;
        free(msg);
    }
    return false;    
}

static inline int compare_channel(const void *a, const void *b)
{
    // __xlogd("---------------------compare_channel a=%lu b=%lu\n", ((xchannel_ptr)a)->lcid.index, ((xchannel_ptr)b)->lcid.index);
    // __xlogd("---------------------compare_channel %d\n", ((xchannel_ptr)a)->lcid.index - ((xchannel_ptr)b)->lcid.index);
	return ((xchannel_ptr)a)->lcid.index - ((xchannel_ptr)b)->lcid.index;
}

static inline int compare_find_channel(const void *a, const void *b)
{
    // __xlogd("---------------------compare_find_channel a=%X b=%X\n", ((channelid_t*)(a))->index, (((xchannel_ptr)b)->lcid.index));
    // __xlogd("---------------------compare_find_channel %d\n", (((channelid_t*)(a))->index) - (((xchannel_ptr)b)->lcid.index));
	return (*((uint64_t*)(a)) - ((xchannel_ptr)b)->lcid.index);
}

xmsger_ptr xmsger_create(xmsgercb_ptr callback)
{
    __xlogd("xmsger_create enter\n");

    __xbreak(callback == NULL);

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));

    msger->sock = __xapi->udp_open(PACK_ONLINE_SIZE);
    __xbreak(msger->sock < 0);
    __xbreak(!__xapi->udp_host_to_addr(NULL, 9256, &msger->addr));
    __xbreak(__xapi->udp_bind(msger->sock, &msger->addr) == -1);

    msger->running = true;
    msger->callback = callback;
    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->send_list.len = 0;
    msger->send_list.head.prev = &msger->send_list.head;
    msger->send_list.head.next = &msger->send_list.head;

    msger->recv_list.len = 0;
    msger->recv_list.head.prev = &msger->recv_list.head;
    msger->recv_list.head.next = &msger->recv_list.head;

    msger->chcache = xtree_create();
    __xbreak(msger->chcache == NULL);
    avl_tree_init(&msger->peers, compare_channel, compare_find_channel, sizeof(struct xchannel), AVL_OFFSET(struct xchannel, node));

    msger->mpipe = xpipe_create(sizeof(void*) * 1024, "SEND PIPE");
    __xbreak(msger->mpipe == NULL);

    msger->mpid = __xapi->process_create(main_loop, msger);
    __xbreak(msger->mpid == NULL);

    __xlogd("xmsger_create exit\n");

    return msger;

Clean:

    if (msger->sock >= 0){
        __xapi->udp_close(msger->sock);
    }
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

        if (msger->mpipe){
            __xlogd("xmsger_free break mpipe\n");
            xpipe_break(msger->mpipe);
        }

        if (msger->mpid){
            __xlogd("xmsger_free main process\n");
            __xapi->process_free(msger->mpid);
        }

        // channel free 会用到 rpipe，所以将 tree clear 提前
        avl_tree_clear(&msger->peers, free_channel);
        if (msger->chcache){
            // __xlogd("xmsger_free clear channel cache\n");
            // xtree_clear(msger->chcache, free_channel);
            // __xlogd("xmsger_free channel cache\n");
            xtree_free(&msger->chcache);
        }

        if (msger->mpipe){
            __xlogd("xmsger_free msg pipe: %lu\n", xpipe_readable(msger->mpipe));
            while (xpipe_readable(msger->mpipe) > 0){
                xmsg_ptr msg;
                xpipe_read(msger->mpipe, &msg, __sizeof_ptr);
                if (msg){
                    xmsg_free(msg);
                }
            }
            xpipe_free(&msger->mpipe);
        }

        if (msger->sock >= 0){
            __xapi->udp_close(msger->sock);
        }

        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}

bool xchannel_get_keepalive(xchannel_ptr channel)
{
    return channel->keepalive;
}

const char* xchannel_get_ip(xchannel_ptr channel)
{
    if (channel->port == 0){
        __xapi->udp_addr_to_host(&channel->addr, channel->ip, &channel->port);
    }
    return channel->ip;
}

uint16_t xchannel_get_port(xchannel_ptr channel)
{
    if (channel->port == 0){
        __xapi->udp_addr_to_host(&channel->addr, channel->ip, &channel->port);
    }
    return channel->port;
}

struct xchannel_ctx* xchannel_get_ctx(xchannel_ptr channel)
{
    return channel->ctx;
}

void xchannel_set_ctx(xchannel_ptr channel, struct xchannel_ctx *ctx)
{
    channel->ctx = ctx;
}