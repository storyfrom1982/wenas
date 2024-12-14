#include "xmsger.h"

#include "xtree.h"
#include "xbuf.h"

#include "xlib/avlmini.h"

#define XHEAD_SIZE          64
#define XBODY_SIZE          1280 // 1024 + 256
#define XPACK_SIZE          ( XHEAD_SIZE + XBODY_SIZE ) // 1344

#define XPACK_FLAG_ACK      0x00
#define XPACK_FLAG_PING     0x01
#define XPACK_FLAG_PONG     0x02
#define XPACK_FLAG_MSG      0x04
#define XPACK_FLAG_ONL      0x08
#define XPACK_FLAG_BYE      0x10
#define XPACK_FLAG_FINAL    0x20
#define XPACK_FLAG_LOCAL    0x40

#define XPACK_SERIAL_RANGE  64

#define XMSG_PACK_RANGE             8192 // 1K*8K=8M 0.25K*8K=2M 8M+2M=10M 一个消息最大长度是 10M
#define XMSG_MAX_LENGTH             ( XBODY_SIZE * XMSG_PACK_RANGE )

#define XCHANNEL_RESEND_LIMIT       10
#define XCHANNEL_RESEND_STEPPING    1.5
#define XCHANNEL_FEEDBACK_TIMES     1000

typedef struct xhead {
    uint8_t flag; // 包类型
    uint8_t sn; // 序列号
    uint8_t resend; // 重传计数
    struct{
        uint8_t flag; // 标志着是否附带了 ACK 和 ACK 确认的包类型
        uint8_t sn; // 确认收到了序列号为 sn 的包
        uint8_t pos; // 确认收到了 pos 之前的所有包
    }ack;
    uint16_t len; // 当前分包的数据长度
    uint16_t range; // 一个消息的分包数量，从 1 到 range
    uint16_t lcid; // 本端 CID
    uint16_t rcid; // 对端 CID
    uint8_t flags[18]; // 扩展标志位
    uint8_t ext[32]; // 预留扩展
}*xhead_ptr;

typedef struct xmsg {
    void *ctx;
    uint32_t flag;
    uint32_t range;
    uint64_t spos;
    xline_t *xl;
}xmsg_t;

typedef struct xpack {
    uint64_t ts; // 计算往返耗时
    uint64_t delay; // 累计超时时长
    xmsg_t *msg;
    xchannel_ptr channel;
    struct xpack *prev, *next;
    struct xhead head;
    uint8_t body[XBODY_SIZE];
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

typedef struct xmsgbuf {
    uint8_t range, spos, rpos, wpos;
    xmsg_t buf[1];
}*xmsgbuf_ptr;


struct xchannel {

    struct avl_node node;
    struct avl_node temp;

    int sock;

    uint8_t remote_sn;
    uint8_t serial_number;
    uint8_t serial_range;
    uint64_t timestamp;

    uint16_t back_times;
    uint64_t back_delay;
    uint64_t back_range;

    char ip[46];
    uint16_t port;
    uint16_t rcid;
    uint16_t lcid;
    uint64_t cid[3];
    uint64_t temp_cid[3];
    __xipaddr_ptr addr;
    struct xchannel_ctx *ctx;

    // uint8_t status;
    bool keepalive;
    bool connected;
    bool disconnected;
    __atom_bool disconnecting;
    __atom_bool sending;
    __atom_size pos, len;

    xline_t *xlmsg;
    xmsger_ptr msger;
    struct xhead ack;
    xmsgbuf_ptr msgbuf;
    serialbuf_ptr recvbuf;
    sserialbuf_ptr sendbuf;
    struct xpacklist flushlist;
    // struct xchannellist *worklist;

    xchannel_ptr prev, next;
};

typedef struct xchannellist {
    size_t len;
    struct xchannel head;
}*xchannellist_ptr;

struct xmsger {
    int sock[2];
    int lsock;
    uint16_t lport;
    __xipaddr_ptr laddr;
    __xipaddr_ptr addr;
    __atom_bool running;
    __atom_size pos, len;
    uint16_t cid;
    uint64_t timer;
    xmsgercb_ptr cb;
    // xpipe_ptr mpipe;
    __xthread_ptr mpid;
    struct avl_tree peers;
    struct avl_tree temps;
    struct xchannellist send_list;
};

#define __serialbuf_wpos(b)         ((b)->wpos & ((b)->range - 1))
#define __serialbuf_rpos(b)         ((b)->rpos & ((b)->range - 1))
#define __serialbuf_spos(b)         ((b)->spos & ((b)->range - 1))

#define __serialbuf_recvable(b)     ((uint8_t)((b)->spos - (b)->rpos))
#define __serialbuf_sendable(b)     ((uint8_t)((b)->wpos - (b)->spos))

#define __serialbuf_readable(b)     ((uint8_t)((b)->wpos - (b)->rpos))
#define __serialbuf_writable(b)     ((uint8_t)((b)->range - (b)->wpos + (b)->rpos))

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


static inline int xmsg_fixed(xmsg_t *msg)
{
    __xcheck(msg == NULL);
    msg->spos = 0;
    msg->range = (msg->xl->wpos / XBODY_SIZE);
    if (msg->range * XBODY_SIZE < msg->xl->wpos){
        // 有余数，增加一个包
        msg->range ++;
    }
    return 0;
XClean:
    return -1;
}


static inline xchannel_ptr xchannel_create(xmsger_ptr msger, uint8_t serial_range)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xcheck(channel == NULL);

    channel->msger = msger;
    channel->serial_range = serial_range;
    channel->timestamp = __xapi->clock();
    // channel->serial_number = channel->timestamp % channel->serial_range;
    channel->serial_number = 0;
    channel->back_delay = 50000000UL;

    channel->recvbuf = (serialbuf_ptr) calloc(1, sizeof(struct serialbuf) + sizeof(xpack_ptr) * channel->serial_range);
    __xcheck(channel->recvbuf == NULL);
    channel->recvbuf->range = channel->serial_range;

    channel->sendbuf = (sserialbuf_ptr) calloc(1, sizeof(struct sserialbuf) + sizeof(struct xpack) * channel->serial_range);
    __xcheck(channel->sendbuf == NULL);
    channel->sendbuf->range = channel->serial_range;
    channel->sendbuf->rpos = channel->sendbuf->spos = channel->sendbuf->wpos = channel->serial_number;

    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsg_t) * 8);
    __xcheck(channel->msgbuf == NULL);
    channel->msgbuf->range = 8;

    channel->flushlist.len = 0;
    channel->flushlist.head.prev = &channel->flushlist.head;
    channel->flushlist.head.next = &channel->flushlist.head;

    __ring_list_put_into_head(&msger->send_list, channel);

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

    // 清空冲洗队列
    channel->flushlist.len = 0;
    channel->flushlist.head.prev = &channel->flushlist.head;
    channel->flushlist.head.next = &channel->flushlist.head;
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
        if (channel->msgbuf->buf[i].xl != NULL){
            __xlogd("xchannel_clear msg send buf\n");
            // 减掉发送缓冲区的数据
            channel->msger->len -= (channel->msgbuf->buf[i].xl->wpos - channel->msgbuf->buf[i].xl->rpos);
            // xl 加了引用计数，这里需要释放一次
            xl_free(&channel->msgbuf->buf[i].xl);
            channel->msgbuf->buf[i].xl = NULL;
        }
    }

    // 释放未完整接收的消息
    if (channel->xlmsg != NULL){
        __xlogd("xchannel_clear msg recv buf\n");
        xl_free(&channel->xlmsg);
        channel->xlmsg = NULL;
    }
    
    __xlogd("xchannel_clear exit\n");
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free >>>>-------------------> IP[%s] PORT[%u] CID[%u->%u]\n", channel->ip, channel->port, channel->lcid, channel->rcid);
    xchannel_clear(channel);
    // __xchannel_take_out_list(channel);
    __ring_list_take_out(&channel->msger->send_list, channel);
    if (channel->node.parent != &channel->node){
        avl_tree_remove(&channel->msger->peers, channel);
    }
    if (channel->temp.parent != &channel->temp){
        avl_tree_remove(&channel->msger->temps, channel);
    }
    free(channel->recvbuf);
    free(channel->sendbuf);
    free(channel->msgbuf);
    free(channel->addr);
    free(channel);
    __xlogd("xchannel_free >>>>-------------------> exit\n");
}

