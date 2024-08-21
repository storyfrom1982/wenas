#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>
#include <xnet/uuid.h>

#include <stdio.h>
#include <stdlib.h>


// KEY_BITS >= 2
#define KEY_BITS    4
#define KEY_SPACE   (1 << KEY_BITS)


typedef struct XChord {
    uint32_t key;
    xchannel_ptr channel;
    struct XChord* predecessor;
    struct XChord* finger_table[KEY_BITS];
}*XChord_Ptr;

static inline bool key_in_open_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (a < b){ 
        if (a < key && key < b){
            //(a=2)->(key=7)->(b=13) 不含零点
            return true;
        }
    }else if (a > b){
        if ((a < key && key > b) || (a > key && key < b)){ 
            //(a=13)->(key=15)->0->(b=2) 环绕零点
            //(a=13)->0->(key=1)->(b=2) 环绕零点
            return true;
        }
    }
    return false;
}

static inline bool key_in_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == a || key == b || a == b){
        return true;
    }
    return key_in_open_interval(key, a, b);
}

static inline bool key_in_left_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == a){
        return true;
    }
    return key_in_open_interval(key, a, b);
}

static inline bool key_in_right_closed_interval(uint64_t key, uint64_t a, uint64_t b)
{
    if (key == b){
        return true;
    }
    return key_in_open_interval(key, a, b);
}

XChord_Ptr closest_preceding_finger(XChord_Ptr chord, uint32_t key)
{
    XChord_Ptr finger;
    for (int i = KEY_BITS - 1; i >= 0; --i) {
        finger = chord->finger_table[i];
        if (key_in_open_interval(finger->key, chord->key, key)) {
            // 节点 i 大于 n，并且在 n 与 key 之间，逐步缩小范围
            return chord->finger_table[i];
        }
    }
    // 网络中只有一个节点
    return chord;
}

XChord_Ptr find_predecessor(XChord_Ptr chord, uint32_t key)
{
    XChord_Ptr n = chord;
    XChord_Ptr successor = chord->finger_table[1];
    // key 大于 n 并且小于等于 n 的后继，所以 n 是 key 的前继
    while (!key_in_right_closed_interval(key, n->key, successor->key)) {
        // n 的后继不是 key 的后继，要继续查找
        n = closest_preceding_finger(n, key);
        successor = n->finger_table[1];
        if (n->key == successor->key){
            return n;
        }
    }
    return n;
}

XChord_Ptr find_successor(XChord_Ptr chord, uint32_t key) {
    if (key == chord->key){
        return chord;
    }
    XChord_Ptr n = find_predecessor(chord, key);
    return n->finger_table[1];
}

void print_node(XChord_Ptr node)
{
    __xlogd("node->key >>>>--------------------------> enter %u\n", node->key);
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%d] start_key=%u key=%u\n", 
            i, (node->key + (1 << (i-1))) % KEY_SPACE, node->finger_table[i]->key);
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    __xlogd("node->key >>>>-----------------------> exit %u\n", node->key);
}

void node_print_all(XChord_Ptr node)
{
    __xlogd("node_print_all >>>>------------------------------------------------> enter\n");
    XChord_Ptr n = node->predecessor;
    while (n != node){
        XChord_Ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == node){
            print_node(n);
            break;
        }        
    }   
    __xlogd("node_print_all >>>>------------------------------------------------> exit\n"); 
}

void update_others(XChord_Ptr node)
{
    __xlogd("update_others enter\n");
    node_print_all(node);
    uint32_t start;
    XChord_Ptr prev, next, finger_node;
    for (int i = 1; i < KEY_BITS; ++i){
        start = (node->key - (1 << (i-1))) % KEY_SPACE;
        __xlogd("update_others start %u\n", start);
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, start);
        __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        next = prev->finger_table[1];
        if (next->key == start){
            // start 的位置正好指向一个 node，所以这个 node 的路由表项可能会指向当前 node
            finger_node = next->finger_table[i];
            if ((key_in_open_interval(node->key, next->key, finger_node->key) 
                    || next->key == finger_node->key /*原来的路由表指向本身，需要更新*/)){
                next->finger_table[i] = node;
            }
        }
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        finger_node = prev->finger_table[i];
        while (key_in_open_interval(node->key, prev->key, finger_node->key) 
                || prev->key == finger_node->key /*原来的路由表指向本身，需要更新*/){
            // node 在 prev 与 prev 的后继之间
            __xlogd("update_others prev->finder[%d]->key=%u\n", i, node->key);
            prev->finger_table[i] = node;
            prev = prev->predecessor;
            finger_node = prev->finger_table[i];
            __xlogd("update_others [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
                prev->key, node->key, i, prev->finger_table[i]->key);                
        }
    }
    node_print_all(node);
    __xlogd("update_others exit\n");
}

