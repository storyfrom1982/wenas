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
    bool is_confirm;
    bool is_flushing;
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

typedef struct xpackbuf {
    uint8_t range;
    __atom_size upos, rpos, wpos;
    struct xpack *buf[1];
}*xpackbuf_ptr;

typedef struct xmsgbuf {
    // struct xmessage msg;
    // uint16_t pack_range;
    uint8_t range, upos, rpos, wpos;
    struct xpack *buf[1];
}*xmsgbuf_ptr;


struct xchannel {
    uint32_t cid;
    uint8_t key;
    uint32_t peer_cid;
    uint8_t peer_key;
    uint64_t timestamp;
    uint64_t update;
    bool ping;
    bool sending;
    bool breaker;
    bool connected; // 用peer_cid来标志是否连接
    __atom_size pos, len;
    struct __xipaddr addr;
    struct xhead ack;
    xmsgbuf_ptr msgbuf;
    xpackbuf_ptr sendbuf;
    xmsger_ptr msger;
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
    xpipe_ptr mpipe, spipe, rpipe;
    __xprocess_ptr mpid, rpid, spid;
    // 定时队列是自然有序的，不需要每次排序，所以使用链表效率最高
    struct xpacklist flushlist;
    struct xchannellist send_list, recv_list, ping_list;
};

#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - (b)->wpos + (b)->rpos))

#define XMSG_KEY    'x'
#define XMSG_VAL    'X'

#define __sizeof_ptr    sizeof(void*)


#define __xchannel_enqueue(que, ch) \
    (ch)->next = &((que)->end); \
    (ch)->prev = (que)->end.prev; \
    (ch)->next->prev = (ch); \
    (ch)->prev->next = (ch); \
    (que)->len++

#define __xchannel_dequeue(que, ch) \
    (ch)->prev->next = (ch)->next; \
    (ch)->next->prev = (ch)->prev; \
    (que)->len--

#define __xchannel_keep_alive(que, ch) \
    (ch)->prev->next = (ch)->next; \
    (ch)->next->prev = (ch)->prev; \
    (ch)->next = &(que)->end; \
    (ch)->prev = (que)->end.prev; \
    (ch)->next->prev = (ch); \
    (ch)->prev->next = (ch)

#define __xchannel_stop_sending(ch) \
    (ch)->sending = false; \
    __xchannel_dequeue(&(ch)->msger->send_list, (ch)); \
    if ((ch)->ping) {__xchannel_enqueue(&(ch)->msger->ping_list, (ch));}\
    else {__xchannel_enqueue(&(ch)->msger->recv_list, (ch));} \

#define __xchannel_start_sending(ch) \
    (ch)->sending = true; \
    if ((ch)->ping) {__xchannel_dequeue(&(ch)->msger->ping_list, (ch));}\
    else {__xchannel_dequeue(&(ch)->msger->recv_list, (ch));} \
    __xchannel_enqueue(&(ch)->msger->send_list, (ch))

#define __xchannel_serial_send(ch, pak) \
    (pak)->channel = (ch); \
    (pak)->is_confirm = false; \
    (pak)->is_flushing = false; \
    (pak)->head.flag = 0; \
    (pak)->head.cid = (ch)->peer_cid; \
    (pak)->head.key = (XMSG_VAL ^ (ch)->peer_key); \
    (pak)->head.sn = (ch)->sendbuf->wpos; \
    (ch)->sendbuf->buf[__transbuf_wpos((ch)->sendbuf)] = (pak); \
    __atom_add((ch)->sendbuf->wpos, 1)



