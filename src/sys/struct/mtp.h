#ifndef __MESSAGE_TRANSPORT_H__
#define __MESSAGE_TRANSPORT_H__


#include <env/env.h>
#include <env/task.h>
#include <env/malloc.h>
#include <sys/struct/heap.h>
#include <sys/struct/tree.h>


enum {
    TRANSUNIT_NONE = 0x00,
    TRANSUNIT_MSG = 0x01,
    TRANSUNIT_ACK = 0x02,
    TRANSUNIT_PING = 0x04,
    TRANSUNIT_HELLO = 0x08,
    TRANSUNIT_BYE = 0x10,
    TRANSUNIT_UNKNOWN = 0x20
};

#define UNIT_HEAD_SIZE          8
#define UNIT_BODY_SIZE          1280
// #define UNIT_BODY_SIZE          8

#define UNIT_BUF_RANGE          16
#define UNIT_MSG_RANGE          8192

#define TRANSMSG_SIZE           ( UNIT_BODY_SIZE * UNIT_MSG_RANGE )
#define TRANSUNIT_SIZE          ( UNIT_BODY_SIZE + UNIT_HEAD_SIZE )


typedef struct msgchannel* msgchannel_ptr;
typedef struct msglistener* msglistener_ptr;
typedef struct msgtransport* msgtransport_ptr;
typedef struct channelqueue* channelqueue_ptr;

typedef struct msgaddr {
    void *addr;
    unsigned int addrlen;
    uint8_t keylen;
    union {
        char key[6];
        struct {
            uint32_t ip;
            uint16_t port;
        };
    };
}*msgaddr_ptr;

typedef struct unithead {
    uint8_t type;
    uint8_t sn;
    uint8_t ack_sn;
    uint8_t ack_range;
    uint16_t msg_range;
    uint16_t body_size;
}*unithead_ptr;

typedef struct transunit {
    bool comfirmed;
    uint8_t resending;
    struct heapnode ts;
    msgchannel_ptr channel;
    struct unithead head;
    uint8_t body[UNIT_BODY_SIZE];
}*transunit_ptr;

typedef struct transunitbuf {
    uint8_t range;
    ___atom_8bit upos, rpos, wpos;
    struct transunit *buf[1];
}*transunitbuf_ptr;


typedef struct transmsg {
    msgchannel_ptr channel;
    size_t size;
    char data[1];
}*transmsg_ptr;


typedef struct transmsgbuf {
    transmsg_ptr msg;
    uint16_t msg_range;
    uint8_t range, upos, rpos, wpos;
    struct transunit *buf[1];
}*transmsgbuf_ptr;


struct msgchannel {
    msgchannel_ptr prev, next;
    int status;
    ___mutex_ptr mtx;
    ___atom_bool connected;
    ___atom_bool disconnected;
    ___atom_bool termination;
    uint8_t interval;
    uint64_t update;
    heap_ptr timerheap;
    struct unithead ack;
    transmsgbuf_ptr msgbuf;
    channelqueue_ptr queue;
    struct msgaddr addr;
    transunitbuf_ptr sendbuf;
    msgtransport_ptr mtp;
};


typedef struct channelqueue {
    ___atom_size len;
    ___atom_bool lock;
    struct msgchannel head, end;
}*channelqueue_ptr;


struct msglistener {
    void *ctx;
    void (*connection)(struct msglistener*, msgchannel_ptr channel);
    void (*disconnection)(struct msglistener*, msgchannel_ptr channel);
    void (*message)(struct msglistener*, msgchannel_ptr channel, transmsg_ptr msg);
    void (*timeout)(struct msglistener*, msgchannel_ptr channel);
};


typedef struct physics_socket {
    void *ctx;
    void (*listening)(struct physics_socket *socket);
    size_t (*sendto)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
    size_t (*recvfrom)(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size);
}*physics_socket_ptr;


#define MSGCHANNELQUEUE_ARRAY_RANGE     3
#define TRANSACK_BUFF_RANGE             4096
#define TRANSUNIT_TIMEOUT_INTERVAL      1000000000ULL

