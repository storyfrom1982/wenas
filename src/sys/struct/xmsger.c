#include "xmsger.h"


#define __sizeof_ptr    sizeof(void*)

static inline xchannel_ptr xchannel_create(xmsger_ptr msger, __xipaddr_ptr addr)
{
    xchannel_ptr channel = (xchannel_ptr) calloc(1, sizeof(struct xchannel));
    channel->connected = false;
    // channel->bye = false;
    channel->breaker = false;
    channel->update = __xapi->clock();
    channel->msger = msger;
    channel->addr = *addr;
    channel->msgbuf = (xmsgbuf_ptr) calloc(1, sizeof(struct xmsgbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->msgbuf->range = PACK_WINDOW_RANGE;
    channel->sendbuf = (xmsgpackbuf_ptr) calloc(1, sizeof(struct xmsgpackbuf) + sizeof(xmsgpack_ptr) * PACK_WINDOW_RANGE);
    channel->sendbuf->range = PACK_WINDOW_RANGE;
    channel->msgqueue = xpipe_create(8 * __sizeof_ptr);
    channel->peer_cid = 0;
    while (xtree_find(msger->peers, &msger->cid, 4) != NULL){
        if (++msger->cid == 0){
            msger->cid = 1;
        }
    }
    channel->cid = msger->cid;
    channel->key = msger->cid % 255;

    // TODO 是否要在创建连接时就加入发送队列，因为这时连接中还没有消息要发送
    xmsger_enqueue_channel(&msger->squeue, channel);
    return channel;
}

static inline void xchannel_free(xchannel_ptr channel)
{
    __xlogd("xchannel_free enter\n");
    __atom_sub(channel->msger->len, channel->len - channel->pos);
    if (channel->sending){
        xmsger_dequeue_channel(&channel->msger->squeue, channel);
    }else {
        xmsger_dequeue_channel(&channel->msger->timed_queue, channel);
    }
    xpipe_free(&channel->msgqueue);
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
                if (channel->msg != NULL){
                    __xlogd("xchannel_pull >>>>------------> msg.rpos: %lu msg.len: %lu pack size: %lu\n", channel->msg->rpos, channel->msg->len, pack->head.pack_size);
                    channel->msg->rpos += pack->head.pack_size;
                    if (channel->msg->rpos == channel->msg->len){
                        //TODO 通知上层，消息发送完成
                        channel->msger->listener->onMessageToPeer(channel->msger->listener, channel, channel->msg->addr);
                        free(channel->msg);
                        if (xpipe_readable(channel->msgqueue) > 0){
                            __xlogd("xchannel_pull >>>>------------> next msg\n");
                            __xbreak(xpipe_read(channel->msgqueue, &channel->msg, __sizeof_ptr) != __sizeof_ptr);
                            __xbreak(xpipe_write(channel->msger->spipe, &channel->msg, __sizeof_ptr) != __sizeof_ptr);
                        }else {
                            __xlogd("xchannel_pull >>>>------------> msg receive finished\n");
                            channel->msg = NULL;
                            xchannel_pause(channel);
                        }
                        // 写线程可以一次性（buflen - 1）个 pack
                    }else if(channel->msg->wpos < channel->msg->len && __transbuf_readable(channel->sendbuf) < (channel->sendbuf->range >> 1)){
                        
                        __xbreak(xpipe_write(channel->msger->spipe, &channel->msg, __sizeof_ptr) != __sizeof_ptr);
                    }
                }
                __xlogd("xchannel_pull >>>>------------------------------------> channel len: %lu msger len %lu\n", channel->len - 0, channel->msger->len - 0);

                if (!pack->comfirmed){
                    pack->comfirmed = true;
                }else {
                    // 这里可以统计收到重复 ACK 的次数
                }

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
                    // TODO 计算已经发送但还没收到ACK的pack的个数，
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
    int result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);

    // 判断发送是否成功
    if (result == PACK_HEAD_SIZE + pack->head.pack_size){
        // 缓冲区下标指向下一个待发送 pack
        channel->sendbuf->upos++;
        // 判断当前 pack 是否为当前连接的消息队列中的最后一个消息
        if (xpipe_readable(channel->msgqueue) == 0 && (channel->msg == NULL || channel->msg->range == 1)){
            // 冲洗一次
            pack->flushing = true;
            __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
            // 记录当前时间
            pack->timestamp = __xapi->clock();
            // 加入冲洗队列
            pack->next = &channel->msger->flushlist.end;
            pack->next->prev = pack;
            pack->prev = channel->msger->flushlist.end.prev;
            pack->prev->next = pack;
            channel->msger->flushlist.len ++;
        }

    }else {
        __xlogd("xchannel_send >>>>------------------------> send failed\n");
    }
}

static inline void xchannel_recv(xchannel_ptr channel, xmsgpack_ptr unit)
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

        if (channel->msgbuf->buf[index]->head.pack_range > 0){
            if (channel->msg == NULL){
                // 收到消息的第一个包，创建 msg，记录范围
                channel->msgbuf->pack_range = channel->msgbuf->buf[index]->head.pack_range;
                assert(channel->msgbuf->pack_range != 0 && channel->msgbuf->pack_range <= XMSG_PACK_RANGE);
                channel->msg = (xmsg_ptr)malloc(sizeof(struct xmsg) + (channel->msgbuf->pack_range * PACK_BODY_SIZE));
                channel->msg->channel = channel;
                channel->msg->wpos = 0;
            }
            __xlogd("xchannel_recv >>>>--------------------------------------------------------------------> pos %u range %u\n", channel->msg->wpos, channel->msgbuf->buf[index]->head.pack_range);
            mcopy(channel->msg->data + channel->msg->wpos, 
                channel->msgbuf->buf[index]->body, 
                channel->msgbuf->buf[index]->head.pack_size);
            channel->msg->wpos += channel->msgbuf->buf[index]->head.pack_size;
            channel->msgbuf->pack_range--;
            if (channel->msgbuf->pack_range == 0){
                channel->msger->listener->onMessageFromPeer(channel->msger->listener, channel, channel->msg);
                channel->msg = NULL;
            }
        }

        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }

    __xlogd("xchannel_recv >>>>------------> exit\n");
}


