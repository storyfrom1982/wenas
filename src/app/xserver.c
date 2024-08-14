#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>

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


typedef struct xtask {
    uint8_t tid;
    void (*req)(struct xtask*);
    void (*res)(struct xtask*);
    uint64_t len, pos;
    struct xpeer_ctx *ctx;
    struct xtask *prev, *next;
}xtask_t;

typedef struct xpeer_ctx {
    xhtnode_t node;
    xchannel_ptr channel;
    struct xpeer *server;
    xtask_t *task;
    xline_t maker;
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
    xhash_table_t api_tabel;
    xhash_tree_t uuid_table;
}*xpeer_ptr;


typedef struct xmsg_ctx {
    void *msg;
    uint64_t len;
    xpeer_ctx_ptr pctx;
}xmsg_ctx_t;

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xline_t xl = xline_maker(1024);
    xline_add_word(&xl, "api", "break");
    xmsg_ctx_t mctx;
    mctx.pctx = xmsger_get_channel_ctx(channel);
    mctx.msg = xl.byte;
    mctx.len = xl.wpos;
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
    mctx.pctx = xmsger_get_channel_ctx(channel);
    mctx.msg = msg;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xmsg_ctx_t mctx;
    mctx.pctx = xmsger_get_channel_ctx(channel);
    if (mctx.pctx->task){
        xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr ctx = (xpeer_ctx_ptr)calloc(1, sizeof(struct xpeer_ctx));
    ctx->node.value = ctx;
    ctx->channel = channel;
    ctx->server = listener->ctx;
    xmsger_set_channel_ctx(channel, ctx);
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


#include <arpa/inet.h>
static int res_join_node(xpeer_ctx_ptr task)
{
    __xlogd("msg_join_node enter\n");
    uint32_t key = xline_find_uint64(&task->maker, "key");
    __xlogd("add key=%u\n", key);

    xline_t maker = xline_maker(0);
    xline_add_word(&maker, "msg", "req");
    xline_add_word(&maker, "task", "invite");
    uint64_t pos = xline_hold_list(&maker, "nodes");
    XChord_Ptr n = task->server->ring->predecessor;
    while (n != task->server->ring) {
        uint64_t tpos = xline_list_hold_tree(&maker);
        __xipaddr_ptr addr = xmsger_get_channel_ipaddr(n->channel);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->ip, ip, sizeof(ip));
        uint16_t port = ntohs(addr->port);
        xline_add_word(&maker, "ip", ip);
        xline_add_uint64(&maker, "port", port);
        xline_add_uint64(&maker, "key", n->key);
        xline_list_save_tree(&maker, tpos);
        n = n->predecessor;
    }

    uint64_t tpos = xline_list_hold_tree(&maker);
    xline_add_word(&maker, "ip", task->server->ip);
    xline_add_uint64(&maker, "port",task->server->port);
    xline_add_uint64(&maker, "key", task->server->ring->key);
    xline_list_save_tree(&maker, tpos);

    xline_save_list(&maker, pos);
    xmsger_send_message(task->server->msger, task->channel, maker.head, maker.wpos);

    XChord_Ptr node = node_create(key);
    node->channel = task->channel;
    int ret = node_join(task->server->ring, node);
    if (ret == -1){
        return -1;
    }
    node_print_all(task->server->ring);
    __xlogd("msg_join_node exit\n");

    return 0;
}

static int req_join_node(xpeer_ctx_ptr ctx)
{
    __xlogd("req_join_node enter\n");
    char *ip = xline_find_word(&ctx->maker, "ip");
    __xlogd("add ip=%s\n", ip);
    unsigned port = xline_find_uint64(&ctx->maker, "port");
    __xlogd("add port=%u\n", port);
    uint32_t key = xline_find_uint64(&ctx->maker, "key");
    __xlogd("add key=%u\n", key);
    xtask_t *new_task = (xtask_t *)malloc(sizeof(xtask_t));
    new_task->ctx = ctx;
    new_task->res = res_join_node;
    xmsger_connect(ctx->server->msger, ip, port, new_task);
    __xlogd("req_join_node exit\n");
    return 0;
}

