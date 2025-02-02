#include "xmsger.h"

#include "xtree.h"
#include "xpipe.h"

#include "xlib/avlmini.h"

#define XHEAD_SIZE          64
#define XBODY_SIZE          1280 // 1024 + 256
#define XPACK_SIZE          ( XHEAD_SIZE + XBODY_SIZE ) // 1344

#define XPACK_SERIAL_RANGE  64
#define XPACK_SEND_RATE     100000UL // 1 毫秒

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAX_LENGTH             ( XBODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_TIMEDOUT_LIMIT         10
#define XCHANNEL_RTT_TIMEDOUT_COUNTS    1.5
#define XCHANNEL_RTT_SAMPLING_COUNTS    256

typedef struct xhead {
    uint16_t type; // 包类型
    uint16_t sn; // 序列号
    uint16_t resend; // 重传计数
    struct{
        uint16_t type; // 标志着是否附带了 ACK 和 ACK 确认的包类型
        uint16_t sn; // 确认收到了序列号为 sn 的包
        uint16_t pos; // 确认收到了 pos 之前的所有包
    }ack;
    uint16_t cid; // 通道 ID
    uint16_t len; // 当前分包的数据长度
    uint16_t range; // 一个消息的分包数量，从 1 到 range
    uint8_t flags[14]; // 扩展标志位
    uint8_t secret[32]; // 密钥
}*xhead_ptr;

// typedef struct xmsg {
//     void *ctx;
//     uint32_t flag;
//     uint32_t range;
//     uint64_t spos;
//     xline_t *xl;
// }xmsg_t;

typedef struct xpack {
    uint64_t ts;
    uint64_t timedout;
    xline_t *msg;
    xchannel_ptr channel;
    struct xpack *prev, *next;
    struct xhead head;
    uint8_t body[XBODY_SIZE];
}*xpack_ptr;

struct xpacklist {
    uint16_t len;
    struct xpack head;
};

typedef struct serialbuf {
    uint16_t range, spos, rpos, wpos;
    struct xpack *buf[1];
}*serialbuf_ptr;

typedef struct sserialbuf {
    uint16_t range, spos, rpos, wpos;
    struct xpack buf[1];
}*sserialbuf_ptr;

typedef struct xmsgbuf {
    uint16_t range, spos, rpos, wpos;
    xline_t *buf[1];
}*xmsgbuf_ptr;


struct xchannel {

    struct avl_node node;

    int sock;

    uint16_t serial_range;
    uint16_t threshold;
    uint16_t resend_counter;

    uint64_t psf; // packet sending frequency
    uint64_t rtt; // round-trip times
    uint16_t rtt_counter; 
    uint64_t rtt_duration;

    uint64_t send_ts, recv_ts, ack_ts;

    void *ctx;
    // uint64_t rid;
    uint16_t cid;
    uint64_t ucid[3];
    __xipaddr_ptr addr;

    // bool keepalive;
    // bool connected;
    // bool disconnected;
    bool timedout;
    // __atom_bool disconnecting;
    __atom_size spos, rpos, wpos;

    xline_t *msg;
    xline_t *req;
    xmsger_ptr msger;
    struct xhead ack;
    xmsgbuf_ptr msgbuf;
    serialbuf_ptr recvbuf;
    sserialbuf_ptr sendbuf;
    // struct xpacklist flushlist;

    xchannel_ptr prev, next;
};

typedef struct xchannellist {
    size_t len;
    struct xchannel head;
}*xchannellist_ptr;

struct xmsger {
    int sock[3];
    uint16_t port, lport;
    __xipaddr_ptr addr, laddr;
    __atom_bool running;
    __atom_size pos, len;
    uint16_t cid;
    uint64_t timer;
    xmsgercb_ptr cb;
    __xthread_ptr tid;
    struct avl_tree peers;
    struct xchannellist sendlist;
};

#define __serialbuf_wpos(b)         ((b)->wpos & ((b)->range - 1))
#define __serialbuf_rpos(b)         ((b)->rpos & ((b)->range - 1))
#define __serialbuf_spos(b)         ((b)->spos & ((b)->range - 1))

#define __serialbuf_recvable(b)     ((uint16_t)((b)->spos - (b)->rpos))
#define __serialbuf_sendable(b)     ((uint16_t)((b)->wpos - (b)->spos))

#define __serialbuf_readable(b)     ((uint16_t)((b)->wpos - (b)->rpos))
#define __serialbuf_writable(b)     ((uint16_t)((b)->range - (b)->wpos + (b)->rpos))

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


static inline int xmsg_fixed(xline_t *msg)
{
    __xcheck(msg == NULL);
    msg->rpos = msg->spos = 0;
    if (msg->wpos > 0){
        msg->range = (msg->wpos / XBODY_SIZE);
        if (msg->range * XBODY_SIZE < msg->wpos){
            // 有余数，增加一个包
            msg->range ++;
        }
    }else {
        msg->range = 1;
    }
    return 0;
XClean:
    return -1;
}


static inline xchannel_ptr xchannel_create(xmsger_ptr msger, uint16_t serial_range)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xcheck(channel == NULL);

    channel->msger = msger;
    channel->serial_range = serial_range;
    channel->threshold = 64;
    channel->send_ts = channel->recv_ts = __xapi->clock();
    channel->rtt = 80000000UL;
    channel->psf = 10000UL;

    // channel->rid = XNONE;

    channel->recvbuf = (serialbuf_ptr) calloc(1, sizeof(struct serialbuf) + sizeof(xpack_ptr) * channel->serial_range);
    __xcheck(channel->recvbuf == NULL);
    channel->recvbuf->range = channel->serial_range;

    channel->sendbuf = (sserialbuf_ptr) calloc(1, sizeof(struct sserialbuf) + sizeof(struct xpack) * channel->serial_range);
    __xcheck(channel->sendbuf == NULL);
    channel->sendbuf->range = channel->serial_range;
    channel->sendbuf->rpos = channel->sendbuf->spos = channel->sendbuf->wpos = 0;

    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xline_t*) * 4);
    __xcheck(channel->msgbuf == NULL);
    channel->msgbuf->range = 4;

    // channel->flushlist.len = 0;
    // channel->flushlist.head.prev = &channel->flushlist.head;
    // channel->flushlist.head.next = &channel->flushlist.head;

    __ring_list_put_into_head(&msger->sendlist, channel);

    return channel;

