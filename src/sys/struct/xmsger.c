#include "xmsger.h"


static void* recv_loop(void *ptr)
{
    xmsg_ptr msg;
    xmsgpack_ptr pack;
    xmsger_ptr msger = (xmsger_ptr)ptr;

    while (__is_true(msger->running))
    {
        if (__is_false(msger->working)){
            __xlogd("recv_loop ############################# udp_listen enter\n");
            __set_true(msger->listening);
            __xapi->udp_listen(msger->sock);
            __set_false(msger->listening);
            __xlogd("recv_loop ############################# udp_listen exit\n");
            // __xapi->mutex_lock(msger->mtx);
            __xapi->mutex_notify(msger->mtx);
            // __xapi->mutex_unlock(msger->mtx);            
        }
        
        if (xpipe_readable(msger->rpipe) > 0)
        {
            __xlogd("recv_loop ############################# xpipe_read enter\n");
            __xbreak(xpipe_read(msger->rpipe, &msg, sizeof(void*)) != sizeof(void*));
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


static void* send_loop(void *ptr)
{
    __xlogd("xmsger_loop enter\n");

    int result;
    xmsg_ptr msg;
    int64_t timeout;
    uint64_t timer = 1000000000ULL;
    xmsger_ptr msger = (xmsger_ptr)ptr;
    
    xmsgpack_ptr rpack = NULL;
    xheapnode_ptr timenode;
    xmsgpack_ptr sendunit = NULL;
    xchannel_ptr channel = NULL;
    xchannel_ptr next = NULL;

    struct __xipaddr addr;
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 0, &addr));

    while (__is_true(msger->running))
    {

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

            if (__is_true(msger->running)){

                if (msger->len - msger->pos == 0){
                    __xapi->mutex_lock(msger->mtx);
                    __xlogd("xmsger_loop ------------------------------------------ wait msg enter\n");
                    __set_false(msger->working);
                    __xapi->mutex_wait(msger->mtx);
                    __set_true(msger->working);
                    __xlogd("xmsger_loop ------------------------------------------ wait msg exit\n");
                    __xapi->mutex_unlock(msger->mtx);
                }else {
                    __xapi->mutex_lock(msger->mtx);
                    if (timer != 0){
                        __xapi->mutex_timedwait(msger->mtx, timer);
                    }else {
                        __xapi->mutex_timedwait(msger->mtx, 10 * MILLI_SECONDS);
                    }
                    __xapi->mutex_unlock(msger->mtx);
                }

            }
        }


        if (xpipe_readable(msger->spipe) > 0){
            // 连接的发起和消息开始发送，都必须经过这个管道
            __xbreak(xpipe_read(msger->spipe, &msg, sizeof(void*)) != sizeof(void*));

            if (msg->channel != NULL){
                __xlogd("xmsger_loop send msg to peer\n");
                // 这里要检查，通道是否在发送队列中
                if (!msg->channel->sending){
                    xchannel_resume(channel);
                }

                if (__is_true(msger->listening)){

                    while (msg->wpos < msg->len)
                    {
                        if (__transbuf_writable(msg->channel->sendbuf) == 0){
                            break;
                        }
                        xmsgpack_ptr pack = make_pack(msg->channel, XMSG_PACK_MSG);
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
                    __xbreak(xpipe_write(msger->rpipe, &msg, sizeof(void*)) != sizeof(void*));
                }


            }else {

                __xlogd("xmsger_loop create channel to peer\n");
                xchannel_ptr channel = xchannel_create(msger, (__xipaddr_ptr)msg->addr);
                __xbreak(channel == NULL);

                // 建立连接时，先用 IP 作为本地索引，在收到 PONG 时，换成 cid 做为索引
                xtree_save(msger->peers, &channel->addr.port, channel->addr.keylen, channel);
                xmsgpack_ptr spack = make_pack(channel, XMSG_PACK_PING);
                // 建立连接时，cid 必须是 0
                spack->head.cid = 0;
                // 这里是协议层验证
                // TODO 需要更换一个密钥
                spack->head.x = (XMSG_VAL ^ XMSG_KEY);
                *((uint32_t*)(spack->body)) = channel->cid;
                *((uint64_t*)(spack->body + 4)) = __xapi->clock();
                spack->head.pack_size = 12;
                __atom_add(channel->len, spack->head.pack_size);
                xchannel_push(channel, spack);

                if (msg->addr){
                    free(msg->addr);
                }
                free(msg);

            }
                        
        }


        __xlogd("xmsger_loop ------------------------------------------ looooooop\n");

        channel = msger->squeue.head.next;

        while (channel != &msger->squeue.end)
        {
            next = channel->next;

            timeout = xchannel_send(channel, NULL);
            if (timeout > 0 && timer > timeout){
                timer = timeout;
            }

            if (__xapi->clock() - channel->update > (1000000000ULL * 10)){
                // 十秒内没有收发过数据，释放连建
                xtree_take(msger->peers, &channel->cid, 4);
                xchannel_free(channel);
                channel->msger->listener->onDisconnection(channel->msger->listener, channel);
            }

            channel = next;
        }
    }

Clean:    


    if (rpack != NULL){
        free(rpack);
    }

    __xlogd("xmsger_loop exit\n");
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

    // __xbreak(xpipe_write(channel->msgqueue, &msg, sizeof(void*)) != sizeof(void*));
    __xbreak(xpipe_write(msger->spipe, &msg, sizeof(void*)) != sizeof(void*));

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
    __xbreak(xpipe_write(msger->spipe, &msg, sizeof(void*)) != sizeof(void*));

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

    msger->spipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->spipe == NULL);

    msger->rpipe = xpipe_create(sizeof(void*) * 1024);
    __xbreak(msger->rpipe == NULL);

    msger->spid = __xapi->process_create(send_loop, msger);
    __xbreak(msger->spid == NULL);

    msger->rpid = __xapi->process_create(recv_loop, msger);
    __xbreak(msger->rpid == NULL);

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

        if (msger->spipe){
            __xlogd("xmsger_free send pipe\n");
            xpipe_free(&msger->spipe);
        }

        if (msger->rpipe){
            __xlogd("xmsger_free recv pipe\n");
            xpipe_free(&msger->rpipe);
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