XChord_Ptr node_create(uint32_t key)
{
    __xlogd("node_create enter\n");

    XChord_Ptr node = (XChord_Ptr)calloc(1, sizeof(struct XChord));

    node->key = key;
    node->predecessor = node;

    for (int i = 0; i < KEY_BITS; i++) {
        node->finger_table[i] = node;
    }

    __xlogd("node_create exit\n");

    return node;
}


int node_join(XChord_Ptr ring, XChord_Ptr node)
{
    __xlogd("node_join >>>>--------------------> enter %u\n", node->key);

    uint32_t start;
    XChord_Ptr successor = NULL;

    if (node->key == ring->key){
        return -1;
    }

    if (ring->predecessor->key == ring->key){
        successor = ring;
        // 更新 finger_table，与新节点组成一个环
        for (int i = 1; i < KEY_BITS; ++i){
            start = (ring->key + (1 << (i-1))) % KEY_SPACE;
            if (key_in_right_closed_interval(start, ring->key, node->key)){
                // start 在 node 与新 node 之间
                ring->finger_table[i] = node;
            }
        }
        
    }else {
        successor = find_successor(ring, node->key);
        if (node->key == successor->key){
            __xlogd("duplicate key=%u successor->key=%u\n", node->key, successor->key);
            return -1;
        }
    }

    // 设置 node 的前继
    node->predecessor = successor->predecessor;
    // 设置 node 的后继的前继等于 node
    successor->predecessor = node;
    // 初始化 finger_table
    node->finger_table[1] = successor;
    for (int i = 1; i < KEY_BITS-1; ++i){
        start = (node->key + (1 << i)) % KEY_SPACE;
        if (key_in_left_closed_interval(start, node->key, node->finger_table[i]->key)){
            // start 在 n 与 finger[i] 之间，所以 n + start 小于 finger[i], 所以 finger[i] 是 start 的后继节点
            node->finger_table[i+1] = node->finger_table[i];
        }else {
            node->finger_table[i+1] = find_successor(ring, start);
        }
    }

    // 更新所有需要指向 node 的节点
    update_others(node);

    __xlogd("node_join exit 0x%x\n", node);

    return 0;
}

void node_remove(XChord_Ptr ring, uint32_t key)
{
    __xlogd("node_remove enter %u\n", key);
    uint32_t start;
    XChord_Ptr node, prev, next, finger_node, successor;
    successor = find_successor(ring, key);
    if (successor->key == key){
        node = successor;
        successor = node->finger_table[1];
    }else {
        node = successor->predecessor;
    }
    __xlogd("node_remove key=%u node=%u suc=%u\n", key, node->key, successor->key);
    successor->predecessor = node->predecessor;

    for (int i = 1; i < KEY_BITS; ++i){
        start = (node->key - (1 << (i-1))) % KEY_SPACE;
        __xlogd("node_remove start %u\n", start);
        // 找到与 n 至少相隔 2**m-1 的前继节点
        prev = find_predecessor(node, start);
        __xlogd("node_remove [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
            prev->key, node->key, i, prev->finger_table[i]->key);
        next = prev->finger_table[1];
        if (next->key == start){
            // start 的位置正好指向一个 node，所以这个 node 的路由表项可能会指向当前 node
            finger_node = next->finger_table[i];
            if (finger_node->key == node->key){
                next->finger_table[i] = successor;
            }
        }
        // n 大于等于 prev 并且小于 prev 的路由表中第 i 项，所以 n 要替换路由表的第 i 项
        finger_node = prev->finger_table[i];
        while (finger_node->key == node->key){
            __xlogd("node_remove prev->finder[%d]->key=%u\n", i, node->key);
            prev->finger_table[i] = successor;
            prev = prev->predecessor;
            finger_node = prev->finger_table[i];
            __xlogd("node_remove [prev->key=%u node->key=%u prev->finger[%d]->key=%u)\n", 
                prev->key, node->key, i, prev->finger_table[i]->key);                
        }
    }

    free(node);
    __xlogd("node_remove exit\n");
}