struct msgtransport {
    __tree peers;
    ___mutex_ptr mtx;
    ___atom_bool running;
    msglistener_ptr listener;
    physics_socket_ptr device;
    linekv_ptr mainloop_func;
    taskqueue_ptr mainloop_task;
    ___atom_size send_queue_length;
    channelqueue_ptr connectionqueue, sendqueue, disconnctionqueue;
    struct channelqueue channels[MSGCHANNELQUEUE_ARRAY_RANGE];
};


#define __transbuf_wpos(b)          ((b)->wpos & ((b)->range - 1))
#define __transbuf_rpos(b)          ((b)->rpos & ((b)->range - 1))
#define __transbuf_upos(b)          ((b)->upos & ((b)->range - 1))

#define __transbuf_inuse(b)         ((uint8_t)((b)->upos - (b)->rpos))
#define __transbuf_usable(b)        ((uint8_t)((b)->wpos - (b)->upos))

#define __transbuf_readable(b)      ((uint8_t)((b)->wpos - (b)->rpos))
#define __transbuf_writable(b)      ((uint8_t)((b)->range - 1 - (b)->wpos + (b)->rpos))



static inline void msgchannel_enqueue(msgchannel_ptr channel, channelqueue_ptr queue)
{
    ___atom_lock(&queue->lock);
    channel->next = &queue->end;
    channel->prev = queue->end.prev;
    channel->next->prev = channel;
    channel->prev->next = channel;
    channel->queue = queue;
    ___atom_add(&queue->len, 1);
    ___atom_unlock(&queue->lock);
    __logi("msgchannel_enqueue channel: 0x%x", channel);
}

static inline msgchannel_ptr msgchannel_dequeue(channelqueue_ptr queue)
{
    if (queue->len > 0){
        if (___atom_try_lock(&queue->lock)){
            msgchannel_ptr channel = queue->head.next;
            channel->prev->next = channel->next;
            channel->next->prev = channel->prev;
            channel->queue = NULL;
            ___atom_sub(&queue->len, 1);
            ___atom_unlock(&queue->lock);
            return channel;
        }
    }
    return NULL;
}

static inline void msgchannel_clear(msgchannel_ptr channel)
{
    ___lock lk = ___mutex_lock(channel->mtx);
    if (channel->queue != NULL){
        channel->prev->next = channel->next;
        channel->next->prev = channel->prev;
        ___atom_sub(&channel->queue->len, 1);
        channel->queue = NULL;
    }
    ___mutex_notify(channel->mtx);
    ___mutex_unlock(channel->mtx, lk);
    ___atom_sub(&channel->mtp->send_queue_length, __transbuf_usable(channel->sendbuf));
    tree_delete(channel->mtp->peers, channel->addr.key, channel->addr.keylen);
}

static inline void msgchannel_push(msgchannel_ptr channel, transunit_ptr unit)
{
    unit->channel = channel;
    unit->resending = 0;
    unit->comfirmed = false;

    if (___is_true(&channel->disconnected)){
        return;
    }

    while (__transbuf_writable(channel->sendbuf) == 0){
        ___lock lk = ___mutex_lock(channel->mtx);
        if (___is_true(&channel->disconnected)){
            ___mutex_unlock(channel->mtx, lk);
            return;
        }
        ___mutex_wait(channel->mtx, lk);
        if (___is_true(&channel->disconnected)){
            ___mutex_unlock(channel->mtx, lk);
            return;
        }
        ___mutex_unlock(channel->mtx, lk);
    }

    // 再将 unit 放入缓冲区 
    unit->head.sn = channel->sendbuf->wpos;
    channel->sendbuf->buf[__transbuf_wpos(channel->sendbuf)] = unit;
    ___atom_add(&channel->sendbuf->wpos, 1);

    if (___atom_add(&channel->mtp->send_queue_length, 1) == 1){
        ___lock lk = ___mutex_lock(channel->mtp->mtx);
        ___mutex_notify(channel->mtp->mtx);
        ___mutex_unlock(channel->mtp->mtx, lk);
    }
}