static inline bool xchannel_enqueue_message(xchannel_ptr channel, xmessage_ptr msg)
{
    msg->rpos = 0;
    msg->wpos = 0;
    
    // 更新连接的发送计数
    __atom_add(msg->channel->len, msg->len);

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

        // 重新加入发送队列，并且从待回收队列中移除
        // TODO 用宏替换，判断加入队
        if (!msg->channel->sending){
            __xchannel_start_sending(msg->channel);
        }

        if (msg->type == XMSG_PACK_BYE){
            __xbreak(xtree_take(channel->msger->peers, &channel->cid, 4) == NULL);
            __xbreak(xtree_save(channel->msger->peers, &channel->addr.port, channel->addr.keylen, channel) == NULL);
        }

        // 将消息放入发送管道，交给发送线程进行分片
        __xbreak(xpipe_write(channel->msger->spipe, &msg, __sizeof_ptr) != __sizeof_ptr);
        
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

    
    __atom_add(channel->msglist_len, 1);

    __xlogd("xchannel_enqueue_message >>>>-------------> send_ptr: %p\n", channel->send_ptr);

    return true;

Clean:

    return false;
}

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr, bool ping)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    __xbreak(channel == NULL);
    channel->send_ptr = NULL;
    channel->connected = false;
    channel->sending = false;
    channel->breaker = false;
    channel->ping = ping;
    channel->update = __xapi->clock();
    channel->timestamp = __xapi->clock();
    channel->msger = msger;
    channel->addr = *addr;
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xpack_ptr) * PACK_WINDOW_RANGE);
    __xbreak(channel->msgbuf == NULL);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xpackbuf_ptr) calloc(1, sizeof(struct xpackbuf) + sizeof(xpack_ptr) * PACK_WINDOW_RANGE);
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

    if (channel->ping){
        __xchannel_enqueue(&msger->ping_list, channel);
    }else {
        // 所有新创建的连接，都先进入接收队列，同时出入保活状态，检测到超时就会被释放
        __xchannel_enqueue(&msger->recv_list, channel);
    }

    return channel;

    Clean:

    if (channel){
        if (channel->msgbuf){
            free(channel->msgbuf);
        }        
        if (channel->sendbuf){
            free(channel->sendbuf);
        }
        free(channel);
    }

    return NULL;
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    __atom_sub(channel->msger->len, channel->len - channel->pos);
    if (channel->sending){
        __xchannel_dequeue(&channel->msger->send_list, channel);
    }else {
        __xchannel_dequeue(&channel->msger->recv_list, channel);
    }
    // TODO 释放 msg queue 中的资源
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    __xlogd("xchannel_free exit\n");
}

static inline bool xchannel_confirm_pack(xchannel_ptr channel, xpack_ptr rpack)
{
    __xlogd("xchannel_confirm_pack >>>>------------> range: %u sn: %u rpos: %u upos: %u wpos %u\n",
           rpack->head.acks, rpack->head.ack, channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

    // 只处理 sn 在 rpos 与 upos 之间的 xpack
    if (__transbuf_inuse(channel->sendbuf) > 0 && ((uint8_t)(rpack->head.ack - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos))){

        __xlogd("xchannel_confirm_pack >>>>------------> in range\n");

        uint8_t index;
        xpack_ptr pack;

        // 顺序，收到第一个 PACK 的 ACK 时，ack 和 acks 都是 1
        // 错序，先收到第二个 PACK 的 ACK 时，ack = 1，acks = 0

        // 对端设置 ack 等于 acks 时，证明对端已经收到了 acks 之前的所有 PACK
        if (rpack->head.ack == rpack->head.acks){
            __xlogd("xchannel_confirm_pack >>>>------------> in serial\n");
            do {
                __xlogd("xchannel_confirm_pack >>>>------------------------------------> 0.0\n");
                //TODO 计算往返延时
                //TODO 统计丢包率
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                // 检测 rpos 越界，调试用
                // assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                pack = channel->sendbuf->buf[index];

                // 数据已送达，从待发送数据中减掉这部分长度
                __atom_add(channel->pos, pack->head.len);
                __atom_add(channel->msger->pos, pack->head.len);

                // 更新已经到达对端的数据计数
                pack->msg->rpos += pack->head.len;
                if (pack->msg->rpos == pack->msg->len){
                    // 把已经传送到对端的 msg 交给发送线程处理
                    __xbreak(xpipe_write(channel->msger->spipe, &pack->msg, __sizeof_ptr) != __sizeof_ptr);
                }

                __xlogd("xchannel_confirm_pack >>>>------------------------------------> channel len: %lu msger len %lu\n", channel->len - 0, channel->msger->len - 0);

                if (!pack->is_confirm){
                    pack->is_confirm = true;
                }else {
                    // 这里可以统计收到重复 ACK 的次数
                }

                // 从定时队列中移除
                if (pack->is_flushing){
                    pack->next->prev = pack->prev;
                    pack->prev->next = pack->next;
                    channel->msger->flushlist.len --;
                }

                __xlogd("xchannel_confirm_pack >>>>------------------------------------> 0.1\n");

                // 释放内存
                free(pack);

                __xlogd("xchannel_confirm_pack >>>>------------------------------------> 0.2\n");

                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                __atom_add(channel->sendbuf->rpos, 1);

                __xlogd("xchannel_confirm_pack >>>>------------------------------------> rpos: %u  ack: %u\n", __transbuf_rpos(channel->sendbuf), (rpack->head.ack & (channel->sendbuf->range - 1)));

                // rpos 一直在 acks 之前，一旦 rpos 等于 acks，所有连续的 ACK 就处理完成了
            } while (__transbuf_rpos(channel->sendbuf) != (rpack->head.ack & (channel->sendbuf->range - 1)));

            __xlogd("xchannel_confirm_pack >>>>------------------------------------> 1\n");

            // 判断是否有消息要发送
            if(channel->send_ptr != NULL){
                
                if (channel->send_ptr->range == 0){
                    channel->send_ptr = channel->send_ptr->next;
                }

                // 判断是否有消息要发送
                if(channel->send_ptr != NULL 
                    && channel->send_ptr->type != XMSG_PACK_BYE
                    // 消息可以一次性发送完成
                    && (channel->send_ptr->range <= __transbuf_writable(channel->sendbuf) 
                    // 发送缓冲区不少于一半可写空间
                    || __transbuf_writable(channel->sendbuf) >= (channel->sendbuf->range >> 1))){
                        __xlogd("xchannel_confirm_pack >>>>------------------------------------> 2\n");
                    // 通知发送线程进行分片
                    __xbreak(xpipe_write(channel->msger->spipe, &channel->send_ptr, __sizeof_ptr) != __sizeof_ptr);
                }
            }

        } else {

            __xlogd("xchannel_confirm_pack >>>>------------------------------------> 3\n");

            // __logi("xchannel_confirm_pack recv interval ack: %u acks: %u", ack->head.ack, ack->head.acks);
            index = rpack->head.ack & (PACK_WINDOW_RANGE - 1);
            pack = channel->sendbuf->buf[index];
            pack->is_confirm = true;

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            index = channel->sendbuf->rpos;
            while (index != rpack->head.sn) {
                pack = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                if (!pack->is_confirm){
                    // TODO 计算已经发送但还没收到ACK的pack的个数
                    // TODO 这里需要处理超时断开连接的逻辑
                    __xlogd("xchannel_confirm_pack >>>>------------------------------------> resend pack: %u\n", pack->head.sn);
                    int result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len);
                    if (result != PACK_HEAD_SIZE + pack->head.len){
                        __xlogd("xchannel_confirm_pack >>>>------------------------> send failed\n");
                        break;
                    }
                }
                index++;
            }
        }

    }else {

        __xlogd("xchannel_confirm_pack >>>>------------------------------------> out of range\n");

    }

    return true;

    Clean:

    return false;
}