static void* send_loop(void *ptr)
{
    xmsg_ptr msg;
    xmsgpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        if (__is_false(msger->working)){
            __xlogd("recv_loop ############################# udp_listen enter\n");
            __set_true(msger->readable);
            __xapi->udp_listen(msger->sock);
            __set_false(msger->readable);
            __xlogd("recv_loop ############################# udp_listen exit\n");
            // __xapi->mutex_lock(msger->mtx);
            __xapi->mutex_notify(msger->mtx);
            // __xapi->mutex_unlock(msger->mtx);            
        }
        
        if (xpipe_readable(msger->spipe) > 0)
        {
            __xlogd("recv_loop ############################# xpipe_read enter\n");
            __xbreak(xpipe_read(msger->spipe, &msg, sizeof(void*)) != sizeof(void*));
            __xlogd("recv_loop ############################# xpipe_read exit\n");

            while (msg->wpos < msg->len)
            {
                if (__transbuf_writable(msg->channel->sendbuf) == 0){
                    break;
                }
                pack = make_pack(msg->channel, XMSG_PACK_MSG);
                if (msg->len - msg->wpos < PACK_BODY_SIZE){
                    pack->head.pack_size = msg->len - msg->wpos;
                }else{
                    pack->head.pack_size = PACK_BODY_SIZE;
                }
                pack->head.pack_range = msg->range;
                mcopy(pack->body, msg->addr + msg->wpos, pack->head.pack_size);
                xchannel_push(msg->channel, pack);
                msg->wpos += pack->head.pack_size;
                msg->range --;
                __xlogd("recv_loop ############################# send range %u\n", msg->range);
            }

        }else {

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
    uint64_t duration = UINT64_MAX;
    xmsger_ptr msger = (xmsger_ptr)ptr;
    
    xmsgpack_ptr rpack = NULL;
    xmsgpack_ptr sendpack = NULL;
    xheapnode_ptr timenode;
    xmsgpack_ptr sendunit = NULL;
    xchannel_ptr channel = NULL;
    xchannel_ptr next = NULL;
    void *readable = NULL;

    struct __xipaddr addr;
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 0, &addr));

    while (__is_true(msger->running))
    {
        // 判断是否有待发送数据和待接收数据
        if (msger->len - msger->pos == 0 && __is_false(msger->readable)){
            // 没有数据可收发
            __xlogd("xmsger_loop >>>>-------------> noting to do\n");

            __xapi->mutex_lock(msger->mtx);
            // 主动发送消息时，会通过判断这个值来检测主线程是否在工作
            __set_false(msger->working);
            // 休息一段时间
            __xapi->mutex_timedwait(msger->mtx, duration);
            // 设置工作状态
            __set_true(msger->working);
            // 设置最大睡眠时间，如果有需要定时重传的 pack，这个时间值将会被设置为，最近的重传时间
            duration = UINT64_MAX;
            __xapi->mutex_unlock(msger->mtx);

            __xlogd("xmsger_loop >>>>-------------> start working\n");
        }

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
                                xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_FINAL);
                                xchannel_push(channel, spack);
                                xchannel_recv(channel, rpack);
                                // 释放连接，结束超时重传
                                msger->listener->onDisconnection(msger->listener, channel);

                            }else {
                                __xlogd("xmsger_loop not breaker\n");
                                // 被动方，收到 BEY，需要回一个 BEY，并且启动超时重传
                                xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_BYE);
                                // 启动超时重传
                                xchannel_push(channel, spack);
                                xchannel_recv(channel, rpack);

                            }

                        }else if (rpack->head.type == XMSG_PACK_FINAL){
                            __xlogd("xmsger_loop receive FINAL\n");
                            //被动方收到 FINAL，释放连接，结束超时重传。
                            msger->listener->onDisconnection(msger->listener, channel);

                        }else if (rpack->head.type == XMSG_PACK_PING){
                            __xlogd("xmsger_loop receive PING\n");
                            // 回复 PONG
                            xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PONG);
                            xchannel_push(channel, spack);
                            xchannel_recv(channel, rpack);

                        }else if (rpack->head.type == XMSG_PACK_PONG){
                            __xlogd("xmsger_loop receive PONG\n");
                            rpack->head.cid = channel->peer_cid;
                            rpack->head.x = XMSG_VAL ^ channel->peer_key;
                            xchannel_recv(channel, rpack);
                        }

                    }

                } else {

                    __xlogd("xmsger_loop cannot fond channel\n");

                    if (rpack->head.type == XMSG_PACK_PING){

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

                                xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PONG);
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
            if (msg->channel != NULL){
                // 连接已经存在，开始发送消息
                __xlogd("xmsger_loop >>>>-------------> send msg to peer\n");

                // 判断是否有正在发送的消息
                if (channel->msg == NULL){
                    // 设置为当前正在发送的消息
                    channel->msg = msg;
                }else {
                    // 加入待发送队列
                    __xbreak(xpipe_write(channel->msgqueue, &msg, __sizeof_ptr) != __sizeof_ptr);
                }

                // 将消息放入发送管道，交给发送线程进行分片
                __xbreak(xpipe_write(msger->spipe, &msg, __sizeof_ptr) != __sizeof_ptr);

                // 这里要检查，通道是否在发送队列中
                if (!msg->channel->sending){
                    // 重新加入发送队列，并且从待回收队列中移除
                    xchannel_resume(channel);
                }                


            }else {

                // 连接不存在，创建新连接
                __xlogd("xmsger_loop >>>>-------------> create channel to peer\n");

                xchannel_ptr channel = xchannel_create(msger, (__xipaddr_ptr)msg->addr);
                __xbreak(channel == NULL);

                // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                xtree_save(msger->peers, &channel->addr.port, channel->addr.keylen, channel);
                xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PING);
                // 建立连接时，cid 必须是 0
                spack->head.cid = 0;
                // 这里是协议层验证
                spack->head.x = (XMSG_VAL ^ XMSG_KEY);
                *((uint32_t*)(spack->body)) = channel->cid;
                // 将此刻时钟做为连接的唯一标识
                *((uint64_t*)(spack->body + 4)) = __xapi->clock();
                spack->head.pack_size = 12;
                __atom_add(channel->len, spack->head.pack_size);
                // PING消息不需要分片，所以不需要移交给发送线程
                xchannel_push(channel, spack);
                // 因为是线程间传递地址信息，所以用了指针，这里要释放内存
                if (msg->addr){
                    // 这里指向 xipaddr 分配的内存地址
                    free(msg->addr);
                }
                free(msg);

            }
                        
        }

        // 判断冲洗列表长度
        if (msger->flushlist.len > 0){

            // 取出第一个 pack
            sendpack = msger->flushlist.head.next;

            // 计算是否需要重传
            if ((countdown = 100000000UL - (__xapi->clock() - sendpack->timestamp)) > 0) {
                // 未超时
                if (duration > countdown){
                    // 超时时间更近，更新休息时间
                    // TODO 这个休息时长，要减掉从这时到需要休息时期间耗费的时间
                    duration = countdown;
                }

            }else {

                // 需要重传
                while (sendpack != &msger->flushlist.end)
                {
                    // 当前 pack 的 next 指针有可能会改变，所以要先记录下来
                    xmsgpack_ptr next = sendpack->next;

                    if ((countdown = 100000000UL - (__xapi->clock() - sendpack->timestamp)) > 0) {
                        // 未超时
                        if (duration > countdown){
                            // 超时时间更近，更新休息时间
                            // TODO 这个休息时长，要减掉从这时到需要休息时期间耗费的时间
                            duration = countdown;
                        }
                        // 最近的重传时间还没有到，后没的 pack 也不需要重传
                        break;
                    }


                    result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(sendpack->head), PACK_HEAD_SIZE + sendpack->head.pack_size);

                    // 判断发送是否成功
                    if (result == PACK_HEAD_SIZE + sendpack->head.pack_size){
                        // 暂时移出冲洗列表
                        sendpack->prev->next = sendpack->next;
                        sendpack->next->prev = sendpack->prev;

                        // 只是把 pack 放到列表的最后面，不需要更新时间戳
                        // 如果更新时间戳，就会像 TCP 一样累计延迟
                        // sendpack->timestamp = __xapi->clock();

                        // 加入定时列表
                        sendpack->next = &msger->flushlist.end;
                        sendpack->next->prev = sendpack;
                        sendpack->prev = msger->flushlist.end.prev;
                        sendpack->prev->next = sendpack;

                    }else {
                        __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                    }

                    sendpack = next;
                }
            }            
        }

        // 判断待发送队列中是否有内容
        if (msger->squeue.len > 0){

            // TODO 如能能更平滑的发送
            // 从头开始，每个连接发送一个 pack
            channel = msger->squeue.head.next;

            while (channel != &msger->squeue.end)
            {
                // TODO 如能能更平滑的发送，这里是否要循环发送，知道清空缓冲区？
                // 判断缓冲区中是否有可发送 pack
                if (__transbuf_usable(channel->sendbuf) > 0){

                    xchannel_send(channel, channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)]);

                }

                //TODO 确认发送过程中，连接不会被移出队列
                channel = channel->next;
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

    msg->rpos = 0;
    msg->wpos = 0;
    msg->len = size;
    msg->addr = data;
    msg->channel = channel;
    channel->msg = msg;
    __xlogd("xmsger_send 1\n");;

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