static inline void msgchannel_pull(msgchannel_ptr channel, transunit_ptr ack)
{
    // 只处理 sn 在 rpos 与 upos 之间的 transunit
    if ((uint8_t)(ack->head.ack_sn - channel->sendbuf->rpos) <= (uint8_t)(channel->sendbuf->upos - channel->sendbuf->rpos)){

        uint8_t index;
        transunit_ptr unit;
        heapnode_ptr timenode;

        // __logi("msgchannel_pull recv serial ack_sn: %u ack_range: %u rpos: %u upos: %u wpos %u", 
        //     ack->head.ack_sn, ack->head.ack_range,
        //     channel->sendbuf->rpos + 0, channel->sendbuf->upos + 0, channel->sendbuf->wpos + 0);

        if (ack->head.ack_sn == ack->head.ack_range){
            // 收到连续的 ACK，连续的最大序列号是 maximal

            do {
                // rpos 对应的 ACK 可能丢失了，现在更新 rpos 并且释放资源
                assert(__transbuf_inuse(channel->sendbuf) > 0);
                // 释放所有已经确认的 SN
                index = __transbuf_rpos(channel->sendbuf);
                unit = channel->sendbuf->buf[index];

                if (!unit->comfirmed){
                    // 移除定时器，设置确认状态
                    assert(channel->timerheap->array[unit->ts.pos] != NULL);
                    unit->comfirmed = true;
                    timenode = heap_delete(channel->timerheap, &unit->ts);
                    assert(timenode->value == unit);
                }

                if (unit->head.type == TRANSUNIT_HELLO){
                    if (___set_true(&channel->connected)){
                        channel->mtp->listener->connection(channel->mtp->listener, channel);
                    }
                }else if (unit->head.type == TRANSUNIT_BYE){
                    if (___set_true(&channel->disconnected)){
                        channel->mtp->listener->disconnection(channel->mtp->listener, channel);
                    }
                }

                // 释放内存
                free(unit);
                // 索引位置空
                channel->sendbuf->buf[index] = NULL;

                ___atom_add(&channel->sendbuf->rpos, 1);
                ___mutex_notify(channel->mtx);

            } while ((uint8_t)(ack->head.ack_range - channel->sendbuf->rpos) <= 0);

//            assert(__transbuf_inuse(channel->sendbuf) > 0);
//            index = __transbuf_rpos(channel->sendbuf);
//            unit = channel->sendbuf->buf[index];

//            if (!unit->comfirmed){
//                assert(channel->timer->array[unit->ts.pos] != NULL);
//                unit->comfirmed = true;
//                timenode = heap_delete(channel->timer, &unit->ts);
//                assert(timenode->value == unit);
//            }

//            free(unit);
//            channel->sendbuf->buf[index] = NULL;

//            ___atom_add(&channel->sendbuf->rpos, 1);
//            ___mutex_notify(channel->mtx);

            channel->interval = 0;

        } else {

            // __logi("msgchannel_pull recv interval ack_sn: %u ack_range: %u", ack->head.ack_sn, ack->head.ack_range);
            index = ack->head.ack_sn & (UNIT_BUF_RANGE - 1);
            unit = channel->sendbuf->buf[index];

            // 检测此 SN 是否未确认
            if (unit && !unit->comfirmed){
                assert(channel->timerheap->array[unit->ts.pos] != NULL);
                // 移除定时器，设置确认状态
                unit->comfirmed = true;
                timenode = heap_delete(channel->timerheap, &unit->ts);
                assert(timenode->value == unit);
            }

            // 重传 rpos 到 SN 之间的所有尚未确认的 SN
            if (++channel->interval > 2){
                index = channel->sendbuf->rpos;
                while (index != ack->head.sn) {
                    unit = channel->sendbuf->buf[index & (channel->sendbuf->range - 1)];
                    if (!unit->comfirmed && unit->resending == 0){
                        __logi("msgchannel_pull resend sn: %u", index);
                        if (channel->mtp->device->sendto(channel->mtp->device, &channel->addr, 
                            (void*)&(unit->head), UNIT_HEAD_SIZE + unit->head.body_size) == UNIT_HEAD_SIZE + unit->head.body_size){
                            unit->resending++;
                        }
                    }
                    index++;
                }
            }
        }
    }
}

