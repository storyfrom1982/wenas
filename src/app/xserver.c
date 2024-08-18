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
    struct __xipaddr ip;
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
    xlkv_t taskctx;
    struct xpeer_ctx *peerctx;
    struct xpeer_task *prev, *next;
    void (*enter)(struct xpeer_task*, struct xpeer_ctx*);
}*xpeer_task_ptr;

typedef struct xpeer_ctx {
    uint16_t reconnect_count;
    uuid_node_t node;
    xchannel_ptr channel;
    struct xpeer *server;
    xlkv_t msgparser;
    struct xpeer_task tasklist;
    XChord_Ptr xchord;
    void (*release)(struct xpeer_ctx*);
}*xpeer_ctx_ptr;

typedef struct xpeer{
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
}*xpeer_ptr;


typedef struct xmsg_ctx {
    void *msg;
    xpeer_ctx_ptr peerctx;
}xmsg_ctx_t;

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    if (pctx->msgparser.head){
        free(pctx->msgparser.head);
        pctx->msgparser.head = NULL;
    }
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "break");
    xmsg_ctx_t mctx;
    mctx.peerctx = pctx;
    mctx.msg = xl.head;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_channel_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    if (pctx->msgparser.head){
        free(pctx->msgparser.head);
        pctx->msgparser.head = NULL;
    }
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "timeout");
    xmsg_ctx_t mctx;
    mctx.peerctx = pctx;
    mctx.msg = xl.head;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
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
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xmsg_ctx_t mctx;
    mctx.peerctx = xmsger_get_channel_ctx(channel);
    mctx.msg = msg;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static inline xpeer_ctx_ptr xpeer_ctx_create(xpeer_ptr server, xchannel_ptr channel)
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
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    pctx->channel = channel;
    if (pctx->msgparser.head){
        xmsg_ctx_t mctx;
        mctx.msg = pctx->msgparser.head;
        pctx->msgparser.head = NULL;
        mctx.peerctx = pctx;
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
    xpeer_ptr server = (xpeer_ptr)ptr;
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
    task->peerctx = pctx;
    task->node.index = pctx->server->task_count++;
    task->enter = enter;
    task->next = pctx->tasklist.next;
    task->prev = &pctx->tasklist;
    task->prev->next = task;
    task->next->prev = task;
    task->taskctx = xl_maker(1024);
    index_table_add(&pctx->server->task_table, &task->node);
    __xlogd("add_task exit\n");
    return task;
}

inline static xpeer_task_ptr remove_task(xpeer_task_ptr task)
{
    xpeer_ptr server = task->peerctx->server;
    __xlogd("remove_task tid=%lu\n", task->node.index);
    index_table_del(&server->task_table, task);
    task->prev->next = task->next;
    task->next->prev = task->prev;
    free(task->taskctx.head);
    free(task);
}

#include <arpa/inet.h>

void release_chord_node(xpeer_ctx_ptr pctx)
{
    node_remove(pctx->server->ring, pctx->xchord->key);
}

static void task_chord_add(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    __xlogd("task_chord_add enter\n");

    uint64_t res_code = xl_find_number(&pctx->msgparser, "code");
    __xlogd("task_chord_add res code %lu\n", res_code);
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    const char* ip = xl_find_word(&pctx->msgparser, "ip");
    uint64_t port = xl_find_number(&pctx->msgparser, "port");
    uint64_t key = xl_find_number(&pctx->msgparser, "key");

    if (res_code == 200){
        __xlogd("res_chord_join_invite ip=%s\n", ip);
        XChord_Ptr node = node_create(key);
        __xapi->udp_host_to_ipaddr(ip, port, &node->ip);
        node->channel = pctx->channel;
        int ret = node_join(pctx->server->ring, node);
        pctx->xchord = node;
        pctx->release = release_chord_node;
        node_print_all(pctx->server->ring);
    }

    remove_task(task);
    __xlogd("task_chord_add exit\n");
}