typedef struct xpeer_task {
    index_node_t node;
    xl_ptr req;
    struct xpeer_ctx *pctx;
    struct xpeer_task *prev, *next;
    void (*enter)(struct xpeer_task*, struct xpeer_ctx*);
}*xpeer_task_ptr;

typedef struct xpeer_ctx {
    uuid_node_t node;
    uint16_t reconnected;
    xchannel_ptr channel;
    struct xserver *server;
    xl_ptr req;
    xlkv_t xlparser;
    struct xpeer_task tasklist;
    XChord_Ptr xchord;
    void (*release)(struct xpeer_ctx*);
}*xpeer_ctx_ptr;

typedef struct xserver{
    int sock;
    __atom_bool runnig;
    struct __xipaddr addr;
    char ip[16];
    uint16_t port;
    XChord_Ptr ring;
    xmsger_ptr msger;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid;
    struct xmsgercb listener;
    uuid_list_t uuid_table;
    uint64_t task_count;
    index_table_t task_table;
    search_table_t api_tabel;
    char password[16];
    xpeer_ctx_ptr commander;
}*xserver_ptr;


typedef struct xmsg_ctx {
    void *msg;
    xpeer_ctx_ptr pctx;
}xmsg_ctx_t;

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    if (pctx->xlparser.head){
        free(pctx->xlparser.head);
        pctx->xlparser.head = NULL;
    }
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "break");
    xmsg_ctx_t mctx;
    mctx.pctx = pctx;
    mctx.msg = xl.head;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_channel_timeout(xmsgercb_ptr listener, xchannel_ptr channel, xmessage_ptr resendmsg)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    char ip[16];
    uint16_t port;
    __xipaddr_ptr addr = xmsger_get_channel_ipaddr(channel);
    __xapi->udp_ipaddr_to_host(addr, ip, 16, &port);
    xlkv_t req = xl_maker(1024);
    xl_add_word(&req, "api", "timeout");
    xl_add_word(&req, "ip", "ip");
    xl_add_number(&req, "port", "port");
    xl_add_ptr(&req, "resendmsg", resendmsg);
    if (pctx->req){
        xl_add_ptr(&req, "req", pctx->req);
    }
    xmsg_ctx_t mctx;
    mctx.pctx = pctx;
    mctx.msg = req.head;
    xpipe_write(pctx->server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_to_peer(xmsgercb_ptr listener, xchannel_ptr channel, void* msg, size_t len)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    free(msg);
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, void *msg, size_t len)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xmsg_ctx_t mctx;
    mctx.pctx = xmsger_get_channel_ctx(channel);
    mctx.msg = msg;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static inline xpeer_ctx_ptr xpeer_ctx_create(xserver_ptr server, xchannel_ptr channel)
{
    xpeer_ctx_ptr pctx = (xpeer_ctx_ptr)calloc(1, sizeof(struct xpeer_ctx));
    pctx->server = server;
    pctx->channel = channel;
    pctx->node.next = &pctx->node;
    pctx->node.prev = &pctx->node;
    pctx->tasklist.next = &pctx->tasklist;
    pctx->tasklist.prev = &pctx->tasklist;
    return pctx;
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    pctx->reconnected = 0;
    pctx->channel = channel;
    if (pctx->req){
        xmsg_ctx_t mctx;
        mctx.msg = pctx->req;
        pctx->req = NULL;
        mctx.pctx = pctx;
        xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xpeer_ctx_create(listener->ctx, channel);
    xmsger_set_channel_ctx(channel, pctx);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}


static void* listen_loop(void *ptr)
{
    xserver_ptr server = (xserver_ptr)ptr;
    while (__is_true(server->runnig))
    {
        __xlogd("recv_loop >>>>-----> listen enter\n");
        __xapi->udp_listen(server->sock);
        __xlogd("recv_loop >>>>-----> listen exit\n");
        xmsger_notify(server->msger, 5000000000);
    }
    return NULL;
}

inline static xpeer_task_ptr add_task(xpeer_ctx_ptr pctx, void(*enter)(xpeer_task_ptr, xpeer_ctx_ptr))
{
    __xlogd("add_task enter\n");
    xpeer_task_ptr task = (xpeer_task_ptr)malloc(sizeof(struct xpeer_task));
    task->pctx = pctx;
    task->node.index = pctx->server->task_count++;
    task->enter = enter;
    task->next = pctx->tasklist.next;
    task->prev = &pctx->tasklist;
    task->prev->next = task;
    task->next->prev = task;
    index_table_add(&pctx->server->task_table, &task->node);
    __xlogd("add_task exit\n");
    return task;
}

inline static xpeer_task_ptr remove_task(xpeer_task_ptr task)
{
    xserver_ptr server = task->pctx->server;
    __xlogd("remove_task tid=%lu\n", task->node.index);
    index_table_del(&server->task_table, task);
    task->prev->next = task->next;
    task->next->prev = task->prev;
    free(task->req);
    free(task);
}

static void chord_remove(xpeer_ctx_ptr pctx)
{
    node_remove(pctx->server->ring, pctx->xchord->key);
}

static void chord_notify(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_notify enter\n");

    uint64_t key = xl_find_number(&pctx->xlparser, "key");
    XChord_Ptr node = node_create(key);
    node_join(pctx->server->ring, node);
    node->channel = pctx->channel;
    pctx->xchord = node;
    pctx->release = chord_remove;
    node_print_all(pctx->server->ring);

    __xlogd("chord_notify exit\n");
}

static void chord_leave(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_leave enter\n");

    uint64_t key = xl_find_number(&pctx->xlparser, "key");
    node_remove(pctx->server->ring, key);
    node_print_all(pctx->server->ring);

    __xlogd("chord_leave exit\n");
}

static void chord_join(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_join enter\n");

    uint32_t key = xl_find_number(&pctx->xlparser, "key");
    XChord_Ptr node = node_create(key);
    node_join(pctx->server->ring, node);
    node->channel = pctx->channel;
    pctx->xchord = node;
    pctx->release = chord_remove;

    xlkv_t req = xl_maker(0);
    xl_add_word(&req, "api", "chord_notify");
    xl_add_word(&req, "ip", pctx->server->ip);
    xl_add_number(&req, "port", pctx->server->port);
    xl_add_number(&req, "key", pctx->server->ring->key);
    xmsger_send_message(pctx->server->msger, pctx->channel, req.head, req.wpos);
    node_print_all(pctx->server->ring);

    __xlogd("chord_join exit\n");
}

static void api_forward(xpeer_ctx_ptr pctx)
{
    __xlogd("api_forward enter\n");
    uint32_t key = xl_find_number(&pctx->xlparser, "key");
    __xlogd("api_forward find key=%u\n", key);
    XChord_Ptr node = find_successor(pctx->server->ring, key);
    __xlogd("api_forward successor node key=%u\n", node->key);
    if (node == pctx->server->ring){
        char *msg = xl_find_word(&pctx->xlparser, "text");
        __xlogd("receive >>>>>---------------------------------> text: %s\n", msg);
    }else {
        xmsger_send_message(pctx->server->msger, node->channel, pctx->xlparser.head, pctx->xlparser.wpos);
        pctx->xlparser.head = NULL;
    }
    __xlogd("api_forward exit\n");
}

static void api_login(xpeer_ctx_ptr pctx)
{
    if (pctx->node.hash_key != 0){
        return;
    }
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");
    pctx->node.hash_key = xl_find_number(&pctx->xlparser, "key");
    xl_ptr xb = xl_find(&pctx->xlparser, "uuid");
    uint64_t *uuid = __xl2o(xb);
    for (int i = 0; i < 4; i++){
        pctx->node.uuid[i] = uuid[i];
    }
    uuid_list_add(&pctx->server->uuid_table, &pctx->node);
    __xlogd("uuid_tree_add tree count=%lu\n", pctx->server->uuid_table.count);

    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "login");
    xl_add_number(&res, "tid", tid);
    xl_add_number(&res, "code", 200);
    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);
}