XClean:

    if (channel){
        if (channel->recvbuf){
            free(channel->recvbuf);
        }
        if (channel->sendbuf){
            free(channel->sendbuf);
        }
        if (channel->msgbuf){
            free(channel->msgbuf);
        }
        free(channel);
    }
    return NULL;
}

static inline void xchannel_clear(xchannel_ptr channel)
{
    __xlogd("xchannel_clear enter\n");

    // // 清空冲洗队列
    // channel->flushlist.len = 0;
    // channel->flushlist.head.prev = &channel->flushlist.head;
    // channel->flushlist.head.next = &channel->flushlist.head;
    // 刷新发送队列
    while(__serialbuf_sendable(channel->sendbuf) > 0){
        __xlogd("xchannel_clear pack send buf\n");
        channel->sendbuf->spos++;
    }
    channel->sendbuf->rpos = channel->sendbuf->spos;

    // 缓冲区里的包有可能不是连续的，所以不能顺序的清理，如果使用 rpos 就会出现内存泄漏的 BUG
    for (int i = 0; i < channel->recvbuf->range; ++i){
        if (channel->recvbuf->buf[i] != NULL){
            __xlogd("xchannel_clear pack recv buf\n");
            free(channel->recvbuf->buf[i]);
            // 这里要置空，否则重复调用这个函数，会导致崩溃
            channel->recvbuf->buf[i] = NULL;
        }
    }

    // 清理没有发送的消息
    for (int i = 0; i < channel->msgbuf->range; ++i){
        if (channel->msgbuf->buf[i] != NULL){
            __xlogd("xchannel_clear msg send buf\n");
            // 减掉发送缓冲区的数据
            channel->msger->len -= (channel->msgbuf->buf[i]->wpos - channel->msgbuf->buf[i]->rpos);
            // xl 加了引用计数，这里需要释放一次
            xl_free(&channel->msgbuf->buf[i]);
            // channel->msgbuf->buf[i] = NULL;
        }
    }

    // 释放未完整接收的消息
    if (channel->msg != NULL){
        __xlogd("xchannel_clear msg recv buf\n");
        xl_free(&channel->msg);
        // channel->msg = NULL;
    }

    if (channel->req != NULL){
        __xlogd("xchannel_clear req\n");
        xl_free(&channel->req);
    }
    
    __xlogd("xchannel_clear exit\n");
}

static void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free >>>>-------------------> CID[%u]\n", channel->cid);
    xchannel_clear(channel);
    __ring_list_take_out(&channel->msger->sendlist, channel);
    if (channel->node.parent != &channel->node){
        avl_tree_remove(&channel->msger->peers, channel);
    }
    __xlogd("xchannel_free >>>>-------------------> 1\n");
    free(channel->recvbuf);
    __xlogd("xchannel_free >>>>-------------------> 2\n");
    free(channel->sendbuf);
    __xlogd("xchannel_free >>>>-------------------> 3\n");
    free(channel->msgbuf);
    __xlogd("xchannel_free >>>>-------------------> 4\n");
    free(channel->addr);
    __xlogd("xchannel_free >>>>-------------------> 5\n");
    free(channel);
    __xlogd("xchannel_free >>>>-------------------> exit\n");
}

// static inline void xchannel_serial_pack(xchannel_ptr channel, uint8_t type)
// {
//     xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
//     pack->head.flags[0] = channel->serial_range;
//     pack->msg = NULL;
//     pack->head.type = type;
//     pack->head.range = 1;
//     pack->channel = channel;
//     pack->delay = channel->back_delay;
//     pack->head.resend = 0;
//     pack->head.ack.type = 0;
//     pack->head.cid = channel->cid;
//     pack->head.sn = channel->sendbuf->wpos;
//     __atom_add(channel->sendbuf->wpos, 1);
//     channel->len += pack->head.len;
//     channel->msger->len += pack->head.len;
// }

static inline void xchannel_serial_msg(xchannel_ptr channel)
{
    // 每次只缓冲一个包，尽量使发送速度均匀
    if (__serialbuf_writable(channel->sendbuf) > 0 && __serialbuf_sendable(channel->msgbuf) > 0){

        xline_t *msg = channel->msgbuf->buf[__serialbuf_spos(channel->msgbuf)];
        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];

        pack->msg = msg;
        pack->head.type = msg->type;
        // pack->head.flags[0] = channel->serial_range;
        if (msg->wpos > 0){
            if (msg->wpos - msg->spos < XBODY_SIZE){
                pack->head.len = msg->wpos - msg->spos;
            }else{
                pack->head.len = XBODY_SIZE;
            }
        }else {
            pack->head.len = 0;
        }
        pack->head.range = msg->range;
        mcopy(pack->body, msg->ptr + msg->spos, pack->head.len);
        msg->spos += pack->head.len;
        // __xlogi("====================== debug msg pack len = %u msg wpos = %lu spos = %lu range = %u\n", pack->head.len, msg->wpos, msg->spos, msg->range);
        msg->range--;

        pack->channel = channel;
        // TODO ?
        pack->ts = channel->rtt;
        pack->head.resend = 0;
        pack->head.ack.type = 0;
        pack->head.ack.pos = channel->serial_range; // 只有每个连接的第一个包会用到
        pack->head.cid = channel->cid;
        pack->head.sn = channel->sendbuf->wpos;
        __atom_add(channel->sendbuf->wpos, 1);
        // 判断消息是否全部写入缓冲区
        if (msg->spos == msg->wpos){
            __atom_add(channel->msgbuf->spos, 1);
        }
    }
}