static inline void msgchannel_recv(msgchannel_ptr channel, transunit_ptr unit)
{
    channel->ack = unit->head;
    channel->ack.type = TRANSUNIT_ACK;
    channel->ack.body_size = 0;
    uint16_t index = unit->head.sn & (UNIT_BUF_RANGE - 1);
    
    __logi("msgchannel_recv SN: %u wpos: %u rpos: %u", unit->head.sn, channel->msgbuf->wpos, channel->msgbuf->rpos);
    
    if (unit->head.sn == channel->msgbuf->wpos){

        // 更新最大连续 SN
        channel->ack.ack_sn = unit->head.sn;
        channel->ack.ack_range = channel->msgbuf->wpos;

        channel->msgbuf->buf[index] = unit;
        channel->msgbuf->wpos++;
        while (channel->msgbuf->buf[__transbuf_wpos(channel->msgbuf)] != NULL){
            //TODO
            assert(__transbuf_writable(channel->msgbuf) > 0);
            channel->msgbuf->wpos++;
        }

    }else {

        // SN 不在 rpos 与 wpos 之间
        if ((uint8_t)(channel->msgbuf->wpos - unit->head.sn) > (uint8_t)(unit->head.sn - channel->msgbuf->wpos)){

            channel->ack.ack_sn = unit->head.sn;
            channel->ack.ack_range = channel->msgbuf->wpos - 1;

            // SN 在 wpos 方向越界，是提前到达的 MSG
            if (channel->msgbuf->buf[index] == NULL){
                // __logi("msgchannel_recv over range sn: %u wpos: %u", unit->head.serial_number, channel->msgbuf->wpos);
                channel->msgbuf->buf[index] = unit;
            }else {
                free(unit);
            }
            
        }else {
            // __logi("msgchannel_recv over range ++++++++++++++++ rpos %u", unit->head.serial_number);
            channel->ack.ack_sn = channel->msgbuf->wpos - 1;
            channel->ack.ack_range = channel->ack.ack_sn;
            // SN 在 rpos 方向越界，是重复的 SN
            free(unit);
        }
    }

    if ((channel->mtp->device->sendto(channel->mtp->device, &channel->addr, (void*)&(channel->ack), UNIT_HEAD_SIZE)) == UNIT_HEAD_SIZE) {
        channel->ack.type = TRANSUNIT_NONE;
    }

    index = __transbuf_rpos(channel->msgbuf);
    while (channel->msgbuf->buf[index] != NULL){
        if (channel->msgbuf->msg == NULL){
            channel->msgbuf->msg_range = channel->msgbuf->buf[index]->head.msg_range;
            assert(channel->msgbuf->msg_range != 0 && channel->msgbuf->msg_range <= UNIT_MSG_RANGE);
            channel->msgbuf->msg = (transmsg_ptr)malloc(sizeof(struct transmsg) + (channel->msgbuf->msg_range * UNIT_BODY_SIZE));
            channel->msgbuf->msg->size = 0;
            channel->msgbuf->msg->channel = channel;
        }

        memcpy(channel->msgbuf->msg->data + channel->msgbuf->msg->size, 
            channel->msgbuf->buf[index]->body, 
            channel->msgbuf->buf[index]->head.body_size);
        channel->msgbuf->msg->size += channel->msgbuf->buf[index]->head.body_size;
        channel->msgbuf->msg_range--;
        __logi("msgchannel_recv msg_range %u", channel->msgbuf->msg_range);
        if (channel->msgbuf->msg_range == 0){
            channel->msgbuf->msg->data[channel->msgbuf->msg->size] = '\0';
            __logi("msgchannel_recv process msg %s", channel->msgbuf->msg->data);
            channel->mtp->listener->message(channel->mtp->listener, channel, channel->msgbuf->msg);
            channel->msgbuf->msg = NULL;
        }
        free(channel->msgbuf->buf[index]);
        channel->msgbuf->buf[index] = NULL;
        channel->msgbuf->rpos++;
        index = __transbuf_rpos(channel->msgbuf);
    }
}