static void api_logout(xpeer_ctx_ptr pctx)
{
    if (pctx->node.hash_key == 0){
        return;
    }
    uuid_node_t *node = uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    pctx->node.hash_key = 0;
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");
    __xlogd("node key=%lu tid=%lu\n", node->hash_key, tid);
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "logout");
    xl_add_number(&res, "tid", tid);
    xl_add_number(&res, "code", 200);
    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);    
}

static void api_break(xpeer_ctx_ptr pctx)
{
    xserver_ptr server = pctx->server;
    __xlogd("api_break >>>>---------------------------> free peer ctx node %lu\n", pctx->node.hash_key);
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    __xlogd("api_break >>>>---------------------------> uuid tree count %lu\n", pctx->server->uuid_table.count);
    if (pctx->tasklist.next != &pctx->tasklist){
        xpeer_task_ptr next = pctx->tasklist.next;
        while (next != &pctx->tasklist){
            xpeer_task_ptr next_task = next->next;
            remove_task(next);
            next = next_task;
        }
    }
    __xlogd("api_break >>>>---------------------------> peerctx->release=%p\n", pctx->release);
    if (pctx->release){
        __xlogd("api_break >>>>---------------------------> peerctx->release\n");
        pctx->release(pctx);
    }
    if (pctx->req){
        free(pctx->req);
    }
    free(pctx);
}