static inline void xchannel_send_pack(xchannel_ptr channel)
{
    if (__serialbuf_sendable(channel->sendbuf) == 0){
        xchannel_serial_msg(channel);
    }
    
    if (__serialbuf_sendable(channel->sendbuf) > 0){

        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_spos(channel->sendbuf)];

        __xlogd("<SEND> TYPE[%u] IP[%s] PORT[%u] CID[%u] PSF[%lu] ACK[%u:%u:%u] >>>>------> SN[%u]\n", 
            pack->head.type, __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, channel->psf,
            pack->head.ack.type, pack->head.ack.sn, pack->head.ack.pos, pack->head.sn);

        if (channel->ack.ack.type != 0){
            // 携带 ACK
            pack->head.ack.type = channel->ack.ack.type;
            pack->head.ack.sn = channel->ack.ack.sn;
            pack->head.ack.pos = channel->ack.ack.pos;
        }

        // 判断发送是否成功
        if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(pack->head), XHEAD_SIZE + pack->head.len) == XHEAD_SIZE + pack->head.len){

            // 发送成功才能清空标志
            channel->ack.ack.type = 0;

            // 缓冲区下标指向下一个待发送 pack
            __atom_add(channel->sendbuf->spos, 1);

            // 记录发送次数
            pack->head.resend = 1;

            // 记录当前时间
            channel->send_ts = __xapi->clock();
            pack->ts = channel->send_ts;
            pack->timedout = channel->rtt * 2;

            channel->spos += pack->head.len;

            // 如果有待发送数据，确保 sendable 会大于 0
            xchannel_serial_msg(channel);

        }else {

            __xlogd("xchannel_send_pack >>>>------------------------> SEND FAILED\n");
        }

    }
}

static inline void xchannel_send_ack(xchannel_ptr channel)
{
    __xlogd("<SEND> TYPE(0) IP[%s] PORT[%u] CID[%u] ACK[%u:%u:%u]\n", 
        __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, 
        channel->ack.ack.type, channel->ack.ack.sn, channel->ack.ack.pos);

    if ((__xapi->udp_sendto(channel->sock, channel->addr, (void*)&channel->ack, XHEAD_SIZE)) == XHEAD_SIZE){
        channel->ack.ack.type = 0;
    }else {
        __xlogd("xchannel_send_ack >>>>------------------------> SEND FAILED\n");
    }
}

static inline void xchannel_send_final(int sock, __xipaddr_ptr addr, xpack_ptr pack)
{
    __xlogd("<SEND FINAL> IP[%s] PORT[%u] CID[%u] ACK[%u:%u:%u]\n", 
        __xapi->udp_addr_ip(addr), __xapi->udp_addr_port(addr), pack->head.cid, 
        pack->head.ack.type, pack->head.ack.sn, pack->head.ack.pos);

    // 设置 flag
    pack->head.type = XPACK_TYPE_ACK;
    pack->head.ack.type = XPACK_TYPE_RES;
    // 设置 acks，通知发送端已经接收了所有包
    pack->head.ack.pos = pack->head.sn + 1;
    pack->head.ack.sn = pack->head.ack.pos;
    // 要设置包长度，对方要校验长度
    pack->head.len = 0;
    pack->head.range = 1;
    if (__xapi->udp_sendto(sock, addr, (void*)&pack->head, XHEAD_SIZE) != XHEAD_SIZE){
        __xlogd("xchannel_send_final >>>>------------------------> SEND FAILED\n");
    }
}

static inline int xchannel_recv_msg(xchannel_ptr channel)
{
    // 更新时间戳
    channel->recv_ts = __xapi->clock();

    if (__serialbuf_readable(channel->recvbuf) > 0){
        // 索引已接收的包
        xpack_ptr pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        do {

            // 如果当前接收缓冲区为空，证明这个包是新消息的第一个包
            if (channel->msg == NULL){
                channel->msg = xl_creator(pack->head.range * XBODY_SIZE);
                __xcheck(channel->msg == NULL);
                channel->msg->type = pack->head.type;
                channel->msg->range = pack->head.range;
                // __xlogi("====================== debug msg range = %lu\n", channel->msg->range);
            }
            if (pack->head.len > 0){
                mcopy(channel->msg->ptr + channel->msg->wpos, pack->body, pack->head.len);
                channel->msg->wpos += pack->head.len;
            }
            // __xlogi("====================== debug msg packet len = %u msg range = %lu size = %lu wpos = %lu\n", pack->head.len, channel->msg->range, channel->msg->size, channel->msg->wpos);
            // 收到了一个完整的消息
            if (--channel->msg->range == 0 /*|| channel->msg->size - channel->msg->wpos < XBODY_SIZE*/){
                // 更新消息长度
                xl_fixed(channel->msg);
                // 通知用户已收到一个完整的消息
                // __xmsg_set_ctx(channel->msg, channel->ctx);
                __xmsg_set_ipaddr(channel->msg, channel->addr);
                // xl_printf(&channel->msg->line);
                channel->msger->cb->on_message_from_peer(channel->msger->cb, channel, channel->msg);
                channel->msg = NULL;
                // // 更新时间戳
                // channel->timestamp = __xapi->clock();
            }
            // 处理过的缓冲区置空
            channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)] = NULL;
            // 更新读索引
            channel->recvbuf->rpos++;
            // 释放资源
            free(pack);
            // 索引下一个缓冲区
            pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        }while (pack != NULL); // 所有已接收的包被处理完之后，接收缓冲区为空
    }

    return 0;