static inline void xchannel_send_pack(xchannel_ptr channel, xpack_ptr pack)
{
    __xlogd("xchannel_send_pack >>>>------------------------> enter\n");

    int result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len);

    __xlogd("xchannel_send_pack >>>>------------------------> check ptr: %p\n", channel->send_ptr);

    // 判断发送是否成功
    if (result == PACK_HEAD_SIZE + pack->head.len){
        // 缓冲区下标指向下一个待发送 pack
        channel->sendbuf->upos++;

        __xlogd("xchannel_send_pack >>>>------------------------> send_ptr %p\n", channel->send_ptr);

        // 判断当前 msg 是否为当前连接的消息队列中的最后一个消息
        if (pack->head.range == 1 && __transbuf_usable(channel->sendbuf) == 0 && channel->msglist_len == 0){
            __xlogd("xchannel_send_pack >>>>------------------------> enqueue flushing\n");
            // 冲洗一次
            pack->is_flushing = true;
            // __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.len);
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
        __xlogd("xchannel_send_pack >>>>------------------------> send failed\n");
    }

    __xlogd("xchannel_send_pack >>>>------------------------> exit\n");
}

static inline bool xchannel_serial_recv(xchannel_ptr channel, xpack_ptr pack)
{
    __xlogd("xchannel_serial_recv >>>>------------> enter\n");
    __xlogd("xchannel_serial_recv >>>>------------> SN: %u rpos: %u wpos: %u\n",
           pack->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);

    channel->ack.type = XMSG_PACK_ACK;
    channel->ack.flag = pack->head.type;
    channel->ack.cid = pack->head.cid;
    channel->ack.key = pack->head.key;

    if (pack->head.flag){
        __xlogd("xchannel_serial_recv >>>>------------> MSG and ACK\n");
        // 如果 PACK 携带了 ACK，就在这里统一回收发送缓冲区
        xchannel_confirm_pack(channel, pack);
    }

    uint16_t index = pack->head.sn & (PACK_WINDOW_RANGE - 1);

    // 如果收到连续的 PACK
    if (pack->head.sn == channel->msgbuf->wpos){

        __xlogd("xchannel_serial_recv >>>>------------> serial\n");

        pack->channel = channel;
        __xlogd("xchannel_serial_recv >>>>------------> pack->channel %p\n", pack->channel);
        // 保存 PACK
        channel->msgbuf->buf[index] = pack;
        // 更新最大连续 SN
        channel->msgbuf->wpos++;

        // 收到连续的 ACK 就不会回复的单个 ACK 确认了        
        channel->ack.acks = channel->msgbuf->wpos;
        // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
        channel->ack.ack = channel->ack.acks;

        // 如果之前有为按顺序到达的 PACK 需要更新
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL)
        {
            channel->msgbuf->wpos++;
            // 这里需要更新将要回复的最大连续 ACK
            channel->ack.acks = channel->msgbuf->wpos;
            // 设置 ack 等于 acks 通知对端，acks 之前的 PACK 已经全都收到
            channel->ack.ack = channel->ack.acks;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->msgbuf->wpos - pack->head.sn) > (uint8_t)(pack->head.sn - channel->msgbuf->wpos)){

            __xlogd("xchannel_serial_recv >>>>------------------------> early\n");

            // SN 在 wpos 方向越界，是提前到达的 PACK

            // 设置将要回复的单个 ACK
            channel->ack.ack = pack->head.sn;
            // 设置将要回复的最大连续 ACK，这时 ack 一定会大于 acks
            channel->ack.acks = channel->msgbuf->wpos;

            // 这里 wpos - 1 在 wpos 等于 0 时会造成 acks 的值是 255
            // channel->ack.acks = channel->msgbuf->wpos - 1;
            
            if (channel->msgbuf->buf[index] == NULL){
                pack->channel = channel;
                // 这个 PACK 首次到达，保存 PACK
                channel->msgbuf->buf[index] = pack;
            }else {
                // 这个 PACK 重复到达，释放 PACK
                free(pack);
            }
            
        }else {

            __xlogd("xchannel_serial_recv >>>>------------------------> again\n");
            
            // SN 在 rpos 方向越界，是滞后到达的 PACK，发生了重传
            // 回复 ACK 等于 ACKS，通知对端包已经收到
            channel->ack.acks = channel->msgbuf->wpos;
            channel->ack.ack = channel->ack.acks;
            // 释放 PACK
            free(pack);
        }
    }


    if (__transbuf_usable(channel->sendbuf) > 0){

        // 取出当前要发送的 pack
        pack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
        __xlogd("xchannel_serial_recv >>>>------------> ACK and MSG\n");
        pack->head.flag = channel->ack.type;
        pack->head.ack = channel->ack.ack;
        pack->head.acks = channel->ack.acks;
        xchannel_send_pack(channel, pack);

    }else {

        __xlogd("xchannel_send_pack >>>>------------> ACK\n");
        if ((__xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&channel->ack, PACK_HEAD_SIZE)) != PACK_HEAD_SIZE){
            __xlogd("xchannel_send_pack >>>>------------------------> failed\n");
        }
    }


    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL)
    {
        __xlogd("xchannel_serial_recv >>>>------------> pack->channel %p\n", channel->msgbuf->buf[index]->channel);
        // 交给接收线程来处理 pack
        __xbreak(xpipe_write(channel->msger->rpipe, &channel->msgbuf->buf[index], __sizeof_ptr) != __sizeof_ptr);
        // 收到一个完整的消息，需要判断是否需要更新保活
        if (channel->msgbuf->buf[index]->head.range == 1 && !channel->sending){
            // 判断队列是否有多个成员
            if (channel->msger->recv_list.len > 1){
                __xchannel_keep_alive(&channel->msger->recv_list, channel);
            }
            // 更新时间戳
            channel->update = __xapi->clock();
        }
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }

    __xlogd("xchannel_serial_recv >>>>------------> exit\n");

    return true;

    Clean:

    return false;
}