static inline void xchannel_serial_pack(xchannel_ptr channel, uint8_t flag)
{
    xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];
    pack->head.flags[0] = channel->serial_number;
    pack->head.flags[1] = channel->serial_range;
    pack->msg = NULL;
    pack->head.flag = flag;
    pack->head.range = 1;
    pack->channel = channel;
    pack->delay = channel->back_delay;
    pack->head.resend = 0;
    pack->head.ack.flag = 0;
    pack->head.rcid = channel->rcid;
    pack->head.lcid = channel->lcid;
    pack->head.sn = channel->sendbuf->wpos;
    __atom_add(channel->sendbuf->wpos, 1);
    channel->len += pack->head.len;
    channel->msger->len += pack->head.len;
}

static inline void xchannel_serial_msg(xchannel_ptr channel)
{
    // 每次只缓冲一个包，尽量使发送速度均匀
    if (__serialbuf_writable(channel->sendbuf) > 0 && __serialbuf_sendable(channel->msgbuf) > 0){

        xmsg_t *msg = &channel->msgbuf->buf[__serialbuf_spos(channel->msgbuf)];
        xpack_ptr pack = &channel->sendbuf->buf[__serialbuf_wpos(channel->sendbuf)];

        pack->msg = msg;
        pack->head.flag = msg->flag;
        if (pack->head.flag == XPACK_FLAG_PING){
            pack->head.flags[0] = channel->serial_number;
            pack->head.flags[1] = channel->serial_range;
        }

        if (msg->xl->wpos - msg->spos < XBODY_SIZE){
            pack->head.len = msg->xl->wpos - msg->spos;
        }else{
            pack->head.len = XBODY_SIZE;
        }

        pack->head.range = msg->range;
        mcopy(pack->body, msg->xl->ptr + msg->spos, pack->head.len);
        msg->spos += pack->head.len;
        msg->range--;

        pack->channel = channel;
        pack->delay = channel->back_delay;
        pack->head.resend = 0;
        pack->head.ack.flag = 0;
        pack->head.rcid = channel->rcid;
        pack->head.lcid = channel->lcid;
        pack->head.sn = channel->sendbuf->wpos;
        __atom_add(channel->sendbuf->wpos, 1);
        // 判断消息是否全部写入缓冲区
        if (msg->spos == msg->xl->wpos){
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

        __xlogd("<SEND> TYPE[%u] IP[%s] PORT[%u] CID[%u->%u] ACK[%u:%u:%u] >>>>-------------------> SN[%u]\n", 
            pack->head.flag, channel->ip, channel->port, channel->lcid, channel->rcid, 
            pack->head.ack.flag, pack->head.ack.sn, pack->head.ack.pos, pack->head.sn);

        if (channel->ack.ack.flag != 0){
            // 携带 ACK
            pack->head.ack.flag = channel->ack.ack.flag;
            pack->head.ack.sn = channel->ack.ack.sn;
            pack->head.ack.pos = channel->ack.ack.pos;
        }

        // 判断发送是否成功
        if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(pack->head), XHEAD_SIZE + pack->head.len) == XHEAD_SIZE + pack->head.len){

            // 发送成功才能清空标志
            channel->ack.ack.flag = 0;

            // 缓冲区下标指向下一个待发送 pack
            __atom_add(channel->sendbuf->spos, 1);

            // 记录发送次数
            pack->head.resend = 1;

            // 记录当前时间
            pack->ts = __xapi->clock();
            pack->delay = channel->back_delay * XCHANNEL_RESEND_STEPPING * 2;
            channel->timestamp = pack->ts;
            __ring_list_put_into_end(&channel->flushlist, pack);

            // 如果有待发送数据，确保 sendable 会大于 0
            xchannel_serial_msg(channel);

        }else {

            __xlogd("xchannel_send_pack >>>>------------------------> SEND FAILED\n");
        }
    }
}

static inline void xchannel_send_ack(xchannel_ptr channel)
{
    __xlogd("<SEND ACK> IP[%s] PORT[%u] CID[%u->%u] >>>>-------------------> ACK[%u:%u:%u]\n", 
        channel->ip, channel->port, channel->lcid, channel->rcid, channel->ack.ack.flag, channel->ack.ack.sn, channel->ack.ack.pos);

    if ((__xapi->udp_sendto(channel->sock, channel->addr, (void*)&channel->ack, XHEAD_SIZE)) == XHEAD_SIZE){
        channel->ack.ack.flag = 0;
    }else {
        __xlogd("xchannel_send_ack >>>>------------------------> SEND FAILED\n");
    }
}