XClean:
    return -1;
}

static inline void xchannel_recv_ack(xchannel_ptr channel, xpack_ptr rpack)
{
    // __xlogd("xchannel_recv_ack >>>>-----------> ack[%u:%u:%u] rpos=%u spos=%u ack-rpos=%u spos-rpos=%u\n",
    //         rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos, channel->sendbuf->rpos, channel->sendbuf->spos, 
    //         (uint16_t)(rpack->head.ack.sn - channel->sendbuf->rpos), (uint16_t)(channel->sendbuf->spos - channel->sendbuf->rpos));

    // 只处理 sn 在 rpos 与 spos 之间的 xpack
    if (__serialbuf_recvable(channel->sendbuf) > 0 
        && ((uint16_t)(rpack->head.ack.sn - channel->sendbuf->rpos) 
        <= (uint16_t)(channel->sendbuf->spos - channel->sendbuf->rpos))){

        xpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        // 如果没有丢包，第一个顺序到达的 acks 一定等于 ack，但是如果这个包丢了，acks 就会不等于 ack
        // 所以每次都要检测 rpos 是否等于 asks

        uint16_t index = __serialbuf_rpos(channel->sendbuf);

        channel->recv_ts = channel->ack_ts = __xapi->clock();

        // 这里曾经使用 do while 方式，造成了收到重复的 ACK，导致 rpos 越界的 BUG
        // 连续的 acks 必须至少比 rpos 大 1
        while ((uint16_t)(rpack->head.ack.pos - channel->sendbuf->rpos) 
                <= (uint16_t)(channel->sendbuf->spos - channel->sendbuf->rpos) // 防止后到的 acks 比之前的 acks 和 rpos 小，导致 rpos 越界
                && channel->sendbuf->rpos != rpack->head.ack.pos) {

            pack = &channel->sendbuf->buf[index];

            if (pack->ts != 0){
                // 累计新的一次往返时长
                channel->rtt_duration += channel->ack_ts - pack->ts;
                pack->ts = 0;
                if (channel->rtt_counter < XCHANNEL_RTT_SAMPLING_COUNTS){
                    // 更新累计次数
                    channel->rtt_counter++;
                    // 重新计算平均时长
                    channel->rtt = channel->rtt_duration / channel->rtt_counter;
                }else {
                    // 已经到达累计次数，需要减掉一次平均时长
                    channel->rtt_duration -= channel->rtt;
                    // 重新计算平均时长
                    channel->rtt = channel->rtt_duration >> 8;
                }
            }

            // 数据已发送，从待发送数据中减掉这部分长度
            __atom_add(channel->msger->pos, pack->head.len);
            __atom_add(channel->rpos, pack->head.len);
            if (channel->rpos == channel->wpos){
                channel->ack_ts = 0;
            }

            // 更新已经到达对端的数据计数
            pack->msg->rpos += pack->head.len;
            if (pack->msg->rpos == pack->msg->wpos){
                // 把已经传送到对端的 msg 交给发送线程处理
                channel->msger->cb->on_message_to_peer(channel->msger->cb, channel, pack->msg);
                channel->msgbuf->buf[__serialbuf_rpos(channel->msgbuf)] = NULL;
                __atom_add(channel->msgbuf->rpos, 1);
                pack->msg = NULL;
            }

            __atom_add(channel->sendbuf->rpos, 1);

            // 更新索引
            index = __serialbuf_rpos(channel->sendbuf);

            // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
        }

        if (rpack->head.ack.sn != rpack->head.ack.pos){

            // __xlogd("RECV ACK OUT OF OEDER >>>>-----------> IP[%s] PORT[%u] CID[%u] ACK[%u:%u:%u]\n", 
            //         channel->ip, channel->port, channel->rcid, 
            //         rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos);

            pack = &channel->sendbuf->buf[rpack->head.ack.sn & (channel->sendbuf->range - 1)];

            if (pack->ts != 0){

                // 累计新的一次往返时长
                channel->rtt_duration += channel->ack_ts - pack->ts;
                pack->ts = 0;
                if (channel->rtt_counter < XCHANNEL_RTT_SAMPLING_COUNTS){
                    // 更新累计次数
                    channel->rtt_counter++;
                    // 重新计算平均时长
                    channel->rtt = channel->rtt_duration / channel->rtt_counter;
                }else {
                    // 已经到达累计次数，需要减掉一次平均时长
                    channel->rtt_duration -= channel->rtt;
                    // 重新计算平均时长
                    channel->rtt = channel->rtt_duration >> 8;
                }
            }

            // ack 与 rpos 的间隔大于一才进行重传
            if (((uint16_t)(rpack->head.ack.sn - channel->sendbuf->rpos)) > 2){
                // 使用临时变量
                uint16_t index = channel->sendbuf->rpos;
                // 实时重传 rpos 到 SN 之间的所有尚未确认的 SN
                while (index != rpack->head.ack.sn)
                {
                    // 取出落后的包
                    pack = &channel->sendbuf->buf[(index & (channel->sendbuf->range - 1))];
                    // 判断这个包是否已经接收过
                    if (pack->ts != 0){
                        // 判断是否进行了重传
                        if (pack->head.resend < 2){
                            pack->head.resend++;
                            // 判断重传的包是否带有 ACK
                            if (pack->head.ack.type != 0){
                                // 更新 ACKS
                                // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                pack->head.ack.pos = channel->recvbuf->wpos;
                            }

                            if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(pack->head), XHEAD_SIZE + pack->head.len) == XHEAD_SIZE + pack->head.len){
                                pack->timedout *= XCHANNEL_RTT_TIMEDOUT_COUNTS;
                                __xlogd("<RESEND> TYPE[%u] IP[%s] PORT[%u] CID[%u] DELAY[%lu] ACK[%u:%u:%u] >>>>-----> SN[%u]\n", 
                                        pack->head.type, __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, pack->timedout / 1000000UL,
                                        pack->head.ack.type, pack->head.ack.sn, pack->head.ack.pos, pack->head.sn);
                                break; // 每次只重传一个包
                            }else {
                                __xlogd("xchannel_recv_ack >>>>------------------------> SEND FAILED\n");
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

static inline int xchannel_recv_pack(xchannel_ptr channel, xpack_ptr *rpack)
{
    xpack_ptr pack = *rpack;

    channel->ack.type = XPACK_TYPE_ACK;
    channel->ack.ack.type = pack->head.type;

    uint16_t index = pack->head.sn & (channel->recvbuf->range - 1);

    // 如果收到连续的 PACK
    if (pack->head.sn == channel->recvbuf->wpos){        

        pack->channel = channel;
        // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
        if (pack->head.ack.type != 0){
            // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
            xchannel_recv_ack(channel, pack);
        }
        // 保存 PACK
        channel->recvbuf->buf[index] = pack;
        *rpack = NULL;
        // 更新最大连续 SN
        channel->recvbuf->wpos++;

        // 收到连续的 ACK 就不会回复的单个 ACK 确认了
        channel->ack.ack.pos = channel->recvbuf->wpos;
        // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
        channel->ack.ack.sn = channel->ack.ack.pos;

        // 如果提前到达的 PACK 需要更新
        while (channel->recvbuf->buf[__serialbuf_wpos(channel->recvbuf)] != NULL 
                // 判断缓冲区是否正好填满，避免首位相连造成死循环的 BUG
                && __serialbuf_writable(channel->recvbuf) > 0)
        {
            channel->recvbuf->wpos++;
            // 这里需要更新将要回复的最大连续 ACK
            channel->ack.ack.pos = channel->recvbuf->wpos;
            // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
            channel->ack.ack.sn = channel->ack.ack.pos;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint16_t)(channel->recvbuf->wpos - pack->head.sn) > (uint16_t)(pack->head.sn - channel->recvbuf->wpos)){

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack.sn = pack->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.ack.pos = channel->recvbuf->wpos;

            __xlogd("RECV EARLY >>>>--------> IP=[%s] PORT=[%u] CID[%u] FLAG[%u] ACK[%u:%u:%u]\n", 
                    __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, pack->head.type,
                    channel->ack.ack.type, channel->ack.ack.sn, channel->ack.ack.pos);

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->recvbuf->wpos - 1;
            
            if (channel->recvbuf->buf[index] == NULL){
                pack->channel = channel;
                // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
                if (pack->head.ack.type != 0){
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

            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.ack.pos = channel->recvbuf->wpos;
            channel->ack.ack.sn = channel->ack.ack.pos;
            // 重复到达的 PACK
            __xlogd("RECV AGAIN >>>>--------> IP=[%s] PORT=[%u] CID[%u] FLAG[%u] ACK[%u:%u:%u]\n", 
                    __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, pack->head.type,
                    channel->ack.ack.type, channel->ack.ack.sn, channel->ack.ack.pos);
        }
    }

    return xchannel_recv_msg(channel);
}


static inline int xmsger_local_recv(xmsger_ptr msger, xhead_ptr head)
{
    xline_t *msg = (xline_t*)(*(uint64_t*)&head->flags[0]);

    if (head->ack.type == XPACK_TYPE_REQ){

        xchannel_ptr channel = xchannel_create(msger, XPACK_SERIAL_RANGE);
        __xcheck(channel == NULL);

        __xcheck(xmsg_fixed(msg) != 0);

        // channel->rid = msg->id;
        // channel->ctx = __xmsg_get_ctx(msg);
        xl_hold(msg);
        channel->req = msg;
        channel->addr = __xmsg_get_ipaddr(msg);

        if (__xapi->udp_addr_is_ipv6(channel->addr)){
            channel->sock = channel->msger->sock[1];
        }else {
            channel->sock = channel->msger->sock[0];
        }

        channel->ucid[0] = ((uint64_t*)channel->addr)[0];
        channel->ucid[1] = ((uint64_t*)channel->addr)[1];
        channel->ucid[2] = ((uint64_t*)channel->addr)[2];
        do {
            channel->cid = msger->cid++;
            *(uint16_t*)&channel->ucid[0] = channel->cid;
        }while (avl_tree_add(&msger->peers, channel) != NULL);

        channel->ack.cid = channel->cid;
        // channel->keepalive = true;

        // __xcheck(msg->wpos > XBODY_SIZE);
        __atom_add(channel->msger->len, msg->wpos);
        __atom_add(channel->wpos, msg->wpos);
        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);

        __xlogd("<CONNECT> IP=[%s] PORT=[%u] CID[%u]\n", 
                __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid);

    }else if (head->ack.type == XPACK_TYPE_RES){

        xchannel_ptr channel = __xmsg_get_channel(msg);
        __xcheck(channel == NULL);
        __xcheck(xmsg_fixed(msg) != 0);
        // __xcheck(msg->wpos > XBODY_SIZE);
        __atom_add(channel->msger->len, msg->wpos);
        __atom_add(channel->wpos, msg->wpos);
        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);
        __xlogd("<DISCONNECT> IP=[%s] PORT=[%u] CID[%u]\n", 
                __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid);

    }else if (head->ack.type == XPACK_TYPE_FLUSH){
        xchannel_ptr channel = (xchannel_ptr)(*(uint64_t*)&head->flags[8]);
        __xlogd("<FINAL> IP=[%s] PORT=[%u] CID[%u]\n", 
                __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid);
        xchannel_free(channel);
    }

    return 0;
XClean:
    return -1;
}

static inline int xmsger_send_all(xmsger_ptr msger)
{
    static int64_t delay;
    static xpack_ptr spack;
    static xchannel_ptr channel;
    
    // 判断待发送队列中是否有内容
    if (msger->sendlist.len > 0){

        // 从头开始，每个连接发送一个 pack
        channel = msger->sendlist.head.next;

        while (channel != &msger->sendlist.head)
        {
            uint64_t current_ts = __xapi->clock();

            // readable 是已经写入缓冲区还尚未发送的包
            if (channel->resend_counter > 0){
                channel->resend_counter--;
            }else {
                channel->threshold = __serialbuf_readable(channel->sendbuf);
                if (channel->threshold < channel->sendbuf->range){
                    if (channel->threshold < 10){
                        channel->psf = (channel->threshold + 1) * 1000UL; // 1 - 10 微妙
                    }else if (channel->threshold < 20){
                        channel->psf = (channel->threshold - 9 + 1) * 10000UL; // 20 - 110 微妙
                        // __xlogd("psf = %lu len = %u\n", channel->psf, channel->threshold);
                    }else {
                        channel->psf = (channel->threshold - 19 + 1) * 100000UL * (channel->threshold / 20); // 200 - 13800 微妙
                        // __xlogd("psf = %lu len = %u\n", channel->psf, channel->threshold);
                    }
                    delay = channel->psf - (current_ts - channel->send_ts);
                    if (delay > 0){
                        if (msger->timer > delay){
                            msger->timer = delay;
                        }
                    }else {
                        xchannel_send_pack(channel);
                    }
                }
            }

            if (__serialbuf_recvable(channel->sendbuf) > 0){

                current_ts = __xapi->clock();
                spack = &channel->sendbuf->buf[__serialbuf_rpos(channel->sendbuf)];

                if (channel->ack_ts > 0){
                    delay = (int64_t)(spack->timedout - (current_ts - channel->ack_ts));
                }else {
                    delay = (int64_t)(spack->timedout - (current_ts - spack->ts));
                }

                if (delay > 0) {
                    // 未超时
                    if (msger->timer > delay){
                        // 超时时间更近，更新休息时间
                        msger->timer = delay;
                    }
                    // 第一个包未超时，后面的包就都没有超时
                    break;

                }else {

                    if (current_ts - spack->ts > NANO_SECONDS * XCHANNEL_TIMEDOUT_LIMIT)
                    {
                        if (!channel->timedout){
                            channel->timedout = true;
                            __xlogd("SEND TIMEOUT >>>>-------------> IP(%s) PORT(%u) CID(%u) DELAY(%lu)\n", 
                                    __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, (current_ts - spack->ts) / 1000000UL);
                            msger->cb->on_message_timedout(msger->cb, channel, spack->msg);
                        }
                        break;

                    }else {

                        // 超时重传

                        // 判断重传的包是否带有 ACK
                        if (spack->head.ack.type != 0){
                            // 更新 ACKS
                            // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                            spack->head.ack.pos = channel->recvbuf->wpos;
                            // TODO ack 也要更新
                        }

                        // 判断发送是否成功
                        if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(spack->head), XHEAD_SIZE + spack->head.len) == XHEAD_SIZE + spack->head.len){
                            // 记录重传次数
                            spack->head.resend++;
                            channel->resend_counter++;
                            // 最后一个待确认包的超时时间加上平均往返时长
                            spack->timedout *= XCHANNEL_RTT_TIMEDOUT_COUNTS;
                            __xlogd("<RESEND> TYPE[%u] IP[%s] PORT[%u] CID[%u] ACK[%u:%u:%u] >>>>-----> SN[%u] RTT[%lu] DELAY[%lu]\n", 
                                    spack->head.type, __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, 
                                    spack->head.ack.type, spack->head.ack.sn, spack->head.ack.pos, spack->head.sn, 
                                    channel->rtt / 1000000UL, spack->timedout);
                        }else {
                            __xlogd(">>>>------------------------> SEND FAILED\n");
                        }
                    }
                }

            }else if (channel->rpos == channel->wpos){

                if (current_ts - channel->recv_ts > NANO_SECONDS * XCHANNEL_TIMEDOUT_LIMIT){
                    if (!channel->timedout){
                        channel->timedout = true;
                        // __set_true(channel->disconnecting);
                        __xlogd("RECV TIMEOUT >>>>-------------> IP(%s) PORT(%u) CID(%u) DELAY(%lu)\n", 
                                __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid, (current_ts - channel->recv_ts) / 1000000UL);
                        if (channel->req != NULL){
                            // 连接已经建立，需要回收资源
                            msger->cb->on_message_timedout(msger->cb, channel, channel->req);
                            channel->req = NULL;
                        }else {
                            // 连接尚未建立，直接释放即可
                            xchannel_free(channel);
                        }
                    }
                }
            }

            channel = channel->next;
        }
    }

    return 0;
XClean:
    return -1;
}


static void msger_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    xmsger_ptr msger = (xmsger_ptr)ptr;

    size_t sid;
    uint64_t cid[3] = {0};
    xchannel_ptr channel = NULL;
    __xipaddr_ptr addrs[2] = {0}, addr;
    addrs[0] =  __xapi->udp_any_to_addr(0, 9256);
    addrs[1] =  __xapi->udp_any_to_addr(1, 9256);
    
    xpack_ptr rpack = (xpack_ptr)malloc(sizeof(struct xpack));
    __xcheck(rpack == NULL);
    rpack->head.len = 0;

    msger->timer = 100000000UL; // 10 毫秒

    while (msger->running)
    {
    for (sid = 0; sid < 2; sid++)
    {
        addr = addrs[sid];
        ((uint64_t*)addr)[0] = ((uint64_t*)addr)[1] = ((uint64_t*)addr)[2] = 0; // ubuntu-20.04 需要清零
        while (__xapi->udp_recvfrom(msger->sock[sid], addr, &rpack->head, XPACK_SIZE) == (rpack->head.len + XHEAD_SIZE)){

            if (rpack->head.type == XPACK_TYPE_LOCAL){
                if (mcompare(addr, msger->laddr, 8) == 0){
                    xmsger_local_recv(msger, &rpack->head);
                }
                rpack->head.len = 0;
                continue;
            }

            __xlogd("[RECV] TYPE(%u) IP(%s) PORT(%u) CID(%u) ACK(%u:%u:%u) SN(%u)\n",
                    rpack->head.type, __xapi->udp_addr_ip(addr), __xapi->udp_addr_port(addr), rpack->head.cid, 
                    rpack->head.ack.type, rpack->head.ack.sn, rpack->head.ack.pos, rpack->head.sn);

            cid[0] = ((uint64_t*)addr)[0];
            cid[1] = ((uint64_t*)addr)[1];
            cid[2] = ((uint64_t*)addr)[2];
            *(uint16_t*)&cid[0] = rpack->head.cid;

            channel = avl_tree_find(&msger->peers, &cid[0]);

            if (channel){

                if (rpack->head.type == XPACK_TYPE_MSG) {

                    __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                    // if (__serialbuf_sendable(channel->sendbuf) > 0){
                    //     xchannel_send_pack(channel);
                    // }else 
                    {
                        xchannel_send_ack(channel);
                    }

                }else if (rpack->head.type == XPACK_TYPE_ACK){

                    xchannel_recv_ack(channel, rpack);

                }else if (rpack->head.type == XPACK_TYPE_RES){

                    // 收到完整的消息后会断开连接
                    __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                    xchannel_send_ack(channel);                    

                }else if (rpack->head.type == XPACK_TYPE_REQ){

                    // 检查是否为重传的包或者 serial range 是否一致
                    if (rpack->head.resend > 0 || rpack->head.ack.pos == channel->recvbuf->range){
                        // 同一个 REQ，回复同一个 ACK
                        __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                        xchannel_send_ack(channel);
                    }else {
                        // 不是同一个 REQ，不回应，让对方超时以后换一个 CID 再创建连接
                    }

                }else {
                    __xlogd("RECV UNKNOWN TYPE >>>>-------------> IP(%s) PORT(%u) CID(%u)\n", 
                            __xapi->udp_addr_ip(channel->addr), __xapi->udp_addr_port(channel->addr), channel->cid);
                }

            } else {

                // 收到对方发起的请求
                if (rpack->head.type == XPACK_TYPE_REQ){

                    // 创建连接
                    channel = xchannel_create(msger, rpack->head.ack.pos);
                    __xcheck(channel == NULL);
                    // 设置 cid
                    channel->cid = rpack->head.cid;;
                    channel->ack.cid = channel->cid;

                    channel->addr = __xapi->udp_addr_dump(addr);
                    channel->sock = channel->msger->sock[sid];

                    channel->ucid[0] = cid[0];
                    channel->ucid[1] = cid[1];
                    channel->ucid[2] = cid[2];
                    __xcheck(avl_tree_add(&msger->peers, channel) != NULL);

                    // 更新接收缓冲区和 ACK
                    __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                    xchannel_send_ack(channel);

                }else if (rpack->head.type == XPACK_TYPE_RES){

                    // 被动端收到重复的 BYE，回复最后的 ACK
                    xchannel_send_final(msger->sock[sid], addr, rpack);
                }
            }

            if (rpack == NULL){
                rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                __xcheck(rpack == NULL);
            }
            rpack->head.len = 0;
            __xcheck(xmsger_send_all(msger) != 0);
        }
        }

        __xapi->udp_listen(msger->sock, msger->timer / 1000);
        msger->timer = 10000000UL; // 10 毫秒
        __xcheck(xmsger_send_all(msger) != 0);
    }


XClean:


    if (rpack != NULL){
        free(rpack);
    }

    if (addrs[0]){
        free(addrs[0]);
    }

    if (addrs[1]){
        free(addrs[1]);
    }
    __xlogd("xmsger_loop exit\n");
}

int xmsger_send(xmsger_ptr msger, xchannel_ptr channel, xline_t *msg)
{
    __xcheck(msger == NULL);
    __xcheck(channel == NULL);
    __xcheck(msg == NULL);
    if (__serialbuf_writable(channel->msgbuf) > 0){
        // msg->type = XPACK_TYPE_BIN;
        __xcheck(xmsg_fixed(msg) != 0);
        __atom_add(msger->len, msg->wpos);
        __atom_add(channel->wpos, msg->wpos);
        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);
        return 0;
    }
XClean:
    return -1;
}

int xmsger_flush(xmsger_ptr msger, xchannel_ptr channel)
{
    __xcheck(msger == NULL);
    __xcheck(channel == NULL);
    // 状态错误会报错
    // __xcheck(!__set_false(channel->disconnecting));
    struct xhead pack;
    pack.type = XPACK_TYPE_LOCAL;
    pack.ack.type = XPACK_TYPE_FLUSH;
    pack.len = 0;
    *((uint64_t*)(&pack.flags[8])) = (uint64_t)channel;
    __xcheck(__xapi->udp_local_send(msger->sock[2], msger->addr, &pack, XHEAD_SIZE) != XHEAD_SIZE);
    return 0;
XClean:
    return -1;
}

int xmsger_disconnect(xmsger_ptr msger, xline_t *msg)
{
    __xcheck(msger == NULL);
    __xcheck(msg == NULL);
    struct xhead pack;
    pack.type = XPACK_TYPE_LOCAL;
    pack.ack.type = msg->type;
    pack.len = 0;
    *((uint64_t*)(&pack.flags[0])) = (uint64_t)msg;
    __xcheck(__xapi->udp_local_send(msger->sock[2], msger->addr, &pack, XHEAD_SIZE) != XHEAD_SIZE);
    return 0;
XClean:
    return -1;

}

int xmsger_connect(xmsger_ptr msger, xline_t *msg)
{
    __xcheck(msger == NULL);
    __xcheck(msg == NULL);
    struct xhead pack;
    pack.type = XPACK_TYPE_LOCAL;
    pack.ack.type = msg->type;
    pack.len = 0;
    *((uint64_t*)(&pack.flags[0])) = (uint64_t)msg;
    __xcheck(__xapi->udp_local_send(msger->sock[2], msger->addr, &pack, XHEAD_SIZE) != XHEAD_SIZE);
    return 0;
XClean:
    return -1;
}

static inline int unicid_comp(const void *a, const void *b)
{
    uint64_t *x = &((xchannel_ptr)a)->ucid[0];
    uint64_t *y = &((xchannel_ptr)b)->ucid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline int unicid_find(const void *a, const void *b)
{
    uint64_t *x = (uint64_t*)a;
    uint64_t *y = &((xchannel_ptr)b)->ucid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

xmsger_ptr xmsger_create(xmsgercb_ptr callback, uint16_t port)
{
    __xlogd("xmsger_create enter\n");

    __xcheck(callback == NULL);

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));

    msger->port = port;
    msger->sock[0] = __xapi->udp_open(0, 1, 1);
    __xcheck(msger->sock[0] < 0);
    while (__xapi->udp_bind(msger->sock[0], msger->port) == -1){
        // 指定的端口在客户端上有可能被占用
        msger->port++;
    }
    
    msger->sock[1] = __xapi->udp_open(1, 1, 1);
    __xcheck(msger->sock[1] < 0);
    __xcheck(__xapi->udp_bind(msger->sock[1], msger->port) == -1);

    msger->sock[2] = __xapi->udp_open(0, 0, 0);
    __xcheck(msger->sock[2] < 0);
    msger->lport = msger->port+1;
    while (__xapi->udp_bind(msger->sock[2], msger->lport) == -1){
        msger->lport++;
    }
    msger->addr = __xapi->udp_host_to_addr("127.0.0.1", msger->port);
    __xcheck(msger->addr == NULL);
    msger->laddr = __xapi->udp_host_to_addr("127.0.0.1", msger->lport);
    __xcheck(msger->laddr == NULL);
    
    msger->running = true;
    msger->cb = callback;
    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->sendlist.len = 0;
    msger->sendlist.head.prev = &msger->sendlist.head;
    msger->sendlist.head.next = &msger->sendlist.head;

    avl_tree_init(&msger->peers, unicid_comp, unicid_find, sizeof(struct xchannel), AVL_OFFSET(struct xchannel, node));

    msger->tid = __xapi->thread_create(msger_loop, msger);
    __xcheck(msger->tid == NULL);

    __xlogd("xmsger_create exit\n");

    return msger;
XClean:
    xmsger_free(&msger);
    return NULL;
}

void xmsger_free(xmsger_ptr *pptr)
{
    __xlogd("xmsger_free enter\n");

    if (pptr && *pptr){

        xmsger_ptr msger = *pptr;
        *pptr = NULL;

        __set_false(msger->running);

        if (msger->sock[2] > 0 && msger->addr != NULL){
            struct xhead pack;
            pack.type = XPACK_TYPE_LOCAL;
            pack.ack.type = XPACK_TYPE_FLUSH;
            __xapi->udp_local_send(msger->sock[2], msger->addr, &pack, XHEAD_SIZE);
        }

        if (msger->tid){
            __xlogd("xmsger_free main process\n");
            __xapi->thread_join(msger->tid);
        }

        xchannel_ptr next, channel = msger->sendlist.head.next;
        while (channel != NULL && channel != &msger->sendlist.head){
            next = channel->next;
            xchannel_free(channel);
            channel = next;
        }

        __xlogd("xmsger_free peers %lu\n", msger->peers.count);
        avl_tree_clear(&msger->peers, NULL);

        if (msger->sock[0] > 0){
            __xapi->udp_close(msger->sock[0]);
        }
        if (msger->sock[1] > 0){
            __xapi->udp_close(msger->sock[1]);
        }
        if (msger->sock[2] > 0){
            __xapi->udp_close(msger->sock[2]);
        }
        if (msger->addr){
            free(msger->addr);
        }
        if (msger->laddr){
            free(msger->laddr);
        }
        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}

// const char* xchannel_get_ip(xchannel_ptr channel)
// {
//     return __xapi->udp_addr_ip(channel->addr);
// }

// uint16_t xchannel_get_port(xchannel_ptr channel)
// {
//     return __xapi->udp_addr_port(channel->addr);
// }

xline_t* xchannel_get_req(xchannel_ptr channel)
{
    return channel->req;
}

void* xchannel_get_ctx(xchannel_ptr channel)
{
    return channel->ctx;
}

void xchannel_set_ctx(xchannel_ptr channel, void *ctx)
{
    channel->ctx = ctx;
}