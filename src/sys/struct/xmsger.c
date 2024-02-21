#include "xmsger.h"


#define __sizeof_ptr    sizeof(void*)

int64_t xchannel_send(xchannel_ptr channel, xmsghead_ptr ack)
{
    // __xlogd("xchannel_send >>>>------------> enter\n");

    long result;
    xmsgpack_ptr pack;

    if (__transbuf_usable(channel->sendbuf) > 0){
        pack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
        if (ack != NULL){
            __xlogd("xchannel_send >>>>------------> ACK and MSG\n");
            pack->head.y = 1;
            pack->head.ack = ack->ack;
            pack->head.acks = ack->acks;
        }else {
            __xlogd("xchannel_send >>>>------------> MSG\n");
        }
        
        result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            channel->sendbuf->upos++;
            pack->ts.key = __xapi->clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timer, &pack->ts);
            __xlogd("xchannel_send >>>>------------------------> channel timer count %lu\n", channel->timer->pos);
            pack->resending = 0;
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
        }

    }else if (ack != NULL){
        __xlogd("xchannel_send >>>>------------> ACK\n");
        result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)ack, PACK_HEAD_SIZE);
        if (result == PACK_HEAD_SIZE){
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
        }
    }

    while (channel->timer->pos > 0 && (int64_t)(__heap_min(channel->timer)->key - __xapi->clock()) < 0){
        __xlogd("xchannel_send >>>>------------------------------------> pack time out resend %lu %lu\n", __heap_min(channel->timer)->key, __xapi->clock());
        pack = (xmsgpack_ptr)__heap_min(channel->timer)->value;
        result = __xapi->udp_sendto(channel->msger->sock, &pack->channel->addr, (void*)&(pack->head), PACK_HEAD_SIZE + pack->head.pack_size);
        if (result == PACK_HEAD_SIZE + pack->head.pack_size){
            xheap_pop(channel->timer);
            pack->ts.key = __xapi->clock() + TRANSUNIT_TIMEOUT_INTERVAL;
            pack->ts.value = pack;
            xheap_push(channel->timer, &pack->ts);
            __xlogd("xchannel_send >>>>------------------------> channel timer count %lu\n", channel->timer->pos);
            pack->resending ++;
        }else {
            __xlogd("xchannel_send >>>>------------------------> failed\n");
            break;
        }
    }

    if (channel->timer->pos > 0){
        result = (int64_t)__heap_min(channel->timer)->key - __xapi->clock();
    }else {
        result = 0;
    }

    // __xlogd("xchannel_send >>>>------------> exit\n");

    return result;

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
    uint64_t rest_duration = UINT64_MAX;
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

        // 判断是否有待发送数据和待接收数据
        if (msger->len - msger->pos == 0 && __is_false(msger->readable)){
            // 没有数据可收发
            __xlogd("xmsger_loop >>>>-------------> noting to do\n");

            __xapi->mutex_lock(msger->mtx);
            // 主动发送消息时，会通过判断这个值来检测主线程是否在工作
            __set_false(msger->working);
            // 休息一段时间
            __xapi->mutex_timedwait(msger->mtx, rest_duration);
            // 设置工作状态
            __set_true(msger->working);
            // 设置最大睡眠时间，如果有需要定时重传的 pack，这个时间值将会被设置为，最近的重传时间
            rest_duration = UINT64_MAX;
            __xapi->mutex_unlock(msger->mtx);

            __xlogd("xmsger_loop >>>>-------------> start working\n");
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

        // 判断待发送队列中是否有内容
        if (msger->squeue.len > 0){

            // TODO 如能能更平滑的发送
            // 从头开始，每个连接发送一个 pack
            channel = msger->squeue.head.next;

            while (channel != &msger->squeue.end)
            {
                // 先处理超时的 pack
                // 判断定时列表是否有内容
                if (channel->timedlist.len > 0){
                    xmsgpack_ptr next;
                    sendpack = channel->timedlist.head.next;
                    while (sendpack != &channel->timedlist.end)
                    {
                        // 当前 pack 的 next 指针有可能会改变，所以要先记录下来
                        next = sendpack->next;
                        
                        // 计算超时时间
                        if ((countdown = 100000000UL - (__xapi->clock() - sendpack->timestamp)) > 0){
                            // 未超时
                            if (rest_duration > countdown){
                                // 超时时间更近，更新休息时间
                                rest_duration = countdown;
                            }
                            // 最近的重传时间还没有到，后没的 pack 也不需要重传
                            break;

                        }else {

                            result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(sendpack->head), PACK_HEAD_SIZE + sendpack->head.pack_size);

                            // 判断发送是否成功
                            if (result == PACK_HEAD_SIZE + sendpack->head.pack_size){
                                // 暂时移出定时列表
                                sendpack->prev->next = sendpack->next;
                                sendpack->next->prev = sendpack->prev;
                                // 记录重传次数
                                sendpack->resending ++;
                                // 记录当前时间
                                sendpack->timestamp = __xapi->clock();
                                // 加入定时列表
                                sendpack->next = &channel->timedlist.end;
                                sendpack->next->prev = sendpack;
                                sendpack->prev = channel->timedlist.end.prev;
                                sendpack->prev->next = sendpack;

                            }else {
                                __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                            }
                        }

                        sendpack = next;
                    }
                }

                // TODO 如能能更平滑的发送，这里是否要循环发送，知道清空缓冲区？
                // 判断缓冲区中是否有可发送 pack
                if (__transbuf_usable(channel->sendbuf) > 0){

                    // 取出当前要发送的 pack
                    sendpack = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                    
                    result = __xapi->udp_sendto(channel->msger->sock, &channel->addr, (void*)&(sendpack->head), PACK_HEAD_SIZE + sendpack->head.pack_size);

                    // 判断发送是否成功
                    if (result == PACK_HEAD_SIZE + sendpack->head.pack_size){
                        // 缓冲区下标指向下一个待发送 pack
                        channel->sendbuf->upos++;
                        // TODO 研究一下，当初为啥要有这个标志
                        sendpack->resending = 0;
                        // 记录当前时间
                        sendpack->timestamp = __xapi->clock();
                        // 加入定时列表
                        sendpack->next = &channel->timedlist.end;
                        sendpack->next->prev = sendpack;
                        sendpack->prev = channel->timedlist.end.prev;
                        sendpack->prev->next = sendpack;
                        channel->timedlist.len ++;

                    }else {
                        __xlogd("xmsger_loop >>>>------------------------> send failed\n");
                    }
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