static int req_turn(xpeer_ctx_ptr task)
{
    __xlogd("req_turn enter\n");
    uint32_t key = xline_find_uint64(&task->maker, "key");
    __xlogd("req_turn find key=%u\n", key);
    XChord_Ptr node = find_successor(task->server->ring, key);
    __xlogd("req_turn successor node key=%u\n", node->key);
    if (node == task->server->ring){
        char *msg = xline_find_word(&task->maker, "text");
        __xlogd("receive >>>>>---------------------------------> text: %s\n", msg);
    }else {
        char *msg = xline_find_word(&task->maker, "text");
        xline_t ctx = xline_maker(0);
        xline_add_word(&ctx, "msg", "req");
        xline_add_word(&ctx, "task", "turn");
        xline_add_uint64(&ctx, "key", key);
        xline_add_word(&ctx, "text", msg);
        xmsger_send_message(task->server->msger, node->channel, ctx.head, ctx.wpos);
    }
    __xlogd("req_turn exit\n");
    return 0;
}

static int req_add_node(xpeer_ctx_ptr task)
{
    __xlogd("req_add_node enter\n");
    uint32_t key = xline_find_uint64(&task->maker, "key");
    XChord_Ptr node = node_create(key);
    node->channel = task->channel;
    node_join(task->server->ring, node);
    node_print_all(task->server->ring);
    __xlogd("req_add_node exit\n");
}

static int res_invite_node(xpeer_ctx_ptr task)
{
    __xlogd("msg_invite_node enter\n");
    XChord_Ptr node = xline_find_pointer(&task->maker, "node");
    __xlogd("msg_invite_node 1\n");
    node->channel = task->channel;
    __xlogd("msg_invite_node 2\n");
    xline_t new_node = xline_maker(0);
    __xlogd("msg_invite_node 3\n");
    xline_add_word(&new_node, "msg", "req");
    __xlogd("msg_invite_node 4\n");
    xline_add_word(&new_node, "task", "add");
    __xlogd("msg_invite_node 5\n");
    xline_add_uint64(&new_node, "key", task->server->ring->key);
    __xlogd("msg_invite_node 6\n");
    xmsger_send_message(task->server->msger, node->channel, new_node.head, new_node.wpos);
    __xlogd("msg_invite_node 7\n");
    node_print_all(task->server->ring);
    __xlogd("msg_invite_node exit\n");
    return 0;
}

static int req_invite_node(xpeer_ctx_ptr ctx)
{
    __xlogd("req_invite_node enter\n");
    xbyte_ptr nodes = xline_find(&ctx->maker, "nodes");

    xbyte_ptr xptr;
    xline_t plist = xline_parser(nodes);

    __xipaddr_ptr addr = xmsger_get_channel_ipaddr(ctx->channel);

    while ((xptr = xline_list_next(&plist)) != NULL)
    {
        xline_t node_parser = xline_parser(xptr);
        char *ip = xline_find_word(&node_parser, "ip");
        uint16_t port = xline_find_uint64(&node_parser, "port");
        uint32_t key = xline_find_uint64(&node_parser, "key");

        XChord_Ptr node = node_create(key);
        __xapi->udp_make_ipaddr(ip, port, &node->ip);
        node_join(ctx->server->ring, node);
        if (node->ip.ip == addr->ip){
            node->channel = ctx->channel;
        }else {
            xtask_t *new_task = (xtask_t *)malloc(sizeof(xtask_t));
            new_task->ctx = ctx;
            new_task->res = res_invite_node;
            xmsger_connect(ctx->server->msger, ip, port, new_task);
        }
    }
    node_print_all(ctx->server->ring);
    __xlogd("req_invite_node exit\n");
    
    return 0;
}