bool xmsger_connect(xmsger_ptr msger, const char *host, uint16_t port)
{
    __xlogd("xmsger_connect enter\n");

        xmsg_ptr msg = (xmsg_ptr)malloc(sizeof(struct xmsg));
    __xbreak(msg == NULL);

    msg->addr = (__xipaddr_ptr)malloc(sizeof(struct __xipaddr));
    __xbreak(msg->addr == NULL);

    __xbreak(!__xapi->udp_make_ipaddr(host, port, (__xipaddr_ptr)msg->addr));
    
    msg->type = XMSG_DGRAM;
    msg->channel = NULL;
    
    __atom_add(msger->len, 12);
    __xbreak(xpipe_write(msger->mpipe, &msg, sizeof(void*)) != sizeof(void*));

    if (__is_false(msger->working)){
        __xlogd("xmsger_connect notify\n");
        __xapi->mutex_notify(msger->mtx);
    }

    __xlogd("xmsger_connect exit\n");

    return true;

Clean:

    if (msg){
        if (msg->addr){
            free(msg->addr);
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

    msger->squeue.len = 0;
    msger->squeue.head.prev = NULL;
    msger->squeue.end.next = NULL;
    msger->squeue.head.next = &msger->squeue.end;
    msger->squeue.end.prev = &msger->squeue.head;

    msger->timed_queue.len = 0;
    msger->timed_queue.head.prev = NULL;
    msger->timed_queue.end.next = NULL;
    msger->timed_queue.head.next = &msger->timed_queue.end;
    msger->timed_queue.end.prev = &msger->timed_queue.head;
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

        if (msger->sock > 0){
            __xapi->udp_close(msger->sock);
        }

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

        free(msger);
    }

    __xlogd("xmsger_free exit\n");
}