static void task_chord_join(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    __xlogd("task_chord_join enter\n");
    xpeer_ptr server = task->peerctx->server;

    xlkv_t xl = xl_maker(1024);

    uint64_t res_code = xl_find_number(&pctx->msgparser, "code");
    __xlogd("res_chord_join_invite res code %lu\n", res_code);
    xlkv_t parser = xl_parser(task->taskctx.head);
    uint64_t tid = xl_find_number(&parser, "tid");
    const char* ip = xl_find_word(&parser, "ip");
    uint64_t port = xl_find_number(&parser, "port");
    uint64_t key = xl_find_number(&parser, "key");

    if (res_code == 200){
        __xlogd("res_chord_join_invite ip=%s\n", ip);
        XChord_Ptr node = node_create(key);
        __xapi->udp_host_to_ipaddr(ip, port, &node->ip);
        node->channel = pctx->channel;
        int ret = node_join(server->ring, node);
        pctx->xchord = node;
        pctx->release = release_chord_node;
        node_print_all(server->ring);
        
        xl_add_word(&xl, "api", "res");
        xl_add_number(&xl, "tid", tid);
        xl_add_number(&xl, "code", res_code);

        uint64_t lpos = xl_hold_list(&xl, "nodes");
        node = server->ring;
        do
        {
            __xlogd("res_chord_join_invite 2\n");
            uint64_t tpos = xl_list_hold_kv(&xl);
            xl_add_number(&xl, "key", node->key);
            xl_list_save_kv(&xl, tpos);
            node = node->predecessor;
        }while (node != server->ring);
        xl_save_list(&xl, lpos);

    }else {

        __xlogd("res_chord_join_invite 3\n");
        xl_add_word(&xl, "api", "res");
        xl_add_number(&xl, "tid", tid);
        xl_add_number(&xl, "code", res_code);
    }

    __xlogd("res_chord_join_invite 4\n"); 
    xmsger_send_message(task->peerctx->server->msger, task->peerctx->channel, xl.head, xl.wpos);

    __xlogd("res_chord_join_invite 5\n"); 
    remove_task(task);

    __xlogd("task_chord_join exit\n");
}

static void req_chord_add(xpeer_ctx_ptr pctx)
{
    __xlogd("req_chord_add enter\n");
    xpeer_task_ptr task = add_task(pctx, task_chord_add);
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "chord_add");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_number(&xl, "key", pctx->server->ring->key);
    xmsger_send_message(pctx->server->msger, pctx->channel, xl.head, xl.wpos);
    __xlogd("req_chord_add exit\n");
}

static void req_chord_invite(xpeer_ctx_ptr pctx)
{
    __xlogd("req_chord_invite enter\n");
    xpeer_ptr server = pctx->server;
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    uint32_t key = xl_find_number(&pctx->msgparser, "key");
    __xlogd("add key=%u\n", key);

    xlkv_t maker = xl_maker(0);
    xl_add_word(&maker, "api", "chord_invite");
    xl_add_number(&maker, "tid", tid);
    uint64_t pos = xl_hold_list(&maker, "nodes");
    XChord_Ptr n = pctx->server->ring->predecessor;
    while (n != pctx->server->ring) {
        uint64_t tpos = xl_list_hold_kv(&maker);
        // __xipaddr_ptr addr = xmsger_get_channel_ipaddr(n->channel);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &n->ip.ip, ip, sizeof(ip));
        uint16_t port = ntohs(n->ip.port);
        __xlogd("req_chord_invite ip=%s\n", ip);
        xl_add_word(&maker, "ip", ip);
        xl_add_number(&maker, "port", port);
        xl_add_number(&maker, "key", n->key);
        xl_list_save_kv(&maker, tpos);
        n = n->predecessor;
    }

    uint64_t tpos = xl_list_hold_kv(&maker);
    __xlogd("req_chord_invite server ip=%s\n", pctx->server->ip);
    xl_add_word(&maker, "ip", pctx->server->ip);
    xl_add_number(&maker, "port",pctx->server->port);
    xl_add_number(&maker, "key", pctx->server->ring->key);
    xl_list_save_kv(&maker, tpos);
    xl_save_list(&maker, pos);

    xl_printf(maker.head);

    xmsger_send_message(pctx->server->msger, pctx->channel, maker.head, maker.wpos);

    __xlogd("req_chord_invite exit\n");
}