static void api_login(xpeer_ctx_ptr pctx)
{
    uint64_t tid = xline_find_uint64(&pctx->maker, "tid");
    pctx->node.key = xline_find_uint64(&pctx->maker, "key");
    pctx->node.uuid_len = xline_find_uint64(&pctx->maker, "len");
    xbyte_ptr xb = xline_find(&pctx->maker, "uuid");
    uint8_t *uuid = __b2d(xb);
    pctx->node.uuid = (uint8_t*)malloc(pctx->node.uuid_len);
    mcopy(pctx->node.uuid, uuid, pctx->node.uuid_len);
    xhash_tree_add(&pctx->server->uuid_table, &pctx->node);
    __xlogd("xhash_tree_add tree count=%lu\n", pctx->server->uuid_table.count);

    xline_t res = xline_maker(1024);
    xline_add_word(&res, "api", "res");
    xline_add_word(&res, "req", "login");
    xline_add_uint64(&res, "tid", tid);
    xline_add_uint64(&res, "code", 200);
    xmsger_send_message(pctx->server->msger, pctx->channel, res.byte, res.wpos);
}

static void api_logout(xpeer_ctx_ptr pctx)
{
    xhtnode_t *node = xhash_tree_del(&pctx->server->uuid_table, &pctx->node);
    uint64_t tid = xline_find_uint64(&pctx->maker, "tid");
    __xlogd("node key=%lu tid=%lu\n", node->key, tid);
    free(pctx->node.uuid);
    xline_t res = xline_maker(1024);
    xline_add_word(&res, "api", "res");
    xline_add_word(&res, "req", "logout");
    xline_add_uint64(&res, "tid", tid);
    xline_add_uint64(&res, "code", 200);
    xmsger_send_message(pctx->server->msger, pctx->channel, res.byte, res.wpos);    
}

static void api_break(xpeer_ctx_ptr pctx)
{
    xpeer_ptr server = pctx->server;
    xmsger_disconnect(server->msger, pctx->channel);
    __xlogd("task_loop >>>>---------------------------> free peer ctx node %lu\n", pctx->node.key);
    xhash_tree_del(&pctx->server->uuid_table, &pctx->node);
    __xlogd("task_loop >>>>---------------------------> uuid tree count %lu\n", pctx->server->uuid_table.count);
    free(pctx);    
}

typedef void(*api_task_enter)(xmsg_ctx_t *task);

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xmsg_ctx_t mctx;
    xpeer_ctx_ptr pctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &mctx, sizeof(xmsg_ctx_t)) == sizeof(xmsg_ctx_t))
    {
        pctx = mctx.pctx;
        if (pctx == NULL){
            __xlogd("task_loop >>>>---------------------------> peerctx == NULL\n");
            continue;
        }

        if (mctx.msg){
            xline_printf(mctx.msg);
            pctx->maker = xline_parser((xbyte_ptr)mctx.msg);
            const char *api = xline_find_word(&pctx->maker, "api");
            api_task_enter enter = xhash_table_find(&pctx->server->api_tabel, api);
            if (enter){
                enter(pctx);
            }
            free(mctx.msg);

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

    xhash_tree_init(&server->uuid_table);
    xhash_table_init(&server->api_tabel, 256);

    xhash_table_add(&server->api_tabel, "login", api_login);
    xhash_table_add(&server->api_tabel, "logout", api_logout);
    xhash_table_add(&server->api_tabel, "break", api_break);

    xmsgercb_ptr listener = &server->listener;

    listener->ctx = server;
    listener->on_channel_to_peer = on_connection_to_peer;
    listener->on_channel_from_peer = on_connection_from_peer;
    listener->on_channel_break = on_channel_break;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;

    server->sock = __xapi->udp_open();
    __xbreak(server->sock < 0);
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 9256, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
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

    xhash_table_clear(&server->api_tabel);
    xhash_tree_clear(&server->uuid_table);
    
    free(server);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
