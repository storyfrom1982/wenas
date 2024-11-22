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


typedef struct xserver {
    uint32_t key;
    xchannel_ptr channel;
    struct xserver* predecessor;
    struct xserver* finger_table[KEY_BITS];
}*xserver_t;

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

xserver_t closest_preceding_finger(xserver_t chord, uint32_t key)
{
    xserver_t finger;
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

xserver_t find_predecessor(xserver_t chord, uint32_t key)
{
    xserver_t n = chord;
    xserver_t successor = chord->finger_table[1];
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

xserver_t find_successor(xserver_t chord, uint32_t key) {
    if (key == chord->key){
        return chord;
    }
    xserver_t n = find_predecessor(chord, key);
    return n->finger_table[1];
}

void print_node(xserver_t node)
{
    __xlogd("node->key >>>>--------------------------> enter %u\n", node->key);
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%d] start_key=%u key=%u\n", 
            i, (node->key + (1 << (i-1))) % KEY_SPACE, node->finger_table[i]->key);
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    __xlogd("node->key >>>>-----------------------> exit %u\n", node->key);
}

void node_print_all(xserver_t node)
{
    __xlogd("node_print_all >>>>------------------------------------------------> enter\n");
    xserver_t n = node->predecessor;
    while (n != node){
        xserver_t t = n->predecessor;
        print_node(n);
        n = t;
        if (n == node){
            print_node(n);
            break;
        }        
    }   
    __xlogd("node_print_all >>>>------------------------------------------------> exit\n"); 
}