static void api_timeout(xpeer_ctx_ptr pctx)
{
    const char *ip = xl_find_word(&pctx->xlparser, "ip");
    uint16_t port = xl_find_number(&pctx->xlparser, "port");
    xmessage_ptr resendmsg = xl_find_ptr(&pctx->xlparser, "resendmsg");
    xl_ptr req = xl_find_ptr(&pctx->xlparser, "req");

    // 只有服务器与服务器之间尝试重连
    if (pctx->xchord != NULL){

        if (pctx->reconnected < 3){
            pctx->reconnected++;
            struct __xipaddr addr;
            __xapi->udp_host_to_ipaddr(ip, port, &addr);
            pctx->req = req;
            xmsger_reconnect(pctx->server->msger, &addr, pctx, resendmsg);

        }else {

            xmessage_ptr next;
            while (resendmsg){
                next = resendmsg;
                if (resendmsg->sid == 0){
                    xlkv_t kv = xl_parser(resendmsg->data);
                    char *api = xl_find_word(&kv, "api");
                    if (mcompare(api, "forward", slength("forward")) == 0){
                        xl_ptr uuid = xl_find(&kv, "uuid");
                        uint64_t key = xl_find_number(&kv, "key");
                        // 通知消息未送达
                    }
                }
                if (resendmsg->data){
                    free(resendmsg->data);
                }
                free(resendmsg);
                resendmsg = next;
            }

            // 记录未完成任务
            api_break(pctx);
        }

    }else {

        // 记录未完成任务
        api_break(pctx);
    }
}

static void api_response(xpeer_ctx_ptr pctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");
    __xlogd("api_response tid=%lu\n", tid);
    xpeer_task_ptr task = (xpeer_task_ptr )index_table_find(&pctx->server->task_table, tid);
    __xlogd("api_response 1\n");
    if (task){
        __xlogd("api_response 2\n");
        task->enter(task, pctx);
    }
    __xlogd("api_response exit\n");
}