static inline msgchannel_ptr msgchannel_create(msgtransport_ptr mtp, msgaddr_ptr addr)
{
    msgchannel_ptr channel = (msgchannel_ptr) malloc(sizeof(struct msgchannel));
    channel->connected = false;
    channel->disconnected = false;
    channel->update = ___sys_clock();
    channel->interval = 0;
    channel->mtp = mtp;
    channel->addr = *addr;
    channel->mtx = ___mutex_create();
    channel->msgbuf = (transmsgbuf_ptr) calloc(1, sizeof(struct transmsgbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->msgbuf->range = UNIT_BUF_RANGE;
    channel->sendbuf = (transunitbuf_ptr) calloc(1, sizeof(struct transunitbuf) + sizeof(transunit_ptr) * UNIT_BUF_RANGE);
    channel->sendbuf->range = UNIT_BUF_RANGE;
    channel->timerheap = heap_create(UNIT_BUF_RANGE);
    channel->queue = NULL;
    return channel;
}

static inline void msgchannel_release(msgchannel_ptr channel)
{
    __logi("msgchannel_release enter");
    heap_destroy(&channel->timerheap);
    free(channel->msgbuf);
    free(channel->sendbuf);
    free(channel);
    ___mutex_release(channel->mtx);
    __logi("msgchannel_release exit");
}

static inline void msgchannel_termination(msgchannel_ptr *pptr)
{
    __logi("msgchannel_termination enter");
    if (pptr && *pptr){
        msgchannel_ptr channel = *pptr;
        *pptr = NULL;
        ___atom_lock(&channel->mtp->disconnctionqueue->lock);
        channel->next = &channel->mtp->disconnctionqueue->end;
        channel->prev = channel->mtp->disconnctionqueue->end.prev;
        channel->next->prev = channel;
        channel->prev->next = channel;
        ___atom_add(&channel->mtp->disconnctionqueue->len, 1);
        ___atom_unlock(&channel->mtp->disconnctionqueue->lock);
    }
    __logi("msgchannel_termination exit");
}

static inline void msgtransport_recv_loop(msgtransport_ptr mtp)
{
    __logi("msgtransport_recv_loop enter");

    while (___is_true(&mtp->running))
    {
        mtp->device->listening(mtp->device);
//        __logi("msgtransport_recv_loop readable");
        if (___is_true(&mtp->running)){
            ___lock lk = ___mutex_lock(mtp->mtx);
            ___mutex_notify(mtp->mtx);
            ___mutex_wait(mtp->mtx, lk);
            ___mutex_unlock(mtp->mtx, lk);
        }
    }

    __logi("msgtransport_recv_loop exit");
}

static inline void msgtransport_main_loop(linekv_ptr ctx)
{
    __logi("msgtransport_main_loop enter");

    int result;
    int64_t timeout;
    uint64_t timer = 1000000000ULL;
    // transack_ptr ack;
    struct msgaddr addr;
    transunit_ptr recvunit = NULL;
    heapnode_ptr timenode;
    transunit_ptr sendunit = NULL;
    msgchannel_ptr channel = NULL;

    msgtransport_ptr mtp = (msgtransport_ptr)linekv_find_ptr(ctx, "ctx");

    addr.addr = NULL;

    while (___is_true(&mtp->running))
    {

        if (recvunit == NULL){
            recvunit = (transunit_ptr)malloc(sizeof(struct transunit));
            if (recvunit == NULL){
                __logi("msgtransport_recv_loop malloc failed");
                exit(0);
            }
        }

        recvunit->head.type = 0;
        recvunit->head.body_size = 0;
        result = mtp->device->recvfrom(mtp->device, &addr, &recvunit->head, TRANSUNIT_SIZE);

        if (result == (recvunit->head.body_size + UNIT_HEAD_SIZE)){

            __logi("msgtransport_main_loop recv ip: %u port: %u keylen: %u", addr.ip, addr.port, addr.keylen);

            if (recvunit->head.type == TRANSUNIT_HELLO){

                __logi("msgtransport_main_loop recv ip: %u port: %u keylen: %u", addr.ip, addr.port, addr.keylen);
                channel = (msgchannel_ptr)tree_find(mtp->peers, addr.key, addr.keylen);
                __logi("msgtransport_main_loop TRANSUNIT_HELLO find channel: 0x%x", channel);

                if (channel != NULL){
                    if (___set_true(&channel->disconnected)){
                        __logi("msgtransport_main_loop reconnection channel clear: 0x%x", channel);
                        msgchannel_clear(channel);
                        mtp->listener->disconnection(mtp->listener, channel);
                    }
                }

                channel = msgchannel_create(mtp, &addr);
                __logi("msgtransport_main_loop new connections channel: 0x%x", channel);
                msgchannel_enqueue(channel, mtp->sendqueue);
                tree_inseart(mtp->peers, addr.key, addr.keylen, channel);
                ___set_true(&channel->connected);
                channel->mtp->listener->connection(channel->mtp->listener, channel);

            }else {

                channel = (msgchannel_ptr)tree_find(mtp->peers, addr.key, addr.keylen);
                if (channel == NULL){
                    __logi("msgtransport_main_loop recv a invalid transunit: %u", recvunit->head.type);
                    continue;
                }
            }

//            __logi("msgtransport_main_loop unit type: %u msg: %s", recvunit->head.type, recvunit->body);

            channel->update = ___sys_clock();

            if (recvunit->head.type == TRANSUNIT_ACK){
                msgchannel_pull(channel, recvunit);
            }else if (recvunit->head.type > TRANSUNIT_NONE && recvunit->head.type < TRANSUNIT_UNKNOWN){
                msgchannel_recv(channel, recvunit);
                if (recvunit->head.type == TRANSUNIT_BYE){
                    if (___set_true(&channel->disconnected)){
                        __logi("msgtransport_main_loop disconnection channel: 0x%x", channel);
                        mtp->listener->disconnection(mtp->listener, channel);
                    }
                }
                recvunit = NULL;
            }

        }else {

             __logi("msgtransport_main_loop send_queue_length %llu channel queue length %llu", mtp->send_queue_length + 0,
                 mtp->channels[0].len + mtp->channels[1].len + mtp->channels[2].len);
            // __logi("msgtransport_main_loop timer %llu", timer);
             if (mtp->send_queue_length == 0 && mtp->connectionqueue->len == 0){
                ___lock lk = ___mutex_lock(mtp->mtx);
                ___mutex_notify(mtp->mtx);
                ___mutex_timer(mtp->mtx, lk, timer);
                ___mutex_unlock(mtp->mtx, lk);
                timer = 1000000000ULL * 10;
            }
        }


        channel = msgchannel_dequeue(mtp->connectionqueue);
        if (channel != NULL){
            __logi("msgtransport_main_loop sendto addr ip: %u port: %u", channel->addr.ip, channel->addr.port);
            tree_inseart(mtp->peers, channel->addr.key, channel->addr.keylen, channel);
            msgchannel_enqueue(channel, mtp->sendqueue);
        }


        channel = mtp->sendqueue->head.next;
        while (channel != &mtp->sendqueue->end)
        {
            if (__transbuf_usable(channel->sendbuf) > 0){

                sendunit = channel->sendbuf->buf[__transbuf_upos(channel->sendbuf)];
                result = mtp->device->sendto(mtp->device, &channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                    channel->sendbuf->upos++;
                    ___mutex_notify(channel->mtx);//TODO
                    sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                    sendunit->ts.value = sendunit;
                    heap_push(channel->timerheap, &sendunit->ts);
                    ___atom_sub(&mtp->send_queue_length, 1);
                    sendunit->resending = 0;
                }else {
                    __logi("msgtransport_main_loop send try again");
                    sendunit->resending++;
                    if (sendunit->resending > 5){
                        __logi("msgtransport_main_loop ***send try again timeout*** termination 0x%x addr: ip: %u port: %u", channel, channel->addr.ip, channel->addr.port);
                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }

            }else {

                if (channel->timerheap->pos == 0 && __transbuf_readable(channel->sendbuf) == 0){
                    if (___sys_clock() - channel->update > (1000000000ULL * 30)){
                        __logi("msgtransport_main_loop ***timeout*** recycle channel: 0x%x addr: ip: %u port: %u", channel, channel->addr.ip, channel->addr.port);
                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }
            }

            if (channel->timerheap->pos > 0){

                if ((timeout = __heap_min(channel->timerheap)->key - ___sys_clock()) > 0){

                    if (timer > timeout){
                        timer = timeout;
                    }

                }else if (timeout > (-1000000000ULL * 30)){

                    __logi("msgtransport_main_loop timeout resend %u", sendunit->head.sn);
                    sendunit = (transunit_ptr)__heap_min(channel->timerheap)->value;
                    if (sendunit->resending < 5){
                        mtp->device->sendto(mtp->device, &sendunit->channel->addr, (void*)&(sendunit->head), UNIT_HEAD_SIZE + sendunit->head.body_size);
                        if (result == UNIT_HEAD_SIZE + sendunit->head.body_size){
                            heap_pop(channel->timerheap);
                            sendunit->ts.key = ___sys_clock() + TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->ts.value = sendunit;
                            heap_push(channel->timerheap, &sendunit->ts);
                            sendunit->resending = 0;
                        }else {
                            __logi("msgtransport_main_loop send try again %u", sendunit->resending);
                            timer = TRANSUNIT_TIMEOUT_INTERVAL;
                            sendunit->resending++;
                        }

                    }else {

                        __logi("msgtransport_main_loop ***send try again timeout*** termination 0x%x addr: ip: %u port: %u", channel, channel->addr.ip, channel->addr.port);
                        msgchannel_clear(channel);
                        mtp->listener->timeout(mtp->listener, channel);
                    }
                }
            }

            channel = channel->next;
        }

        if (mtp->disconnctionqueue->len > 0){
            __logi("msgtransport_main_loop ----------------------------------------- delete channel");
            if (___atom_try_lock(&mtp->disconnctionqueue->lock)){
                channel = mtp->disconnctionqueue->head.next;
                do {
                    channel->next->prev = channel->prev;
                    channel->prev->next = channel->next;
                    ___atom_sub(&mtp->disconnctionqueue->len, 1);
//                    msgchannel_clear(channel);
                    msgchannel_release(channel);
                    channel = mtp->disconnctionqueue->head.next;
                }while (channel != &mtp->disconnctionqueue->end);
                ___atom_unlock(&mtp->disconnctionqueue->lock);
            }
        }
    }


    if (recvunit != NULL){
        free(recvunit);
    }

    free(addr.addr);

    __logi("msgtransport_main_loop exit");
}

static inline msgtransport_ptr msgtransport_create(physics_socket_ptr device, msglistener_ptr listener)
{
    __logi("msgtransport_create enter");

    msgtransport_ptr mtp = (msgtransport_ptr)calloc(1, sizeof(struct msgtransport));

    mtp->running = true;
    mtp->device = device;
    mtp->listener = listener;
    mtp->send_queue_length = 0;
    mtp->mtx = ___mutex_create();
    mtp->peers = tree_create();
    
    for (size_t i = 0; i < MSGCHANNELQUEUE_ARRAY_RANGE; i++)
    {
        channelqueue_ptr queueptr = &mtp->channels[i];
        queueptr->len = 0;
        queueptr->lock = false;
        queueptr->head.prev = NULL;
        queueptr->end.next = NULL;
        queueptr->head.next = &queueptr->end;
        queueptr->end.prev = &queueptr->head;
    }

    mtp->connectionqueue = &mtp->channels[0];
    mtp->sendqueue = &mtp->channels[1];
    mtp->disconnctionqueue = &mtp->channels[2];

//    mtp->mainloop_func = linekv_create(1024);
//    linekv_add_ptr(mtp->mainloop_func, "func", (void*)msgtransport_main_loop);
//    linekv_add_ptr(mtp->mainloop_func, "ctx", mtp);
//    mtp->mainloop_task = taskqueue_create();
//    taskqueue_post(mtp->mainloop_task, mtp->mainloop_func);

    mtp->mainloop_task = taskqueue_run_loop(msgtransport_main_loop, mtp);

    __logi("msgtransport_create exit");

    return mtp;
}

static void free_channel(__ptr val)
{
    __logi(">>>>------------> free channel 0x%x", val);
    msgchannel_release((msgchannel_ptr)val);
}

static inline void msgtransport_release(msgtransport_ptr *pptr)
{
    __logi("msgtransport_release enter");
    if (pptr && *pptr){        
        msgtransport_ptr mtp = *pptr;
        *pptr = NULL;
        ___set_false(&mtp->running);
        ___mutex_broadcast(mtp->mtx);
        taskqueue_release(&mtp->mainloop_task);
        linekv_release(&mtp->mainloop_func);
        tree_clear(mtp->peers, free_channel);
        tree_release(&mtp->peers);
        ___mutex_release(mtp->mtx);
        free(mtp);
    }
    __logi("msgtransport_release exit");
}

static inline msgchannel_ptr msgtransport_connect(msgtransport_ptr mtp, msgaddr_ptr addr)
{
    __logi("msgtransport_connect enter");
    msgchannel_ptr channel = msgchannel_create(mtp, addr);
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    unit->head.type = TRANSUNIT_HELLO;
    unit->head.body_size = 0;
    unit->head.msg_range = 1;
    msgchannel_enqueue(channel, mtp->connectionqueue);
    msgchannel_push(channel, unit);

    // ___lock lk = ___mutex_lock(channel->mtx);
    // ___mutex_wait(channel->mtx, lk);
    // ___mutex_unlock(channel->mtx, lk);
    // __logi("msgtransport_connect exit");
    __logi("msgtransport_connect exit");
    return channel;
}

static inline void msgtransport_disconnect(msgtransport_ptr mtp, msgchannel_ptr channel)
{
    __logi("msgtransport_disconnect enter");
    transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
    unit->head.type = TRANSUNIT_BYE;
    unit->head.body_size = 0;
    unit->head.msg_range = 1;
    msgchannel_push(channel, unit);
    __logi("msgtransport_disconnect exit");
}

static inline void msgtransport_ping(msgchannel_ptr channel)
{
    __logi("msgtransport_ping enter");
    if (___sys_clock() - channel->update >= 1000000000ULL * 10){
        transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
        unit->head.type = TRANSUNIT_MSG;
        unit->head.body_size = 4;
        unit->head.msg_range = 1;
        unit->body[0] = 'a';
        unit->body[1] = 'b';
        unit->body[2] = 'c';
        unit->body[3] = '\0';
        msgchannel_push(channel, unit);
    }
    __logi("msgtransport_ping exit");
}

static inline void msgtransport_send(msgtransport_ptr mtp, msgchannel_ptr channel, void *data, size_t size)
{
    char *msg_data;
    size_t msg_size;
    size_t msg_count = size / (TRANSMSG_SIZE);
    size_t last_msg_size = size - (msg_count * TRANSMSG_SIZE);

    uint16_t unit_count;
    uint16_t last_unit_size;
    uint16_t last_unit_id;

    for (int x = 0; x < msg_count; ++x){
        msg_size = TRANSMSG_SIZE;
        msg_data = ((char*)data) + x * TRANSMSG_SIZE;
        for (size_t y = 0; y < UNIT_MSG_RANGE; y++)
        {
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.msg_range = UNIT_MSG_RANGE - y;
            msgchannel_push(channel, unit);
        }
    }

    if (last_msg_size){
        msg_data = ((char*)data) + (size - last_msg_size);
        msg_size = last_msg_size;

        unit_count = msg_size / UNIT_BODY_SIZE;
        last_unit_size = msg_size - unit_count * UNIT_BODY_SIZE;
        last_unit_id = 0;

        if (last_unit_size > 0){
            last_unit_id = 1;
        }

        for (size_t y = 0; y < unit_count; y++){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (y * UNIT_BODY_SIZE), UNIT_BODY_SIZE);
            unit->head.type = TRANSUNIT_MSG;
            unit->head.body_size = UNIT_BODY_SIZE;
            unit->head.msg_range = (unit_count + last_unit_id) - y;
            msgchannel_push(channel, unit);
        }

        if (last_unit_id){
            transunit_ptr unit = (transunit_ptr)malloc(sizeof(struct transunit));
            memcpy(unit->body, msg_data + (unit_count * UNIT_BODY_SIZE), last_unit_size);
            unit->head.type = TRANSUNIT_MSG;        
            unit->head.body_size = last_unit_size;
            unit->head.msg_range = last_unit_id;
            msgchannel_push(channel, unit);
        }
    }
}



#endif //__MESSAGE_TRANSPORT_H__