static inline void xchannel_send_final(int sock, __xipaddr_ptr addr, xpack_ptr pack)
{
    __xlogd("<SEND FINAL> CID[%u->%u] >>>>-------------------> ACK[%u:%u:%u]\n", 
        pack->head.lcid, pack->head.rcid, pack->head.ack.flag, pack->head.ack.sn, pack->head.ack.pos);
    // 调换 cid
    uint16_t cid = pack->head.rcid;
    pack->head.rcid = pack->head.lcid;
    pack->head.lcid = cid;
    // 设置 flag
    pack->head.flag = XPACK_FLAG_ACK;
    pack->head.ack.flag = XPACK_FLAG_BYE;
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
    if (__serialbuf_readable(channel->recvbuf) > 0){
        // 索引已接收的包
        xpack_ptr pack = channel->recvbuf->buf[__serialbuf_rpos(channel->recvbuf)];

        do {

            if (pack->head.len > 0){
                // 如果当前接收缓冲区为空，证明这个包是新消息的第一个包
                if (channel->xlmsg == NULL){
                    channel->xlmsg = xl_creator(pack->head.range * XBODY_SIZE);
                    __xcheck(channel->xlmsg == NULL);
                }
                mcopy(channel->xlmsg->ptr + channel->xlmsg->wpos, pack->body, pack->head.len);
                channel->xlmsg->wpos += pack->head.len;
                // 收到了一个完整的消息
                if (channel->xlmsg->size - channel->xlmsg->wpos < XBODY_SIZE){
                    // 更新消息长度
                    xl_fixed(channel->xlmsg);
                    // 通知用户已收到一个完整的消息
                    channel->msger->cb->on_msg_from_peer(channel->msger->cb, channel, channel->xlmsg);
                    channel->xlmsg = NULL;
                    // 更新时间戳
                    channel->timestamp = __xapi->clock();
                    // // 判断队列是否有多个成员
                    // if (channel->worklist->len > 1){
                    //     // 将更新后的成员移动到队尾
                    //     __xchannel_move_to_end(channel);
                    // }
                }
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
    __xlogd("xchannel_recv_ack >>>>-----------> ack[%u:%u] rpos=%u spos=%u ack-rpos=%u spos-rpos=%u\n", 
            rpack->head.ack.sn, rpack->head.ack.pos, channel->sendbuf->rpos, channel->sendbuf->spos, 
            (uint8_t)(rpack->head.ack.sn - channel->sendbuf->rpos), (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos));

    // 只处理 sn 在 rpos 与 spos 之间的 xpack
    if (__serialbuf_recvable(channel->sendbuf) > 0 
        && ((uint8_t)(rpack->head.ack.sn - channel->sendbuf->rpos) 
        <= (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos))){

        xpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        // 如果没有丢包，第一个顺序到达的 acks 一定等于 ack，但是如果这个包丢了，acks 就会不等于 ack
        // 所以每次都要检测 rpos 是否等于 asks

        uint8_t index = __serialbuf_rpos(channel->sendbuf);

        // 这里曾经使用 do while 方式，造成了收到重复的 ACK，导致 rpos 越界的 BUG
        // 连续的 acks 必须至少比 rpos 大 1
        while ((uint8_t)(rpack->head.ack.pos - channel->sendbuf->rpos) 
                <= (uint8_t)(channel->sendbuf->spos - channel->sendbuf->rpos) // 防止后到的 acks 比之前的 acks 和 rpos 小，导致 rpos 越界
                && channel->sendbuf->rpos != rpack->head.ack.pos) {

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

                // 数据已发送，从待发送数据中减掉这部分长度
                __atom_add(channel->pos, pack->head.len);
                __atom_add(channel->msger->pos, pack->head.len);

                __ring_list_take_out(&channel->flushlist, pack);
                // // 判断是否有未发送的消息和未确认消息和是否需要保活
                // if (channel->flushlist.len == 0 
                //     && channel->pos == channel->len
                //     && !channel->keepalive){
                //     // 不需要保活，必须加入等待超时队列
                //     if (__atom_try_lock(channel->sending)){
                //         // 这里使用 try lock 
                //         // 如果没有获得锁，是因为外部正在写入数据，所以不需要切换到接收队列
                //         __xchannel_take_out_list(channel);
                //         __xchannel_put_into_list(&channel->msger->recv_list, channel);
                //         __atom_unlock(channel->sending);
                //     }
                // }
            }

            if (pack->head.flag == XPACK_FLAG_MSG){
                // 更新已经到达对端的数据计数
                pack->msg->xl->rpos += pack->head.len;
                if (pack->msg->xl->rpos == pack->msg->xl->wpos){
                    // 把已经传送到对端的 msg 交给发送线程处理
                    channel->msger->cb->on_msg_to_peer(channel->msger->cb, channel, pack->msg->xl);
                    channel->msgbuf->buf[__serialbuf_rpos(channel->msgbuf)].xl = NULL;
                    __atom_add(channel->msgbuf->rpos, 1);
                    pack->msg = NULL;
                }

            }else if (pack->head.flag == XPACK_FLAG_PONG){

                if (channel->temp.parent != &channel->temp){
                    avl_tree_remove(&channel->msger->temps, channel);
                }

            }else if (pack->head.flag == XPACK_FLAG_BYE){

                // 这里只能执行一次，首先 channel 会从 peers 中移除，还有 ack 也不会被重复确认
                __xlogd("RECV FINAL ACK: IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->rcid, channel->lcid);
                __set_true(channel->disconnecting);
                channel->disconnected = true;
                channel->msger->cb->on_disconnection(channel->msger->cb, channel);
                avl_tree_remove(&channel->msger->peers, channel);
            }

            __atom_add(channel->sendbuf->rpos, 1);

            // 更新索引
            index = __serialbuf_rpos(channel->sendbuf);

            xchannel_send_pack(channel);
            // if ((__serialbuf_sendable(channel->msgbuf) > 0 || __serialbuf_sendable(channel->sendbuf) > 0 )
            //     && __serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 2)){
            //     __xloge("xchannel_recv_ack >>>>------------------------> serial\n");
            //     xchannel_send_pack(channel);
            // }

            // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
        }

        __xlogd("RECV ACK OUT OF OEDER >>>>-----------> sn=%u pos=%u\n", rpack->head.ack.sn, rpack->head.ack.pos);

        if (rpack->head.ack.sn != rpack->head.ack.pos){

            __xlogd("RECV ACK OUT OF OEDER >>>>-----------> IP[%s] PORT[%u] CID[%u->%u] ACK[%u:%u:%u]\n", 
                    channel->ip, channel->port, channel->lcid, channel->rcid, 
                    rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos);

            pack = &channel->sendbuf->buf[rpack->head.ack.sn & (channel->sendbuf->range - 1)];

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
                // 数据已发送，从待发送数据中减掉这部分长度
                __atom_add(channel->pos, pack->head.len);
                __atom_add(channel->msger->pos, pack->head.len);
            }

            // ack 与 rpos 的间隔大于一才进行重传
            if (((rpack->head.ack.sn - channel->sendbuf->rpos) & (channel->sendbuf->range - 1)) >= 1){
                // 使用临时变量
                uint8_t index = channel->sendbuf->rpos;
                // 实时重传 rpos 到 SN 之间的所有尚未确认的 SN
                while (index != rpack->head.ack.sn){
                    // 取出落后的包
                    pack = &channel->sendbuf->buf[(index & (channel->sendbuf->range - 1))];
                    // 判断这个包是否已经接收过
                    if (pack->ts != 0){
                        // 判断是否进行了重传
                        if (pack->head.resend < 2){
                            pack->head.resend++;
                            // 判断重传的包是否带有 ACK
                            if (pack->head.ack.flag != 0){
                                // 更新 ACKS
                                // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                pack->head.ack.pos = channel->recvbuf->wpos;
                            }
                            __xlogd("<RESEND> TYPE[%u] IP[%s] PORT[%u] CID[%u->%u] ACK[%u:%u:%u] >>>>-------------------> SN[%u]\n", 
                                    pack->head.flag, channel->ip, channel->port, channel->lcid, channel->rcid, 
                                    pack->head.ack.flag, pack->head.ack.sn, pack->head.ack.pos, pack->head.sn);
                            if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(pack->head), XHEAD_SIZE + pack->head.len) == XHEAD_SIZE + pack->head.len){
                                pack->delay *= XCHANNEL_RESEND_STEPPING;
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

    channel->ack.flag = XPACK_FLAG_ACK;
    channel->ack.ack.flag = pack->head.flag;

    uint16_t index = pack->head.sn & (channel->recvbuf->range - 1);

    // 如果收到连续的 PACK
    if (pack->head.sn == channel->recvbuf->wpos){        

        pack->channel = channel;
        // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
        if (pack->head.ack.flag != 0){
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
        if ((uint8_t)(channel->recvbuf->wpos - pack->head.sn) > (uint8_t)(pack->head.sn - channel->recvbuf->wpos)){

            __xlogd("RECV EARLY >>>>--------> IP=[%s] PORT=[%u] CID[%u->%u] FLAG[%u] SN[%u] WPOS[%u]\n", 
                    channel->ip, channel->port, channel->lcid, channel->rcid, pack->head.flag, pack->head.sn, channel->recvbuf->wpos);

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack.sn = pack->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.ack.pos = channel->recvbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->recvbuf->wpos - 1;
            
            if (channel->recvbuf->buf[index] == NULL){
                pack->channel = channel;
                // 只处理第一次到达的包带 ACK，否则会造成发送缓冲区索引溢出的 BUG
                if (pack->head.ack.flag != 0){
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

            __xlogd("RECV AGAIN >>>>--------> IP=[%s] PORT=[%u] CID[%u->%u] SN[%u]\n", 
                    channel->ip, channel->port, channel->lcid, channel->rcid, pack->head.sn);
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.ack.pos = channel->recvbuf->wpos;
            channel->ack.ack.sn = channel->ack.ack.pos;
            // 重复到达的 PACK
        }
    }

    return xchannel_recv_msg(channel);
}


static inline bool xmsger_recv_local(xmsger_ptr msger, xhead_ptr head)
{
    // xchannel_ptr channel = (xchannel_ptr)head->flags[0];
    xmsg_t msg;
    msg.flag = head->ack.flag;

    if (msg.flag == XPACK_FLAG_PING){

        xchannel_ptr channel = xchannel_create(msger, XPACK_SERIAL_RANGE);
        if(channel == NULL){
            return false;
        }

        msg.xl = (xline_t*)(*(uint64_t*)&head->flags[8]);
        xmsg_fixed(&msg);
        xline_t parser = xl_parser(&msg.xl->data);
        char *ip = xl_find_word(&parser, "host");
        uint64_t port = xl_find_uint(&parser, "port");

        channel->ctx = (void*)(*(uint64_t*)&head->flags[0]);
        mcopy(channel->ip, ip, slength(ip));
        channel->port = port;
        channel->addr = __xapi->udp_host_to_addr(channel->ip, channel->port);
        if (__xapi->udp_addr_is_ipv6(channel->addr)){
            channel->sock = channel->msger->sock[1];
        }else {
            channel->sock = channel->msger->sock[0];
        }
        // channel->lcid.port = channel->addr->port;
        // channel->lcid.ip = channel->addr->ip;

        do {
            channel->lcid = msger->cid++;
            channel->cid[0] = ((uint64_t*)channel->addr)[0];
            channel->cid[1] = ((uint64_t*)channel->addr)[1];
            channel->cid[2] = ((uint64_t*)channel->addr)[2];
            *(uint16_t*)&channel->cid[0] = channel->lcid;
        }while (avl_tree_add(&msger->peers, channel) != NULL);

        channel->rcid = channel->lcid;

        channel->keepalive = true;

        // channel->status = XPACK_FLAG_PING;
        // msg->flag = XMSG_PACK_PING;

        // // 先用本端的 cid 作为对端 channel 的索引，重传这个 HELLO 就不会多次创建连接
        // xchannel_serial_pack(channel, XMSG_PACK_PING);

        __xcheck(msg.xl->wpos > XBODY_SIZE);
        __atom_add(channel->msger->len, msg.xl->wpos);
        __atom_add(channel->len, msg.xl->wpos);
        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);
        // if (channel->worklist != &msger->send_list){
        //     // 这里无需加锁，因为此时，其他线程还没这个连接的指针
        //     __xchannel_take_out_list(channel);
        //     __xchannel_put_into_list(&msger->send_list, channel);
        // }

        __xlogd("xmsger_recv_local >>>>--------> Create channel IP=[%s] PORT=[%u] CID[%u] SN[%u]\n", 
                                    channel->ip, port, channel->lcid, channel->serial_number);

    }else if (msg.flag == XPACK_FLAG_BYE){

        msg.xl = (xline_t*)(*(uint64_t*)&head->flags[8]);
        xmsg_fixed(&msg);
        xchannel_ptr channel = (xchannel_ptr)(*(uint64_t*)&head->flags[0]);
        __xcheck(channel == NULL);
        __xcheck(msg.xl->wpos > XBODY_SIZE);
        __atom_add(channel->msger->len, msg.xl->wpos);
        __atom_add(channel->len, msg.xl->wpos);
        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);
        __xlogd("xmsger_recv_local >>>>--------> Release channel IP=[%s] PORT=[%u] CID[%u->%u] SN[%u]\n", 
                    channel->ip, channel->port, channel->lcid, channel->rcid, channel->serial_number);

    }else if (msg.flag == XPACK_FLAG_FINAL){
        xchannel_ptr channel = (xchannel_ptr)(*(uint64_t*)&head->flags[0]);
        __xlogd("xmsger_recv_local >>>>--------> XLMSG_FLAG_FINAL IP=[%s] PORT=[%u] CID[%u->%u] SN[%u]\n", 
                                    channel->ip, channel->port, channel->lcid, channel->rcid, channel->serial_number);
        xchannel_free(channel);
    }

    return true;
XClean:
    return false;
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
            // __xlogd("xmsger_send_all >>>>------------------------> msg sendable = %u\n", channel->msgbuf->spos);
            // if (__serialbuf_readable(channel->sendbuf) < (channel->serial_range >> 3))
            if (__serialbuf_readable(channel->sendbuf) < 3)
            {
                // __xlogd("xmsger_send_all >>>>------------------------> send packet %lu\n",
                //         __serialbuf_sendable(channel->msgbuf));
                xchannel_send_pack(channel);
                // __xlogd("xmsger_send_all >>>>------------------------> send packet exit\n");
                // // 发送缓冲区未到达8/1，且有消息未发送完，主线程不休眠
                // if (__serialbuf_sendable(channel->msgbuf) > 0){
                //     msger->timer = 1000000;
                // }
            }

            if (channel->flushlist.len > 0){

                spack = channel->flushlist.head.next;

                while (spack != &channel->flushlist.head)
                {
                    xpack_ptr next_pack = spack->next;
                    delay = (int64_t)((spack->ts + spack->delay) - __xapi->clock());
                    if (delay >= 0) {
                        // 未超时
                        if (msger->timer > delay){
                            // 超时时间更近，更新休息时间
                            msger->timer = delay;
                        }
                        // 第一个包未超时，后面的包就都没有超时
                        break;

                    }else {

                        // __xlogd("backdelay=%lu spack->delay =%lu delay ====================== %ld\n", channel->back_delay, spack->delay / 1000000UL, delay);

                        if (spack->delay > NANO_SECONDS * XCHANNEL_RESEND_LIMIT)
                        {
                            // if (!channel->disconnected){
                            //     channel->disconnected = true;
                            //     msger->callback->on_timeout(msger->callback, channel);
                            // }
                            if (!channel->disconnected){
                                __xlogd("SEND TIMEOUT >>>>-------------> IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);
                                __set_true(channel->disconnecting);
                                channel->disconnected = true;
                                msger->cb->on_timeout(msger->cb, channel);
                            }
                            // // 移除超时的连接
                            // avl_tree_remove(&msger->peers, channel);
                            // xchannel_free(channel);
                            break;

                        }else {

                            // 超时重传

                            // 判断重传的包是否带有 ACK
                            if (spack->head.ack.flag != 0){
                                // 更新 ACKS
                                // 是 recvbuf->wpos 而不是 __serialbuf_wpos(channel->recvbuf) 否则会造成接收端缓冲区溢出的 BUG
                                spack->head.ack.pos = channel->recvbuf->wpos;
                                // TODO ack 也要更新
                            }

                            __xlogd("<RESEND> TYPE[%u] IP[%s] PORT[%u] CID[%u->%u] ACK[%u:%u:%u] >>>>-----> SN[%u] Delay[%lu:%lu]\n", 
                                    spack->head.flag, channel->ip, channel->port, channel->lcid, channel->rcid, 
                                    spack->head.ack.flag, spack->head.ack.sn, spack->head.ack.pos, spack->head.sn, 
                                    channel->back_delay / 1000000UL, spack->delay / 1000000UL);

                            // 判断发送是否成功
                            if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(spack->head), XHEAD_SIZE + spack->head.len) == XHEAD_SIZE + spack->head.len){
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
                                __xlogd(">>>>------------------------> SEND FAILED\n");
                            }

                            if (msger->timer > channel->back_delay){
                                msger->timer = channel->back_delay;
                            }
                        }
                    }

                    spack = next_pack;
                }

            }else if (channel->pos == channel->len){ // 只要发送缓冲区有数据， pos 就不可能等于 len

                if (channel->keepalive){

                    if ((delay = ((NANO_SECONDS * 9) - (__xapi->clock() - channel->timestamp))) > 0) {
                        // 未超时
                        if (msger->timer > delay){
                            // 超时时间更近，更新休息时间
                            msger->timer = delay;
                        }

                    }else {
                        __xlogd("xmsger_send_all timestamp=%lu delay = %ld\n", channel->timestamp, delay);
                        xchannel_serial_pack(channel, XPACK_FLAG_ONL);
                        // 更新时间戳
                        channel->timestamp = __xapi->clock();

                    }

                }else {

                    if (__xapi->clock() - channel->timestamp > NANO_SECONDS * 10){
                        if (!channel->disconnected){
                            __xlogd("RECV TIMEOUT >>>>-------------> IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);
                            __set_true(channel->disconnecting);
                            channel->disconnected = true;
                            msger->cb->on_disconnection(msger->cb, channel);
                        }
                    }
                }
            }

            channel = next_channel;
        }
    }
}

// static inline void xmsger_check_all(xmsger_ptr msger)
// {
//     if (msger->recv_list.len > 0){
        
//         xchannel_ptr next_channel, channel = msger->recv_list.head.next;

//         while (channel != &msger->recv_list.head){
//             next_channel = channel->next;
//             // 10 秒钟超时
//             if (__xapi->clock() - channel->timestamp > NANO_SECONDS * 10){
//                 if (!channel->disconnected){
//                     __xlogd("RECV TIMEOUT >>>>-------------> IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);
//                     channel->disconnected = true;
//                     msger->cb->on_disconnection(msger->cb, channel);
//                 }
//                 // if (channel->status != XPACK_FLAG_BYE){
//                 //     channel->status = XPACK_FLAG_BYE;
//                 //     msger->cb->on_disconnection(msger->cb, channel);
//                 // }
//                 // // 移除超时的连接
//                 // avl_tree_remove(&msger->peers, channel);
//                 // // 如果连接正在创建中，需要从缓存中移除
//                 // if (!channel->connected){
//                 //     avl_tree_remove(&msger->temp_channels, channel);
//                 // }
//                 // xchannel_free(channel);
//             }else {
//                 // 队列的第一个连接没有超时，后面的连接就都没有超时
//                 break;
//             }

//             channel = next_channel;
//         }
//     }

// }

static void main_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    xmsger_ptr msger = (xmsger_ptr)ptr;

    xmsg_t msg = {0};
    xchannel_ptr channel = NULL;

    size_t sid;
    uint64_t cid[3] = {0};
    __xipaddr_ptr addr = __xapi->udp_any_to_addr(0, 0);

    xpack_ptr rpack = (xpack_ptr)malloc(sizeof(struct xpack));
    __xcheck(rpack == NULL);
    rpack->head.len = 0;

    msger->timer = 10000000UL; // 10 毫秒

    while (msger->running)
    {
    for (sid = 0; sid < 2; sid++)
    {
        ((uint64_t*)addr)[0] = ((uint64_t*)addr)[1] = ((uint64_t*)addr)[2] = 0;
        while (__xapi->udp_recvfrom(msger->sock[sid], addr, &rpack->head, XPACK_SIZE) == (rpack->head.len + XHEAD_SIZE)){

            if (rpack->head.flag == XPACK_FLAG_LOCAL){
                // char ip[46] = {0};
                // uint16_t port = 0;
                // __xapi->udp_addr_to_host(addr, ip, &port);
                // __xlogd("ip=%s port=%u\n", ip, port);
                if (mcompare(addr, msger->laddr, 8) == 0){
                    xmsger_recv_local(msger, &rpack->head);
                }
                rpack->head.len = 0;
                continue;
            }

            cid[0] = ((uint64_t*)addr)[0];
            cid[1] = ((uint64_t*)addr)[1];
            cid[2] = ((uint64_t*)addr)[2];
            *(uint16_t*)&cid[0] = rpack->head.rcid;
            // cid.cid = rpack->head.cid;
            // cid.port = msger->addr->port;
            // cid.ip = msger->addr->ip;

            channel = avl_tree_find(&msger->peers, &cid);

            if (channel){

                __xlogd("[RECV] TYPE(%u) IP(%s) PORT(%u) CID(%u->%u) FLAG(%u:%u:%u) SN(%u)\n",
                        rpack->head.flag, channel->ip, channel->port, channel->rcid, rpack->head.rcid, 
                        rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos, rpack->head.sn);

                if (rpack->head.flag == XPACK_FLAG_MSG) {
                    __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                    if (__serialbuf_sendable(channel->sendbuf) > 0){
                        xchannel_send_pack(channel);
                    }else {
                        xchannel_send_ack(channel);
                    }

                }else if (rpack->head.flag == XPACK_FLAG_ACK){
                    xchannel_recv_ack(channel, rpack);

                }else if (rpack->head.flag == XPACK_FLAG_PONG){

                    __xlogd("xmsger_loop >>>>-------------> RECV PONG: IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);

                    if (!channel->connected){
                        channel->connected = true;
                        // 取出同步参数
                        uint8_t serial_number = rpack->head.flags[0];
                        uint8_t serial_range = rpack->head.flags[1];
                        // 更新 rcid
                        channel->rcid = rpack->head.lcid;
                        channel->ack.rcid = channel->rcid;

                        // uint64_t cid[3];
                        // cid[0] = ((uint64_t*)channel->addr)[0];
                        // cid[1] = ((uint64_t*)channel->addr)[1];
                        // cid[2] = ((uint64_t*)channel->addr)[2];
                        // *(uint16_t*)&cid[0] = channel->rcid;
                        // __xlogd("recv ping ack temp channels cid=%u cid[%lu] cid[%lu] cid[%lu]\n", 
                        //     channel->rcid, cid[0], cid[1], cid[2]);

                        // 同步序列号
                        channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                        __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                        xchannel_send_ack(channel);
                        // 通知用户建立连接
                        channel->msger->cb->on_connection_to_peer(channel->msger->cb, channel);                            
                    }else {
                        // 更新接收缓冲区和 ACK
                        __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                        xchannel_send_ack(channel);
                    }

                }else if (rpack->head.flag == XPACK_FLAG_BYE){
                    if (!channel->disconnected){
                        __xlogd("RECV BYE >>>>-------------> IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);
                        __set_true(channel->disconnecting);
                        channel->disconnected = true;
                        msger->cb->on_disconnection(msger->cb, channel);
                        // // 被动端收到 BYE，删除索引
                        avl_tree_remove(&msger->peers, channel);
                    }
                    // 回复最后的 ACK
                    xchannel_send_final(msger->sock[sid], addr, rpack);

                }else {
                    __xlogd("RECV UNKNOWN FLAG >>>>-------------> IP(%s) PORT(%u) CID(%u->%u)\n", channel->ip, channel->port, channel->lcid, channel->rcid);
                }

            } else {

                // 收到对方发起的 PING
                if (rpack->head.flag == XPACK_FLAG_PING){
                    // 取出同步参数
                    uint8_t serial_number = rpack->head.flags[0];
                    uint8_t serial_range = rpack->head.flags[1];
                    channel = avl_tree_find(&msger->temps, &cid);
                    if (channel == NULL){
                        // 创建连接
                        channel = xchannel_create(msger, serial_range);
                        __xcheck(channel == NULL);

                        channel->temp_cid[0] = cid[0];
                        channel->temp_cid[1] = cid[1];
                        channel->temp_cid[2] = cid[2];
                        avl_tree_add(&msger->temps, channel);

                        __xapi->udp_addr_to_host(addr, channel->ip, &channel->port);
                        channel->addr = __xapi->udp_host_to_addr(channel->ip, channel->port);
                        channel->sock = channel->msger->sock[sid];

                        do {
                            channel->lcid = msger->cid++;
                            channel->cid[0] = ((uint64_t*)channel->addr)[0];
                            channel->cid[1] = ((uint64_t*)channel->addr)[1];
                            channel->cid[2] = ((uint64_t*)channel->addr)[2];
                            *(uint16_t*)&channel->cid[0] = channel->lcid;
                        }while (avl_tree_add(&msger->peers, channel) != NULL);

                        __xlogd("[RECV] TYPE(%u) IP(%s) PORT(%u) CID(%u->%u) FLAG(%u:%u:%u) SN(%u)\n",
                                rpack->head.flag, channel->ip, channel->port, channel->rcid, rpack->head.rcid, 
                                rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos, rpack->head.sn);

                        // 同步序列号
                        channel->remote_sn = channel->recvbuf->rpos = channel->recvbuf->spos = channel->recvbuf->wpos = serial_number;
                        // 设置对端 cid
                        channel->rcid = rpack->head.lcid;;
                        channel->ack.rcid = channel->rcid;
                        channel->connected = true;
                        channel->msger->cb->on_connection_from_peer(channel->msger->cb, channel);
                        // 更新接收缓冲区和 ACK
                        __xcheck(xchannel_recv_pack(channel, &rpack) != 0);
                        xchannel_serial_pack(channel, XPACK_FLAG_PONG);
                        xchannel_send_pack(channel);

                    }else {
                        // 检查 serial range 是否一致
                        if (serial_range == channel->recvbuf->range 
                            && serial_number == channel->remote_sn){
                            // 同一个 PING，回复同一个 PONG
                            xpack_ptr pack = channel->flushlist.head.next;
                            if (__xapi->udp_sendto(channel->sock, channel->addr, (void*)&(pack->head), XHEAD_SIZE + pack->head.len) == XHEAD_SIZE + pack->head.len){
                                __xlogd("xmsger_loop >>>>-------------> SEND FAILED\n");
                            }
                        }else {
                            // 不是同一个 PING，不回应，让对方超时以后换一个 CID 再创建连接
                        }
                    }

                }else if (rpack->head.flag == XPACK_FLAG_BYE){

                    __xlogd("xmsger_loop >>>>-------------> RECV REPEATED BYE: CID(%u)\n", rpack->head.rcid);
                    // 被动端收到重复的 BYE，回复最后的 ACK
                    xchannel_send_final(msger->sock[sid], addr, rpack);

                }else if (rpack->head.flag == XPACK_FLAG_ACK){
                    
                    // if (rpack->head.ack.flag == XPACK_FLAG_BYE){
                    //     // 主动端收到 BYE 的 ACK，移除索引
                    //     // __xlogd("temp channels = %lu\n", msger->temps.count);
                    //     channel = avl_tree_find(&msger->peers, &cid[0]);
                    //     __xlogd("xmsger_loop >>>>-------------> RECV FINAL ACK: CID(%u) FLAG(%u:%u:%u)\n", rpack->head.rcid, rpack->head.ack.flag, rpack->head.ack.sn, rpack->head.ack.pos);
                    //     __xlogd("xmsger_loop >>>>-------------> RECV FINAL ACK: cid[%lu] cid[%lu] cid[%lu]\n", cid[0], cid[1], cid[2]);
                    //     if (channel){
                    //         __xlogd("xmsger_loop >>>>-------------> RECV FINAL ACK First: cid[%lu] cid[%lu] cid[%lu]\n", channel->cid[0], channel->cid[1], channel->cid[2]);
                    //         __xlogd("xmsger_loop >>>>-------------> RECV FINAL ACK First enter: IP(%s) PORT(%u) CID(%u)\n", channel->ip, channel->port, channel->rcid);
                    //         channel->status = XPACK_FLAG_BYE;
                    //         msger->cb->on_disconnection(msger->cb, channel);
                    //         __xlogd("xmsger_loop >>>>-------------> RECV FINAL ACK First exit: IP(%s) PORT(%u) CID(%u)\n", channel->ip, channel->port, channel->rcid);
                    //         avl_tree_remove(&msger->peers, channel);
                    //         // channel->temp.left = channel->temp.right = NULL;
                    //         // xchannel_free(channel);
                    //     }
                    // }

                }else {}
            }

            if (rpack == NULL){
                rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                __xcheck(rpack == NULL);
            }

            rpack->head.len = 0;

            // if (__xpipe_read(msger->mpipe, &msg, sizeof(msg)) == sizeof(msg)){
            //     __xpipe_notify(msger->mpipe);
            //     __xcheck(!xmsger_recv_local(msger, &msg));
            // }            

            // 检查每个连接，如果满足发送条件，就发送一个数据包
            xmsger_send_all(msger);
            // // 检查超时连接
            // xmsger_check_all(msger);
        }
        }

        // // __xlogd("xmsger_loop 1\n");
        // if (__xpipe_read(msger->mpipe, &msg, sizeof(msg)) == sizeof(msg)){
        //     __xpipe_notify(msger->mpipe);
        //     __xcheck(!xmsger_recv_local(msger, &msg));
        // else 
        {
            // __xlogd("xmsger_loop 2\n");
            __xapi->udp_listen(msger->sock, msger->timer / 1000);
        }

        msger->timer = 10000000UL; // 10 毫秒

        // __xlogd("xmsger_loop 3\n");
        // 检查每个连接，如果满足发送条件，就发送一个数据包
        xmsger_send_all(msger);
        // // 检查超时连接
        // // __xlogd("xmsger_loop 4\n");
        // xmsger_check_all(msger);
    }


    XClean:


    if (rpack != NULL){
        free(rpack);
    }

    if (addr){
        free(addr);
    }

    __xlogd("xmsger_loop exit\n");

    return;
}

bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, xline_t *xl)
{
    __xcheck(channel == NULL || xl == NULL);

    if (__serialbuf_writable(channel->msgbuf) > 0){

        xmsg_t msg;
        msg.xl = xl;
        msg.ctx = channel;
        msg.flag = XPACK_FLAG_MSG;
        __xcheck(xmsg_fixed(&msg) != 0);

        __atom_add(msger->len, xl->wpos);
        __atom_add(channel->len, xl->wpos);

        channel->msgbuf->buf[__serialbuf_wpos(channel->msgbuf)] = msg;
        __atom_add(channel->msgbuf->wpos, 1);

        // // 必须先锁住 worklist
        // __atom_lock(channel->sending);
        // if (channel->worklist != &msger->send_list){
        //     // 发送一个消息，网络线程收到后会切换队列
        //     // 如果在这里切换队列，网络线程中好多地方就必须加锁
        //     if ((xpipe_write(msger->mpipe, &msg, sizeof(msg)) != sizeof(msg))){
        //         // 如果执行到这里，网络线程也将会终止，所有资源都将会被清空
        //         __xloge("xpipe_write() failed\n");
        //         __atom_unlock(channel->sending);
        //         goto XClean;
        //     }
        // }
        // __atom_unlock(channel->sending);
        return true;
    }

XClean:
    return false;
}

bool xmsger_final(xmsger_ptr msger, xchannel_ptr channel)
{
    // 状态错误会报错
    __xcheck(!__set_false(channel->disconnecting));
    struct xhead pack;
    pack.flag = XPACK_FLAG_LOCAL;
    pack.ack.flag = XPACK_FLAG_FINAL;
    *((uint64_t*)(&pack.flags[0])) = (uint64_t)channel;
    __xapi->udp_send_local(msger->lsock, msger->addr, &pack, XHEAD_SIZE);
    // xmsg_t msg;
    // msg.ctx = channel;
    // msg.flag = XPACK_FLAG_FINAL;
    // __xcheck(xpipe_write(msger->mpipe, &msg, sizeof(msg)) != sizeof(msg));
    return true;
XClean:
    return false;
}

bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel, xline_t *xl)
{
    __xcheck(channel == NULL || xl == NULL);
    // 重复调用会报错
    __xcheck(!__set_true(channel->disconnecting));
    struct xhead pack;
    pack.flag = XPACK_FLAG_LOCAL;
    pack.ack.flag = XPACK_FLAG_BYE;
    *((uint64_t*)(&pack.flags[0])) = (uint64_t)channel;
    *((uint64_t*)(&pack.flags[8])) = (uint64_t)xl;
    __xapi->udp_send_local(msger->lsock, msger->addr, &pack, XHEAD_SIZE);
    // xmsg_t msg;
    // msg.xl = xl;
    // msg.ctx = channel;
    // msg.flag = XPACK_FLAG_BYE;
    // __xcheck(xmsg_fixed(&msg) != 0);
    // __xcheck(xpipe_write(msger->mpipe, &msg, sizeof(msg)) != sizeof(msg));
    return true;
XClean:
    return false;

}

bool xmsger_connect(xmsger_ptr msger, void *ctx, xline_t *xl)
{
    __xcheck(msger == NULL || xl == NULL);
    struct xhead pack;
    pack.flag = XPACK_FLAG_LOCAL;
    pack.ack.flag = XPACK_FLAG_PING;
    *((uint64_t*)(&pack.flags[0])) = (uint64_t)ctx;
    *((uint64_t*)(&pack.flags[8])) = (uint64_t)xl;
    __xapi->udp_send_local(msger->lsock, msger->addr, &pack, XHEAD_SIZE);

    // xmsg_t msg;
    // msg.xl = xl;
    // msg.ctx = ctx;
    // msg.flag = XPACK_FLAG_PING;
    // __xcheck(xmsg_fixed(&msg) != 0);
    // __xcheck(xpipe_write(msger->mpipe, &msg, sizeof(msg)) != sizeof(msg));

    return true;
XClean:
    return false;
}

static inline int temp_cid_compare(const void *a, const void *b)
{
    uint64_t *x = &((xchannel_ptr)a)->temp_cid[0];
    uint64_t *y = &((xchannel_ptr)b)->temp_cid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline int temp_cid_find(const void *a, const void *b)
{
    uint64_t *x = (uint64_t*)a;
    uint64_t *y = &((xchannel_ptr)b)->temp_cid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline int cid_compare(const void *a, const void *b)
{
    uint64_t *x = &((xchannel_ptr)a)->cid[0];
    uint64_t *y = &((xchannel_ptr)b)->cid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline int cid_find(const void *a, const void *b)
{
    uint64_t *x = (uint64_t*)a;
    uint64_t *y = &((xchannel_ptr)b)->cid[0];
    for (int i = 0; i < 3; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

xmsger_ptr xmsger_create(xmsgercb_ptr callback, int ipv6)
{
    __xlogd("xmsger_create enter\n");

    __xcheck(callback == NULL);

    xmsger_ptr msger = (xmsger_ptr)calloc(1, sizeof(struct xmsger));

    msger->sock[0] = __xapi->udp_open(0, 1, 1);
    __xcheck(msger->sock[0] < 0);
    __xcheck(__xapi->udp_bind(msger->sock[0], 9256) == -1);

    if (ipv6){
        msger->sock[1] = __xapi->udp_open(1, 1, 1);
        __xcheck(msger->sock[1] < 0);
        __xcheck(__xapi->udp_bind(msger->sock[1], 9256) == -1);
    }else {
        msger->sock[1] = __xapi->udp_open(0, 1, 1);
        __xcheck(msger->sock[1] < 0);
        __xcheck(__xapi->udp_bind(msger->sock[1], 9257) == -1);
    }

    msger->lsock = __xapi->udp_open(0, 0, 0);
    __xcheck(msger->lsock < 0);
    msger->lport = 9256;
    do {
        msger->lport++;
    }while (__xapi->udp_bind(msger->lsock, msger->lport) == -1);
    msger->addr = __xapi->udp_host_to_addr("127.0.0.1", 9256);
    __xcheck(msger->addr == NULL);
    msger->laddr = __xapi->udp_host_to_addr("127.0.0.1", msger->lport);
    __xcheck(msger->laddr == NULL);
    
    msger->running = true;
    msger->cb = callback;
    msger->cid = __xapi->clock() % UINT16_MAX;

    msger->send_list.len = 0;
    msger->send_list.head.prev = &msger->send_list.head;
    msger->send_list.head.next = &msger->send_list.head;

    avl_tree_init(&msger->peers, cid_compare, cid_find, sizeof(struct xchannel), AVL_OFFSET(struct xchannel, node));
    avl_tree_init(&msger->temps, temp_cid_compare, temp_cid_find, sizeof(struct xchannel), AVL_OFFSET(struct xchannel, temp));

    // msger->mpipe = xpipe_create(sizeof(xmsg_t) * 1024, "SEND PIPE");
    // __xcheck(msger->mpipe == NULL);

    msger->mpid = __xapi->thread_create(main_loop, msger);
    __xcheck(msger->mpid == NULL);

    __xlogd("xmsger_create exit\n");

    return msger;

XClean:

    // if (msger->sock[0] > 0){
    //     __xapi->udp_close(msger->sock[0]);
    // }
    // if (msger->sock[1] > 0){
    //     __xapi->udp_close(msger->sock[1]);
    // }
    xmsger_free(&msger);
    __xlogd("xmsger_create failed\n");
    return NULL;
}

static void free_channel(void *val)
{
    xchannel_free((xchannel_ptr)val);
    // avl_tree_remove(&((xchannel_ptr)val)->msger->temp_channels, (xchannel_ptr)val);
}

// static void free_channel_1(void *val)
// {
//     xchannel_free((xchannel_ptr)val);
// }

void xmsger_free(xmsger_ptr *pptr)
{
    __xlogd("xmsger_free enter\n");

    if (pptr && *pptr){

        xmsger_ptr msger = *pptr;
        *pptr = NULL;

        __set_false(msger->running);

        if (msger->lsock > 0 && msger->addr != NULL){
            struct xhead msg;
            msg.flag = XPACK_FLAG_FINAL;
            __xapi->udp_send_local(msger->lsock, msger->addr, &msg, XHEAD_SIZE);
        }

        // if (msger->mpipe){
        //     __xlogd("xmsger_free break mpipe\n");
        //     xpipe_break(msger->mpipe);
        // }

        if (msger->mpid){
            __xlogd("xmsger_free main process\n");
            __xapi->thread_join(msger->mpid);
        }

        xchannel_ptr next, channel = msger->send_list.head.next;
        while (channel != NULL && channel != &msger->send_list.head){
            next = channel->next;
            xchannel_free(channel);
            channel = next;
        }

        __xlogd("xmsger_free peers %lu\n", msger->peers.count);
        avl_tree_clear(&msger->peers, NULL);
        __xlogd("xmsger_free temps %lu\n", msger->temps.count);
        avl_tree_clear(&msger->temps, NULL);

        // if (msger->mpipe){
        //     __xlogd("xmsger_free msg pipe: %lu\n", xpipe_readable(msger->mpipe));
        //     while (xpipe_readable(msger->mpipe) > 0){
        //         xline_t *msg;
        //         xpipe_read(msger->mpipe, &msg, __sizeof_ptr);
        //         if (msg){
        //             xl_free(&msg);
        //         }
        //     }
        //     xpipe_free(&msger->mpipe);
        // }

        if (msger->sock[0] > 0){
            __xapi->udp_close(msger->sock[0]);
        }
        if (msger->sock[1] > 0){
            __xapi->udp_close(msger->sock[1]);
        }
        if (msger->lsock > 0){
            __xapi->udp_close(msger->lsock);
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

bool xchannel_get_keepalive(xchannel_ptr channel)
{
    return channel->keepalive;
}

const char* xchannel_get_host(xchannel_ptr channel)
{
    if (channel->port == 0){
        __xapi->udp_addr_to_host((const __xipaddr_ptr)&channel->addr, channel->ip, &channel->port);
    }
    return channel->ip;
}

uint16_t xchannel_get_port(xchannel_ptr channel)
{
    if (channel->port == 0){
        __xapi->udp_addr_to_host((const __xipaddr_ptr)&channel->addr, channel->ip, &channel->port);
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