static void command(xpeer_ctx_ptr pctx)
{
    if (pctx != pctx->server->commander){
        return;
    }

    char *cmd = xl_find_word(&pctx->xlparser, "cmd");
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");

    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_number(&res, "tid", tid);

    if (mcompare(cmd, "chord_list", slength("chord_list")) == 0){

        xl_add_word(&res, "req", "chord_list");

        uint64_t lpos = xl_hold_list(&res, "nodes");

        XChord_Ptr node = pctx->server->ring;
        do {
            char ip[16];
            uint16_t port;
            __xipaddr_ptr addr = xmsger_get_channel_ipaddr(node->channel);
            __xapi->udp_ipaddr_to_host(addr, ip, 16, &port);
            uint64_t kvpos = xl_list_hold_kv(&res);
            xl_add_word(&res, "ip", ip);
            xl_add_number(&res, "port", port);
            xl_add_number(&res, "key", node->key);
            xl_list_save_kv(&res, kvpos);
            node = node->predecessor;
        }while (node != pctx->server->ring);

        xl_save_list(&res, lpos);

        xl_add_number(&res, "code", 200);

    }else if (mcompare(cmd, "chord_invite", slength("chord_invite")) == 0){

        xl_add_word(&res, "req", "chord_invite");

        xl_ptr xlnode;
        xl_ptr xlnodes = xl_find(&pctx->xlparser, "nodes");
        xlkv_t kvnodes = xl_parser(xlnodes);

        while ((xlnode = xl_list_next(&kvnodes)) != NULL)
        {
            xlkv_t kvnode = xl_parser(xlnode);
            char *ip = xl_find_word(&kvnode, "ip");
            uint16_t port = xl_find_number(&kvnode, "port");
            uint32_t key = xl_find_number(&kvnode, "key");
            
            xlkv_t req = xl_maker(1024);
            xl_add_word(&req, "api", "chord_join");
            xl_add_word(&req, "ip", ip);
            xl_add_number(&req, "port", port);
            xl_add_number(&req, "key", key);
            xpeer_ctx_ptr node_pctx = xpeer_ctx_create(pctx->server, NULL);
            node_pctx->req = req.head;
            struct __xipaddr addr;
            __xapi->udp_host_to_ipaddr(ip, port, &addr);
            xmsger_connect(pctx->server->msger, &addr, node_pctx);
        }

        xl_add_number(&res, "code", 200);

    }else if (mcompare(cmd, "chord_leave", slength("chord_leave")) == 0){

        xl_add_word(&res, "req", "chord_leave");

        XChord_Ptr node = pctx->server->ring->predecessor;
        while (node != pctx->server->ring) {
            xlkv_t req = xl_maker(0);
            xl_add_word(&req, "api", "chord_leave");
            xl_add_number(&req, "key", pctx->server->ring->key);
            xmsger_send_message(pctx->server->msger, node->channel, req.head, req.wpos);
            node_remove(pctx->server->ring, node->key);
            node = node->predecessor;
        }

        xl_add_number(&res, "code", 200);
    }

    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);
}

static void hello(xpeer_ctx_ptr pctx)
{
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");
    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "commander_login");
    xl_add_number(&res, "tid", tid);
    const char *password = xl_find_word(&pctx->xlparser, "password");
    if (mcompare(pctx->server->password, password, slength(password)) == 0){
        pctx->server->commander = pctx;
        xl_add_number(&res, "code", 200);
    }else {
        xl_add_number(&res, "code", 400);
    }    
    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);
}

static void bye(xpeer_ctx_ptr pctx)
{
    uint64_t tid = xl_find_number(&pctx->xlparser, "tid");
    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "commander_logout");
    xl_add_number(&res, "tid", tid);
    const char *password = xl_find_word(&pctx->xlparser, "password");
    if (mcompare(pctx->server->password, password, slength(password)) == 0){
        pctx->server->commander = NULL;
        xl_add_number(&res, "code", 200);
    }else {
        xl_add_number(&res, "code", 400);
    }    
    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);
}