static void api_chord_add(xpeer_ctx_ptr pctx)
{
    __xlogd("api_chord_add enter\n");
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    const char* ip = xl_find_word(&pctx->msgparser, "ip");
    uint64_t port = xl_find_number(&pctx->msgparser, "port");    
    uint32_t key = xl_find_number(&pctx->msgparser, "key");
    XChord_Ptr node = node_create(key);
    node->channel = pctx->channel;
    __xapi->udp_host_to_ipaddr(ip, port, &node->ip);
    node_join(pctx->server->ring, node);
    pctx->xchord = node;
    pctx->release = release_chord_node;

    xlkv_t xl = xl_maker(0);
    xl_add_word(&xl, "api", "res");
    xl_add_number(&xl, "code", 200);
    xl_add_number(&xl, "tid", tid);
    xl_add_word(&xl, "ip", pctx->server->ip);
    xl_add_number(&xl, "port", pctx->server->port);
    xl_add_number(&xl, "key", pctx->server->ring->key);
    xmsger_send_message(pctx->server->msger, pctx->channel, xl.head, xl.wpos);

    __xlogd("api_chord_add exit\n");
}

static void api_chord_invite(xpeer_ctx_ptr pctx)
{
    __xlogd("api_chord_invite enter\n");
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    xl_ptr nodes = xl_find(&pctx->msgparser, "nodes");

    xl_ptr xl_node;
    xlkv_t node_list = xl_parser(nodes);

    __xipaddr_ptr addr = xmsger_get_channel_ipaddr(pctx->channel);

    while ((xl_node = xl_list_next(&node_list)) != NULL)
    {
        xlkv_t parser = xl_parser(xl_node);
        char *ip = xl_find_word(&parser, "ip");
        uint16_t port = xl_find_number(&parser, "port");
        uint32_t key = xl_find_number(&parser, "key");

        struct __xipaddr node_addr;
        __xapi->udp_host_to_ipaddr(ip, port, &node_addr);
        
        if (node_addr.ip == addr->ip){
            XChord_Ptr node = node_create(key);
            node->channel = pctx->channel;
            node_join(pctx->server->ring, node);
            node->ip = node_addr;
            pctx->xchord = node;
            pctx->release = release_chord_node;
        }else {
            xlkv_t xl = xl_maker(1024);
            xl_add_word(&xl, "api", "req_chord_add");
            xl_add_word(&xl, "ip", ip);
            xl_add_number(&xl, "port", port);
            xl_add_number(&xl, "key", key);
            xpeer_ctx_ptr peerctx = xpeer_ctx_create(pctx->server, NULL);
            peerctx->msgparser = xl;
            xmsger_connect(pctx->server->msger, ip, port, peerctx);
        }
    }

    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "res");
    xl_add_number(&xl, "tid", tid);
    xl_add_number(&xl, "code", 200);
    xl_add_number(&xl, "key", pctx->server->ring->key);
    xmsger_send_message(pctx->server->msger, pctx->channel, xl.head, xl.wpos);

    node_print_all(pctx->server->ring);
    __xlogd("api_chord_invite exit\n");
}

static void api_chord_join(xpeer_ctx_ptr pctx)
{
    xpeer_ptr server = pctx->server;
    __xlogd("api_chord_join enter\n");
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    char *ip = xl_find_word(&pctx->msgparser, "ip");
    __xlogd("api_chord_join ip=%s\n", ip);
    uint64_t port = xl_find_number(&pctx->msgparser, "port");
    __xlogd("api_chord_join port=%u\n", port);
    uint64_t key = xl_find_number(&pctx->msgparser, "key");
    __xlogd("api_chord_join key=%u\n", key);

    __xlogd("api_chord_join 1\n");
    xpeer_task_ptr task = add_task(pctx, task_chord_join);
    xl_add_number(&task->taskctx, "tid", tid);
    xl_add_word(&task->taskctx, "ip", ip);
    xl_add_number(&task->taskctx, "port", port);
    xl_add_number(&task->taskctx, "key", key);

    __xlogd("api_chord_join 2\n");
    
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "req_chord_invite");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_number(&xl, "key", key);

    __xlogd("api_chord_join 3\n");
    xpeer_ctx_ptr peerctx = xpeer_ctx_create(pctx->server, NULL);
    peerctx->msgparser = xl;
    xmsger_connect(pctx->server->msger, ip, port, peerctx);
    __xlogd("api_chord_join exit\n");
}