static void* send_loop(void *ptr)
{
    __xlogd("send_loop enter\n");

    xmessage_ptr msg;
    xpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        __xbreak(xpipe_read(msger->spipe, &msg, __sizeof_ptr) != __sizeof_ptr);
        
        // 判断消息是否全部送达
        if (msg->rpos == msg->len){
            // 通知用户，消息已经送达
            msg->channel->msger->listener->onMessageToPeer(msg->channel->msger->listener, msg->channel, msg->data);
            __xlogd("send_loop 1\n");
            free(msg);
            __xlogd("send_loop 2\n");
            continue;
        }
        
        while (__transbuf_writable(msg->channel->sendbuf) > 0)
        {
            if (msg->wpos < msg->len){
                // pack = xpack_new(msg->channel, msg->type);
                pack = (xpack_ptr)malloc(sizeof(struct xpack));
                __xbreak(pack == NULL);

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

                __xchannel_serial_send(msg->channel, pack);

            }else {

                __atom_sub(msg->channel->msglist_len, 1);
                break;
            }
        }
    }

    __xlogd("send_loop exit\n");

Clean:    

    return NULL;
}


static void* main_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    int result;
    xmessage_ptr msg;
    int64_t countdown;
    uint64_t duration = UINT32_MAX;
    xmsger_ptr msger = (xmsger_ptr)ptr;
    
    xpack_ptr rpack = NULL;
    xpack_ptr sendpack = NULL;
    xpack_ptr sendunit = NULL;
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
                rpack = (xpack_ptr)malloc(sizeof(struct xpack));
                __xbreak(rpack == NULL);
            }

            rpack->head.type = 0;
            rpack->head.len = 0;
            result = __xapi->udp_recvfrom(msger->sock, &addr, &rpack->head, PACK_ONLINE_SIZE);
            __xlogd("xmsger_loop udp_recvfrom %ld\n", result);
            if (result == (rpack->head.len + PACK_HEAD_SIZE)){

                __xlogd("xmsger_loop recv ip: %u port: %u cid: %u msg: %d\n", addr.ip, addr.port, rpack->head.cid, rpack->head.type);

                channel = (xchannel_ptr)xtree_find(msger->peers, &rpack->head.cid, 4);

                if (channel){

                    __xlogd("xmsger_loop fond channel\n");

                    // 协议层验证
                    if ((rpack->head.key ^ channel->key) == XMSG_VAL){

                        if (rpack->head.type == XMSG_PACK_MSG) {
                            __xlogd("xmsger_loop receive MSG\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.key = (XMSG_VAL ^ channel->peer_key);
                            xchannel_serial_recv(channel, rpack);

                        }else if (rpack->head.type == XMSG_PACK_ACK) {
                            __xlogd("xmsger_loop receive ACK\n");
                            xchannel_confirm_pack(channel, rpack);
                            free(rpack);

                        }else if (rpack->head.type == XMSG_PACK_PING){
                            __xlogd("xmsger_loop receive PING\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.key = (XMSG_VAL ^ channel->peer_key);                            
                            xchannel_serial_recv(channel, rpack);
                            
                        }else if (rpack->head.type == XMSG_PACK_HELLO){
                            __xlogd("xmsger_loop receive HELLO\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.key = (XMSG_VAL ^ channel->peer_key);                            
                            xchannel_serial_recv(channel, rpack);
                            
                        }else if (rpack->head.type == XMSG_PACK_BYE){
                            __xlogd("xmsger_loop receive BYE\n");
                            // 判断是否为主动断开的一方
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.key = (XMSG_VAL ^ channel->peer_key);                            
                            xchannel_serial_recv(channel, rpack);
                            msger->listener->onChannelBreak(msger->listener, channel);
                            // TODO 释放连接

                        }
                    }

                } else {

                    __xlogd("xmsger_loop cannot fond channel\n");

                    if (rpack->head.type == XMSG_PACK_HELLO){

                        __xlogd("xmsger_loop receive HELLO\n");

                        if (rpack->head.cid == 0 && (rpack->head.key ^ XMSG_KEY) == XMSG_VAL){

                            __xlogd("xmsger_loop receive HELLO 0\n");

                            // 这里收到的是对方发起的 HELLO
                            uint32_t peer_cid = *((uint32_t*)(rpack->body));
                            uint64_t timestamp = *((uint64_t*)(rpack->body + 4));

                            channel = (xchannel_ptr)xtree_find(msger->peers, &addr.port, addr.keylen);

                            if (channel == NULL){

                                __xlogd("xmsger_loop receive HELLO 1.1\n");

                                // 这里是对端发起的 HELLO
                                // 回复 HELLO，等待对端回复的 ACK，接收到对端的 ACK，连接建立完成

                                // 创建连接
                                channel = xchannel_create(msger, &addr, false);
                                __xbreak(channel == NULL);

                                // 设置 peer cid
                                // 虽然已经设置了对端的 cid，但是对端无法通过 cid 索引到 channel，因为这时还是 addr 作为索引
                                channel->peer_cid = peer_cid;
                                channel->peer_key = peer_cid % 255;
                                
                                xtree_save(msger->peers, &addr.port, addr.keylen, channel);

                                channel->msgbuf->wpos++;
                                channel->msgbuf->rpos++;

                                // 新创建的 channel 一定要给对端发送一个 HELLO，才能完成连接的创建
                                xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
                                __xbreak(msg == NULL);

                                msg->type = XMSG_PACK_HELLO;
                                msg->sid = 0;
                                msg->channel = channel;
                                msg->range = 1;
                                msg->len = XMSG_CMD_SIZE;
                                msg->data = msg->cmd;
                                
                                *((uint32_t*)(msg->data)) = channel->cid;
                                *((uint64_t*)(msg->data + 4)) = __xapi->clock();

                                // 这是内部生成的消息，所有要自己更新 xmsger 的计数
                                __atom_add(msger->len, XMSG_CMD_SIZE);
                                __xlogd("xmsger_loop receive HELLO 1\n");
                                __xbreak(!xchannel_enqueue_message(channel, msg));

                            }else { // 这里是对穿的 HELLO

                                // 对端会一直发重复送这个 HELLO，直到收到一个 ACK 为止

                                __xlogd("xmsger_loop receive HELLO 2\n");
                                // 设置 peer cid 和校验码
                                channel->peer_cid = peer_cid;
                                channel->peer_key = peer_cid % 255;

                                __xlogd("xmsger_loop receive HELLO 2.1\n");

                                // 后发起的一方负责 PING
                                if (channel->timestamp > timestamp){
                                    if (channel->ping){
                                        channel->ping = false;
                                        __xchannel_dequeue(&channel->msger->ping_list, channel);
                                        __xchannel_enqueue(&channel->msger->recv_list, channel);
                                    }
                                }

                                // 开始用对端的校验码，虽然已经设置了对端的 cid，但是对端无法通过 cid 索引到 channel，因为这时还是 addr 作为索引
                                rpack->head.cid = channel->peer_cid;
                                rpack->head.key = (XMSG_VAL ^ channel->peer_key);

                                // 各自回复 ACK，通知对端连接建立完成
                                xchannel_serial_recv(channel, rpack);                                
                            }

                        } else {

                            // 这里收到的是对方回复的 HELLO

                            // 移除 ipaddr 索引，建立 cid 索引，连接建立完成
                            channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                            if (channel && (rpack->head.key ^ channel->key) == XMSG_VAL){
                                // 开始使用 cid 作为索引
                                xtree_save(msger->peers, &channel->cid, 4, channel);
                                channel->connected = true;
                                //这里是被动建立连接 onChannelFromPeer
                                channel->msger->listener->onChannelToPeer(channel->msger->listener, channel);
                                
                                uint32_t peer_cid = *((uint32_t*)(rpack->body));
                                uint64_t timestamp = *((uint64_t*)(rpack->body + 4));
                                // 设置 peer cid 和校验码
                                channel->peer_cid = peer_cid;
                                channel->peer_key = peer_cid % 255;
                                channel->timestamp = timestamp;

                                // 开始用对端的校验码，虽然已经设置了对端的 cid，但是对端无法通过 cid 索引到 channel，因为这时还是 addr 作为索引
                                rpack->head.cid = channel->peer_cid;
                                rpack->head.key = (XMSG_VAL ^ channel->peer_key);

                                // 收到了对端回复的 HELLO，这里需要更新发送缓冲区，停止 HELLO 的 flushing。因为对端只回复 HELLO，不回复 ACK
                                rpack->head.flag = XMSG_PACK_HELLO;
                                rpack->head.acks = 1;
                                rpack->head.ack = rpack->head.acks;
                                // 回复 ACK 通知对端连接已经建立
                                xchannel_serial_recv(channel, rpack);                                
                            }
                        }


                    }else if (rpack->head.type == XMSG_PACK_BYE){

                        // 连接已经释放了，现在用默认的校验码
                        rpack->head.cid = 0;
                        rpack->head.key = (XMSG_VAL ^ XMSG_KEY);
                        xchannel_serial_recv(channel, rpack);

                    }else if (rpack->head.type == XMSG_PACK_ACK){
                        
                        __xlogd("xmsger_loop receive ACK\n");

                        if (rpack->head.flag == XMSG_PACK_HELLO){

                            channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                            // HELLO 的 ACK 都是生成的校验码
                            if (channel && (rpack->head.key ^ channel->key) == XMSG_VAL){
                                // 开始使用 cid 作为索引
                                xtree_save(msger->peers, &channel->cid, 4, channel);
                                // 停止发送 HELLO
                                xchannel_confirm_pack(channel, rpack);
                                channel->connected = true;
                                //这里是被动建立连接 onChannelFromPeer
                                channel->msger->listener->onChannelFromPeer(channel->msger->listener, channel);
                            }

                        }else if (rpack->head.flag == XMSG_PACK_BYE){

                            channel = (xchannel_ptr)xtree_take(msger->peers, &addr.port, addr.keylen);
                            // HELLO 的 ACK 都是生成的校验码
                            if (channel && ((rpack->head.key ^ channel->key) == XMSG_VAL || (rpack->head.key ^ XMSG_KEY) == XMSG_VAL)){
                                channel->msger->listener->onChannelBreak(channel->msger->listener, channel);
                                xchannel_confirm_pack(channel, rpack);
                                // TODO 释放连接
                            }
                        }
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
            if (msg->type != XMSG_PACK_MSG){

                if (msg->type == XMSG_PACK_HELLO){
                    __xlogd("xmsger_loop >>>>-------------> create channel to peer\n");

                    channel = xchannel_create(msger, (__xipaddr_ptr)msg->data, true);
                    __xbreak(channel == NULL);

                    // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                    xtree_save(msger->peers, &channel->addr.port, channel->addr.keylen, channel);

                    // 所有 pack 统一交给发送线程发出，保持有序，在这里先填充好 PING 的内容
                    *((uint32_t*)(msg->data)) = channel->cid;
                    // 将此刻时钟做为连接的唯一标识
                    *((uint64_t*)(msg->data + 4)) = __xapi->clock();

                    msg->channel = channel;

                }else {

                    channel = msg->channel;
                    __xbreak(xtree_take(channel->msger->peers, &channel->cid, 4) == NULL);
                    __xbreak(xtree_save(channel->msger->peers, &channel->addr.port, channel->addr.keylen, channel) == NULL);
                }
            }

            __xlogd("xmsger_loop >>>>-------------> send msg to peer\n");

            __xbreak(!xchannel_enqueue_message(msg->channel, msg));
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
                    xpack_ptr next = sendpack->next;

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

                    result = __xapi->udp_sendto(sendpack->channel->msger->sock, &sendpack->channel->addr, (void*)&(sendpack->head), PACK_HEAD_SIZE + sendpack->head.len);
                    __xlogd("xmsger_loop >>>>-------------> resend 1\n");

                    // 判断发送是否成功
                    if (result == PACK_HEAD_SIZE + sendpack->head.len){
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
        if (msger->send_list.len > 0){

            // __xlogd("xmsger_loop >>>>-------------> send channel enter\n");

            // TODO 如能能更平滑的发送
            // 从头开始，每个连接发送一个 pack
            channel = msger->send_list.head.next;

            // __xlogd("xmsger_loop >>>>-------------> send channel 1\n");

            while (channel != &msger->send_list.end)
            {
                // __xlogd("xmsger_loop >>>>-------------> send channel 2\n");
                // TODO 如能能更平滑的发送，这里是否要循环发送，知道清空缓冲区？
                // 判断缓冲区中是否有可发送 pack
                if (__transbuf_usable(channel->sendbuf) > 0){
                    __xlogd("xmsger_loop >>>>-------------> send channel send_ptr: %p  usable: %u\n", channel->send_ptr, __transbuf_usable(channel->sendbuf));
                    xchannel_send_pack(channel, channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)]);

                }

                //TODO 确认发送过程中，连接不会被移出队列
                channel = channel->next;
            }

            // __xlogd("xmsger_loop >>>>-------------> send channel exit\n");

        }

        // 处理超时
        if (msger->recv_list.len > 0){
            
            __xlogd("xmsger_loop >>>>-------------> check recv list\n");

            channel = msger->recv_list.head.next;
            // 10 秒钟超时
            while (channel != &msger->recv_list.end){
                next = channel->next;

                // TODO 发起端需要发送 PING 保活
                // 对穿连接，时间戳晚的一端负责发 PING 保活
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

        __xlogd("xmsger_loop >>>>-------------> check ping list %lu\n", msger->ping_list.len);

        // 连接保活
        if (msger->ping_list.len > 0){

            channel = msger->ping_list.head.next;
            // 10 秒钟超时
            while (channel != &msger->ping_list.end){
                next = channel->next;

                if (__xapi->clock() - channel->update > NANO_SECONDS * 5){

                    // 新创建的 channel 一定要给对端发送一个 HELLO，才能完成连接的创建
                    xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
                    __xbreak(msg == NULL);

                    msg->type = XMSG_PACK_PING;
                    msg->sid = 0;
                    msg->channel = channel;
                    msg->range = 1;
                    msg->len = XMSG_CMD_SIZE;
                    msg->data = msg->cmd;

                    // 这是内部生成的消息，所有要自己更新 xmsger 的计数
                    __atom_add(msger->len, XMSG_CMD_SIZE);
                    __xlogd("xmsger_loop >>>>-------------> send ping\n");
                    __xbreak(!xchannel_enqueue_message(channel, msg));
                    
                    if (msger->ping_list.len > 1){
                        __xchannel_keep_alive(&msger->ping_list, channel);
                    }
                    // 更新时间戳
                    channel->update = __xapi->clock();

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
    __xlogd("recv_loop enter\n");

    xmessage_ptr msg;
    xpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        __xlogd("recv_loop 1\n");
        __xapi->udp_listen(msger->sock);
        __set_true(msger->readable);
        __xapi->mutex_notify(msger->mtx);
        __xlogd("recv_loop 2\n");
        
        while(xpipe_read(msger->rpipe, &pack, __sizeof_ptr) == __sizeof_ptr)
        {
            if (pack == NULL){
                break;
            }

            __xlogd("recv_loop sid: %lu\n", pack->head.sid);
            msg = &(pack->channel->streams[pack->head.sid]);
            __xlogd("recv_loop msg: %p\n", msg);

            // PING 和 PONG BYE FINAL 的 pack_range 都设置为 0
            if (pack->head.type == XMSG_PACK_MSG && pack->head.range > 0){
                if (msg->data == NULL){
                    __xlogd("recv_loop 4\n");
                    // 收到消息的第一个包，创建 msg，记录范围
                    msg->range = pack->head.range;
                    assert(msg->range != 0 && msg->range <= XMSG_PACK_RANGE);
                    msg->data = (uint8_t*)malloc(msg->range * PACK_BODY_SIZE);
                    msg->wpos = 0;
                }
                __xlogd("recv_loop >>>>--------------------------------------------------------------------> pos %u range %u\n", msg->wpos, pack->head.range);
                mcopy(msg->data + msg->wpos, pack->body, pack->head.len);
                msg->wpos += pack->head.len;
                msg->range--;
                if (msg->range == 0){
                    pack->channel->msger->listener->onMessageFromPeer(pack->channel->msger->listener, pack->channel, msg->data, msg->wpos);
                    msg->data = NULL;
                }
            }

            free(pack);
        }
    }

    __xlogd("recv_loop exit\n");

Clean:

    return NULL;
}


static inline bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t len, uint8_t sid)
{
    __xlogd("xmsger_send enter\n");

    __xbreak(channel == NULL);

    xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_MSG;
    msg->channel = channel;
    msg->sid = sid;
    msg->data = data;
    msg->len = len;

    msg->range = (msg->len / PACK_BODY_SIZE);
    if (msg->range * PACK_BODY_SIZE < msg->len){
        // 有余数，增加一个包
        msg->range ++;
    }    

    __atom_add(msger->len, len);

    __xbreak(xpipe_write(msger->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

    if (__is_false(msger->working)){
        __xlogd("xmsger_send notify\n");
        __xapi->mutex_notify(msger->mtx);
    }

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
    __xlogd("xmsger_disconnect channel 0x%X enter", channel);

    // 避免重复调用
    if (!__set_true(channel->breaker)){
        __xlogd("xmsger_disconnect channel 0x%X repeated calls", channel);
        return true;
    }

    xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_BYE;
    msg->channel = channel;
    msg->sid = 0;
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

    __xlogd("xmsger_disconnect channel 0x%X failed", channel);

    if (msg){
        free(msg);
    }

    return false;

}

bool xmsger_connect(xmsger_ptr msger, const char *host, uint16_t port)
{
    __xlogd("xmsger_connect enter\n");

    xmessage_ptr msg = (xmessage_ptr)malloc(sizeof(struct xmessage));
    __xbreak(msg == NULL);

    msg->type = XMSG_PACK_HELLO;
    msg->channel = NULL;
    msg->sid = 0;
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

    __xlogd("xmsger_connect failed\n");

    if (msg){
        free(msg);
    }
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

    msger->ping_list.len = 0;
    msger->ping_list.head.prev = NULL;
    msger->ping_list.end.next = NULL;
    msger->ping_list.head.next = &msger->ping_list.end;
    msger->ping_list.end.prev = &msger->ping_list.head;

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