typedef void(*api_task_enter)(xpeer_ctx_ptr *pctx);

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xmsg_ctx_t mctx;
    xpeer_ctx_ptr pctx;
    xserver_ptr server = (xserver_ptr)ptr;

    while (xpipe_read(server->task_pipe, &mctx, sizeof(xmsg_ctx_t)) == sizeof(xmsg_ctx_t))
    {
        pctx = mctx.pctx;
        // xl_printf(mctx.msg);
        pctx->xlparser = xl_parser((xl_ptr)mctx.msg);
        const char *api = xl_find_word(&pctx->xlparser, "api");
        api_task_enter enter = search_table_find(&pctx->server->api_tabel, api);
        if (enter){
            enter(pctx);
        }
        if (pctx->xlparser.head){
            free(pctx->xlparser.head);
            pctx->xlparser.head = NULL;
        }
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

static xserver_ptr g_server = NULL;

static void __sigint_handler()
{
    if(g_server){
        __set_false(g_server->runnig);
        if (g_server->sock > 0){
            int sock = __xapi->udp_open();
            for (int i = 0; i < 10; ++i){
                __xapi->udp_sendto(sock, &g_server->addr, &i, sizeof(int));
            }
            __xapi->udp_close(sock);
        }
    }
}

extern void sigint_setup(void (*handler)());
#include <arpa/inet.h>
int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/server/log", NULL);

    xserver_ptr server = (xserver_ptr)calloc(1, sizeof(struct xserver));
    mcopy(server->password, "123456", slength("123456"));
    mcopy(server->ip, argv[1], slength(argv[1]));
    server->port = atoi(argv[2]);
    server->ring = node_create(atoi(argv[3]));

    __xlogi("ip=%s port=%u key=%u\n", server->ip, server->port, server->ring->key);

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

    // const char *host = "127.0.0.1";
    // const char *host = "47.99.146.226";
    // const char *host = "18.138.128.58";
    // uint16_t port = atoi(argv[1]);
    // uint16_t port = 9256;

    uuid_list_init(&server->uuid_table);
    index_table_init(&server->task_table, 256);
    search_table_init(&server->api_tabel, 256);

    search_table_add(&server->api_tabel, "res", api_response);
    search_table_add(&server->api_tabel, "login", api_login);
    search_table_add(&server->api_tabel, "logout", api_logout);
    search_table_add(&server->api_tabel, "forward", api_forward);
    search_table_add(&server->api_tabel, "chord_join", chord_join);
    search_table_add(&server->api_tabel, "chord_notify", chord_notify);
    search_table_add(&server->api_tabel, "chord_leave", chord_leave);
    search_table_add(&server->api_tabel, "command", command);
    search_table_add(&server->api_tabel, "hello", hello);
    search_table_add(&server->api_tabel, "bye", bye);
    search_table_add(&server->api_tabel, "break", api_break);
    search_table_add(&server->api_tabel, "timeout", api_timeout);

    xmsgercb_ptr listener = &server->listener;

    listener->ctx = server;
    listener->on_channel_to_peer = on_connection_to_peer;
    listener->on_channel_from_peer = on_connection_from_peer;
    listener->on_channel_break = on_channel_break;
    listener->on_channel_timeout = on_channel_timeout;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;

    server->sock = __xapi->udp_open();
    __xbreak(server->sock < 0);
    __xbreak(!__xapi->udp_host_to_ipaddr(NULL, 9256, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    // struct sockaddr_in server_addr; 
    // socklen_t addr_len = sizeof(server_addr);
    // __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    // __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    // __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));
    // __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", 9256, &server->addr));

    server->task_pipe = xpipe_create(sizeof(xmsg_ctx_t) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener, server->sock);

    listen_loop(server);

    if (server->task_pipe){
        xpipe_break(server->task_pipe);
    }

    if (server->task_pid){
        __xapi->process_free(server->task_pid);
    }

    if (server->task_pipe){
        while (xpipe_readable(server->task_pipe) > 0){
            // TODO 清理管道
        }
        xpipe_free(&server->task_pipe);
    }
    
    xmsger_free(&server->msger);

    if (server->sock > 0){
        __xapi->udp_close(server->sock);
    }

    search_table_clear(&server->api_tabel);
    index_table_clear(&server->task_table);
    uuid_list_clear(&server->uuid_table);
    
    free(server->ring);
    free(server);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