static void api_messaging(xpeer_ctx_ptr pctx)
{
    __xlogd("api_messaging enter\n");
    uint32_t key = xl_find_number(&pctx->msgparser, "key");
    __xlogd("api_messaging find key=%u\n", key);
    XChord_Ptr node = find_successor(pctx->server->ring, key);
    __xlogd("api_messaging successor node key=%u\n", node->key);
    if (node == pctx->server->ring){
        char *msg = xl_find_word(&pctx->msgparser, "text");
        __xlogd("receive >>>>>---------------------------------> text: %s\n", msg);
    }else {
        xmsger_send_message(pctx->server->msger, node->channel, pctx->msgparser.head, pctx->msgparser.wpos);
        pctx->msgparser.head = NULL;
    }
    __xlogd("api_messaging exit\n");
}

static void api_login(xpeer_ctx_ptr pctx)
{
    if (pctx->node.hash_key != 0){
        return;
    }
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    pctx->node.hash_key = xl_find_number(&pctx->msgparser, "key");
    xl_ptr xb = xl_find(&pctx->msgparser, "uuid");
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
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    __xlogd("node key=%lu tid=%lu\n", node->hash_key, tid);
    uuid_list_del(&pctx->server->uuid_table, &pctx->node);
    xlkv_t res = xl_maker(1024);
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "logout");
    xl_add_number(&res, "tid", tid);
    xl_add_number(&res, "code", 200);
    xmsger_send_message(pctx->server->msger, pctx->channel, res.head, res.wpos);    
}

static void api_break(xpeer_ctx_ptr peerctx)
{
    xpeer_ptr server = peerctx->server;
    __xlogd("api_break >>>>---------------------------> free peer ctx node %lu\n", peerctx->node.hash_key);
    uuid_list_del(&peerctx->server->uuid_table, &peerctx->node);
    __xlogd("api_break >>>>---------------------------> uuid tree count %lu\n", peerctx->server->uuid_table.count);
    if (peerctx->tasklist.next != &peerctx->tasklist){
        xpeer_task_ptr next = peerctx->tasklist.next;
        while (next != &peerctx->tasklist){
            xpeer_task_ptr next_task = next->next;
            remove_task(next);
            next = next_task;
        }
    }
    __xlogd("api_break >>>>---------------------------> peerctx->release=%p\n", peerctx->release);
    if (peerctx->release){
        __xlogd("api_break >>>>---------------------------> peerctx->release\n");
        peerctx->release(peerctx);
    }
    free(peerctx);
}

static void api_timeout(xpeer_ctx_ptr peerctx)
{
    if (peerctx->xchord != NULL && peerctx->reconnect_count < 3){
        peerctx->reconnect_count++;
        xmsger_reconnect(peerctx->server->msger, &peerctx->xchord->ip, peerctx);
    }else {
        api_break(peerctx);
    }
}

static void api_response(xpeer_ctx_ptr pctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_number(&pctx->msgparser, "tid");
    __xlogd("api_response tid=%lu\n", tid);
    xpeer_task_ptr task = (xpeer_task_ptr )index_table_find(&pctx->server->task_table, tid);
    __xlogd("api_response 1\n");
    if (task){
        __xlogd("api_response 2\n");
        task->enter(task, pctx);
    }
    __xlogd("api_response exit\n");
}

typedef void(*api_task_enter)(xpeer_ctx_ptr *pctx);

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xmsg_ctx_t mctx;
    xpeer_ctx_ptr pctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &mctx, sizeof(xmsg_ctx_t)) == sizeof(xmsg_ctx_t))
    {
        pctx = mctx.peerctx;
        // xl_printf(mctx.msg);
        pctx->msgparser = xl_parser((xl_ptr)mctx.msg);
        const char *api = xl_find_word(&pctx->msgparser, "api");
        api_task_enter enter = search_table_find(&pctx->server->api_tabel, api);
        if (enter){
            enter(pctx);
        }
        if (pctx->msgparser.head){
            free(pctx->msgparser.head);
            pctx->msgparser.head = NULL;
        }
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

static xpeer_ptr g_server = NULL;

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

    xpeer_ptr server = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
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
    search_table_add(&server->api_tabel, "messaging", api_messaging);
    search_table_add(&server->api_tabel, "break", api_break);
    search_table_add(&server->api_tabel, "chord_join", api_chord_join);
    search_table_add(&server->api_tabel, "chord_invite", api_chord_invite);
    search_table_add(&server->api_tabel, "chord_add", api_chord_add);
    search_table_add(&server->api_tabel, "req_chord_add", req_chord_add);
    search_table_add(&server->api_tabel, "req_chord_invite", req_chord_invite);

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