void update_others(xserver_t node)
{
    __xlogd("update_others enter\n");
    node_print_all(node);
    uint32_t start;
    xserver_t prev, next, finger_node;
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

xserver_t node_create(uint32_t key)
{
    __xlogd("node_create enter\n");

    xserver_t node = (xserver_t)calloc(1, sizeof(struct xserver));

    node->key = key;
    node->predecessor = node;

    for (int i = 0; i < KEY_BITS; i++) {
        node->finger_table[i] = node;
    }

    __xlogd("node_create exit\n");

    return node;
}


int node_join(xserver_t ring, xserver_t node)
{
    __xlogd("node_join >>>>--------------------> enter %u\n", node->key);

    uint32_t start;
    xserver_t successor = NULL;

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

void node_remove(xserver_t ring, uint32_t key)
{
    __xlogd("node_remove enter %u\n", key);
    uint32_t start;
    xserver_t node, prev, next, finger_node, successor;
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


typedef struct xmsg_callback {
    index_node_t node;
    xline_ptr req;
    struct xchannel_ctx *channelctx;
    struct xmsg_callback *prev, *next;
    void (*process)(struct xmsg_callback*, struct xchannel_ctx*);
}*xpeer_task_ptr;

typedef struct xchannel_ctx {
    uuid_node_t node;
    uint16_t reconnected;
    xchannel_ptr channel;
    struct xltpd *server;
    // xmsg_ptr msg;
    xlmsg_t xlparser;
    struct xmsg_callback tasklist;
    xserver_t xchord;
    void (*release)(struct xchannel_ctx*);
    void (*process)(struct xchannel_ctx*, xlmsg_ptr msg);
}*xchannel_ctx_ptr;

typedef struct xltpd{
    int sock;
    __atom_bool runnig;
    struct __xipaddr addr;
    char ip[16];
    uint16_t port;
    xserver_t ring;
    xmsger_ptr msger;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid;
    struct xmsgercb listener;
    uuid_list_t uuid_table;
    uint64_t task_count;
    index_table_t task_table;
    xtree api;
    char password[16];
}*xltpd_ptr;


static void on_disconnect(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xltpd_ptr server = listener->ctx;
    xlmsg_ptr xkv = xl_maker();
    xkv->ctx = xchannel_get_ctx(channel);
    xl_add_word(xkv, "api", "break");
    xpipe_write(server->task_pipe, &xkv, __sizeof_ptr);
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_msg_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_msg_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xltpd_ptr server = listener->ctx;
    const char *ip = xchannel_get_host(channel);
    uint16_t port = xchannel_get_port(channel);
    // xmsg_ptr msg = xmsg_maker(0, channel, xchannel_get_ctx(channel), NULL);
    __xlogd("on_msg_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_to_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr xkv)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xl_free(xkv);
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr xkv)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xltpd_ptr server = (xltpd_ptr)listener->ctx;
    xkv->ctx = xchannel_get_ctx(channel);
    xpipe_write(server->task_pipe, &xkv, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static inline xchannel_ctx_ptr xpeer_ctx_create(xltpd_ptr server, xchannel_ptr channel)
{
    xchannel_ctx_ptr pctx = (xchannel_ctx_ptr)calloc(1, sizeof(struct xchannel_ctx));
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
    xltpd_ptr server = (xltpd_ptr)listener->ctx;
    xchannel_ctx_ptr ctx = xchannel_get_ctx(channel);
    if (ctx){
        ctx->reconnected = 0;
        ctx->channel = channel;
    }else {
        ctx = xpeer_ctx_create(listener->ctx, channel);
        ctx->server = server;
        ctx->channel = channel;
        xchannel_set_ctx(channel, ctx);
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xchannel_ctx_ptr pctx = xpeer_ctx_create(listener->ctx, channel);
    xchannel_set_ctx(channel, pctx);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

inline static xpeer_task_ptr add_task(xchannel_ctx_ptr pctx, void(*enter)(xpeer_task_ptr, xchannel_ctx_ptr))
{
    __xlogd("add_task enter\n");
    xpeer_task_ptr task = (xpeer_task_ptr)malloc(sizeof(struct xmsg_callback));
    task->channelctx = pctx;
    task->node.index = pctx->server->task_count++;
    task->process = enter;
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
    xltpd_ptr server = task->channelctx->server;
    __xlogd("remove_task tid=%lu\n", task->node.index);
    index_table_del(&server->task_table, (index_node_t*)task);
    task->prev->next = task->next;
    task->next->prev = task->prev;
    free(task->req);
    free(task);
}

static void chord_remove(xchannel_ctx_ptr pctx)
{
    node_remove(pctx->server->ring, pctx->xchord->key);
}

static void chord_notify(xchannel_ctx_ptr pctx)
{
    __xlogd("chord_notify enter\n");

    const char *password = xl_find_word(&pctx->xlparser, "password");
    
    if (mcompare(pctx->server->password, password, slength(password)) == 0){
        uint64_t key = xl_find_uint(&pctx->xlparser, "key");
        xserver_t node = node_create(key);
        node_join(pctx->server->ring, node);
        node->channel = pctx->channel;
        pctx->xchord = node;
        pctx->release = chord_remove;
        node_print_all(pctx->server->ring);
    }

    __xlogd("chord_notify exit\n");
}

static void chord_leave(xchannel_ctx_ptr pctx)
{
    __xlogd("chord_leave enter\n");

    const char *password = xl_find_word(&pctx->xlparser, "password");

    if (mcompare(pctx->server->password, password, slength(password)) == 0){
        uint64_t key = xl_find_uint(&pctx->xlparser, "key");
        node_remove(pctx->server->ring, key);
        node_print_all(pctx->server->ring);
    }
    __xlogd("chord_leave exit\n");
}

static void chord_join(xchannel_ctx_ptr pctx)
{
    __xlogd("chord_join enter\n");

    const char *password = xl_find_word(&pctx->xlparser, "password");

    if (mcompare(pctx->server->password, password, slength(password)) == 0){

        uint32_t key = xl_find_uint(&pctx->xlparser, "key");
        xserver_t node = node_create(key);
        node_join(pctx->server->ring, node);
        node->channel = pctx->channel;
        pctx->xchord = node;
        pctx->release = chord_remove;

        xlmsg_ptr msg = xl_maker();
        xl_add_word(msg, "api", "chord_notify");
        xl_add_word(msg, "password", password);
        xl_add_word(msg, "ip", pctx->server->ip);
        xl_add_uint(msg, "port", pctx->server->port);
        xl_add_uint(msg, "key", pctx->server->ring->key);
        xmsger_send(pctx->server->msger, pctx->channel, msg);
        node_print_all(pctx->server->ring);
    }

    __xlogd("chord_join exit\n");
}

static void api_forward(xchannel_ctx_ptr pctx)
{
    __xlogd("api_forward enter\n");
    uint32_t key = xl_find_uint(&pctx->xlparser, "key");
    __xlogd("api_forward find key=%u\n", key);
    xserver_t node = find_successor(pctx->server->ring, key);
    __xlogd("api_forward successor node key=%u\n", node->key);
    if (node == pctx->server->ring){
        char *text = xl_find_word(&pctx->xlparser, "text");
        __xlogd("receive >>>>>---------------------------------> text: %s\n", text);
    }else {
        // xmsg_ref(pctx->msg);
        // xmsger_send_message(pctx->server->msger, node->channel, pctx->msg);
    }
    __xlogd("api_forward exit\n");
}

static void api_processor(xchannel_ctx_ptr ctx, xlmsg_ptr msg);

static void api_login(xchannel_ctx_ptr pctx)
{
    if (pctx->node.hash_key != 0){
        return;
    }
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
    pctx->node.hash_key = xl_find_uint(&pctx->xlparser, "key");
    xline_ptr xb = xl_find(&pctx->xlparser, "uuid");
    uint64_t *uuid = __xl_b2o(xb);
    for (int i = 0; i < 4; i++){
        pctx->node.uuid[i] = uuid[i];
    }
    uuid_list_add(&pctx->server->uuid_table, &pctx->node);
    __xlogd("uuid_tree_add tree count=%lu\n", pctx->server->uuid_table.count);

    pctx->process = api_processor;

    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_word(res, "req", "login");
    xl_add_uint(res, "tid", tid);
    xl_add_uint(res, "code", 200);
    xmsger_send(pctx->server->msger, pctx->channel, res);
}

static void api_logout(xchannel_ctx_ptr pctx)
{
    if (pctx->node.hash_key == 0){
        return;
    }
    uuid_node_t *node = uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    pctx->node.hash_key = 0;
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
    __xlogd("node key=%lu tid=%lu\n", node->hash_key, tid);
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_word(res, "req", "logout");
    xl_add_uint(res, "tid", tid);
    xl_add_uint(res, "code", 200);
    xmsger_send(pctx->server->msger, pctx->channel, res);
}

static void api_echo(xchannel_ctx_ptr pctx)
{
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_word(res, "req", "echo");
    xl_add_uint(res, "tid", tid);
    xl_add_word(res, "host", xchannel_get_host(pctx->channel));
    xl_add_uint(res, "port", xchannel_get_port(pctx->channel));
    xl_add_uint(res, "code", 200);
    xmsger_send(pctx->server->msger, pctx->channel, res);
}

static void api_break(xchannel_ctx_ptr pctx)
{
    xltpd_ptr server = pctx->server;
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    if (pctx->tasklist.next != &pctx->tasklist){
        xpeer_task_ptr next = pctx->tasklist.next;
        while (next != &pctx->tasklist){
            xpeer_task_ptr next_task = next->next;
            remove_task(next);
            next = next_task;
        }
    }
    if (pctx->release){
        pctx->release(pctx);
    }
    free(pctx);
}

static void api_timeout(xchannel_ctx_ptr pctx)
{
    __xlogd("api_timeout enter\n");

    const char *ip = xl_find_word(&pctx->xlparser, "ip");
    uint16_t port = xl_find_uint(&pctx->xlparser, "port");
    xlmsg_ptr msg = xl_find_ptr(&pctx->xlparser, "msg");

    if (xchannel_get_keepalive(pctx->channel)){
        if (pctx->reconnected < 3){
            pctx->reconnected++;
            struct __xipaddr addr;
            __xapi->udp_host_to_addr(ip, port, &addr);
            // TODO 找到未完成的任务
            xmsger_connect(pctx->server->msger, msg);

        }else {

            if (msg){
                xl_printf(&msg->line);
            }
            // 记录未完成任务
            api_break(pctx);
        }

    }else {
        // 记录未完成任务
        api_break(pctx);
    }
    __xlogd("api_timeout exit\n");
}

static void api_response(xchannel_ctx_ptr pctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
    __xlogd("api_response tid=%lu\n", tid);
    xpeer_task_ptr task = (xpeer_task_ptr )index_table_find(&pctx->server->task_table, tid);
    __xlogd("api_response 1\n");
    if (task){
        __xlogd("api_response 2\n");
        task->process(task, pctx);
    }
    __xlogd("api_response exit\n");
}

static void command(xchannel_ctx_ptr pctx)
{
    char *cmd = xl_find_word(&pctx->xlparser, "cmd");
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
    const char *password = xl_find_word(&pctx->xlparser, "password");

    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_uint(res, "tid", tid);
    xl_add_word(res, "req", cmd);

    if (mcompare(pctx->server->password, password, slength(password)) != 0){        
        xl_add_uint(res, "code", 400);
        xmsger_send(pctx->server->msger, pctx->channel, res);
        return;
    }

    if (mcompare(cmd, "chord_list", slength("chord_list")) == 0){

        const char *ip;
        uint16_t port;
        uint64_t lpos = xl_hold_list(res, "nodes");
        xserver_t node = pctx->server->ring->predecessor;

        while (node != pctx->server->ring) {
            ip = xchannel_get_host(node->channel);
            port = xchannel_get_port(node->channel);
            uint64_t kvpos = xl_list_hold_obj(res);
            xl_add_word(res, "ip", ip);
            xl_add_uint(res, "port", port);
            xl_add_uint(res, "key", node->key);
            xl_list_fixed_obj(res, kvpos);
            node = node->predecessor;
        }

        uint64_t kvpos = xl_list_hold_obj(res);
        xl_add_word(res, "ip", pctx->server->ip);
        xl_add_uint(res, "port", pctx->server->port);
        xl_add_uint(res, "key", pctx->server->ring->key);
        xl_list_fixed_obj(res, kvpos);

        xl_fixed_list(res, lpos);

        xl_add_uint(res, "code", 200);

    }else if (mcompare(cmd, "chord_invite", slength("chord_invite")) == 0){

        xline_ptr xlnode;
        xline_ptr xlnodes = xl_find(&pctx->xlparser, "nodes");
        xlmsg_t kvnodes = xl_parser(xlnodes);

        while ((xlnode = xl_list_next(&kvnodes)) != NULL)
        {
            xlmsg_t kvnode = xl_parser(xlnode);
            char *ip = xl_find_word(&kvnode, "ip");
            uint16_t port = xl_find_uint(&kvnode, "port");
            uint32_t key = xl_find_uint(&kvnode, "key");
            
            xlmsg_ptr req = xl_maker();
            xl_add_word(req, "api", "chord_join");
            xl_add_word(req, "host", ip);
            xl_add_uint(req, "port", port);
            xl_add_uint(req, "key", key);
            xchannel_ctx_ptr node_pctx = xpeer_ctx_create(pctx->server, NULL);
            xmsger_connect(pctx->server->msger, req);
        }

        xl_add_uint(res, "code", 200);

    }else if (mcompare(cmd, "chord_leave", slength("chord_leave")) == 0){

        xserver_t node = pctx->server->ring->predecessor;
        while (node != pctx->server->ring) {
            xlmsg_ptr req = xl_maker();
            xl_add_word(req, "api", "chord_leave");
            xl_add_uint(req, "key", pctx->server->ring->key);
            xmsger_send(pctx->server->msger, node->channel, req);
            node_remove(pctx->server->ring, node->key);
            node = node->predecessor;
        }

        xl_add_uint(res, "code", 200);
    }

    xmsger_send(pctx->server->msger, pctx->channel, res);
}

typedef void(*api_handle)(xchannel_ctx_ptr pctx);

static void api_processor(xchannel_ctx_ptr ctx, xlmsg_ptr msg)
{
    xl_printf(&msg->line);
    ctx->xlparser = xl_parser(&msg->line);
    __xlogd("task_loop 1\n");
    const char *api = xl_find_word(&ctx->xlparser, "api");
    api_handle handle = xtree_find(ctx->server->api, api, slength(api));
    if (handle){
        handle(ctx);
    }
    __xlogd("task_loop 3\n");    
}

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xlmsg_ptr msg;
    xchannel_ctx_ptr ctx;
    xltpd_ptr server = (xltpd_ptr)ptr;

    while (xpipe_read(server->task_pipe, &msg, __sizeof_ptr) == __sizeof_ptr)
    {
        ctx = msg->ctx;
        if (ctx->process){
            ctx->process(ctx, msg);
        }else {
            xl_printf(&msg->line);
            ctx->xlparser = xl_parser(&msg->line);
            const char *api = xl_find_word(&ctx->xlparser, "api");
            api_handle handle = xtree_find(server->api, api, slength(api));
            if (handle){
                handle(ctx);
            }
        }
        xl_free(msg);
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

static xltpd_ptr g_server = NULL;

static void __sigint_handler()
{
    if(g_server){
        __set_false(g_server->runnig);
    }
}

extern void sigint_setup(void (*handler)());
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/server/log", NULL);

    xltpd_ptr server = (xltpd_ptr)calloc(1, sizeof(struct xltpd));
    mcopy(server->password, "123456", slength("123456"));

	__xlogd("argc=%d\n", argc);

//    mcopy(server->ip, argv[1], slength(argv[1]));
//    server->port = atoi(argv[2]);
//    server->ring = node_create(atoi(argv[3]));
//    __xlogi("ip=%s port=%u key=%u\n", server->ip, server->port, server->ring->key);

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
    server->api = xtree_create();

    xtree_add(server->api, "res", slength("res"), api_response);
    xtree_add(server->api, "login", slength("login"), api_login);
    xtree_add(server->api, "logout", slength("logout"), api_logout);
    xtree_add(server->api, "forward", slength("forward"), api_forward);
    xtree_add(server->api, "chord_join", slength("chord_join"), chord_join);
    xtree_add(server->api, "chord_notify", slength("chord_notify"), chord_notify);
    xtree_add(server->api, "chord_leave", slength("chord_leave"), chord_leave);
    xtree_add(server->api, "command", slength("command"), command);
    xtree_add(server->api, "break", slength("break"), api_break);
    xtree_add(server->api, "timeout", slength("timeout"), api_timeout);
    xtree_add(server->api, "echo", slength("echo"), api_echo);

    xmsgercb_ptr listener = &server->listener;

    listener->ctx = server;
    listener->on_connection_to_peer = on_connection_to_peer;
    listener->on_connection_from_peer = on_connection_from_peer;
    listener->on_connection_timeout = on_msg_timeout;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;
    listener->on_msg_timeout = on_msg_timeout;
    listener->on_disconnection = on_disconnect;

    // server->sock = __xapi->udp_open();
    // __xbreak(server->sock < 0);
    // __xbreak(!__xapi->udp_host_to_addr(NULL, 9256, &server->addr));
    // __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);

    // struct sockaddr_in server_addr; 
    // socklen_t addr_len = sizeof(server_addr);
    // __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    // __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    // __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));
    // __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", 9256, &server->addr));

    server->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener);

    char str[1024];
    char input[256];
    char command[256];
    char ip[256] = {0};
    uint64_t cid, key;
    while (server->runnig)
    {
        sleep(1);
    }

    xmsger_free(&server->msger);

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

    xtree_clear(server->api, NULL);
    xtree_free(&server->api);
    index_table_clear(&server->task_table);
    uuid_list_clear(&server->uuid_table);
    
    free(server->ring);
    free(server);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
