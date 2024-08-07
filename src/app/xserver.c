#include "ex/ex.h"

#include <sys/struct/xtree.h>
#include <sys/struct/xmsger.h>
#include <sys/struct/xbuf.h>

#include <stdio.h>
#include <stdlib.h>


// KEY_BITS >= 2
#define KEY_BITS    4
#define KEY_SPACE   (1 << KEY_BITS)


typedef struct Node {
    uint32_t key;
    struct __xipaddr ip;
    xchannel_ptr channel;
    struct Node* predecessor;
    struct Node* finger_table[KEY_BITS];
}*Node_Ptr;

// Node_Ptr nodes[KEY_SPACE] = {0};


typedef struct XChord {

    uint32_t count;
    Node_Ptr ring;
    Node_Ptr prev, next;

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

Node_Ptr closest_preceding_finger(Node_Ptr chord, uint32_t key)
{
    Node_Ptr finger;
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

Node_Ptr find_predecessor(Node_Ptr chord, uint32_t key)
{
    Node_Ptr n = chord;
    Node_Ptr successor = chord->finger_table[1];
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

Node_Ptr find_successor(Node_Ptr chord, uint32_t key) {
    if (key == chord->key){
        return chord;
    }
    Node_Ptr n = find_predecessor(chord, key);
    return n->finger_table[1];
}

void print_node(Node_Ptr node)
{
    __xlogd("node->key >>>>--------------------------> enter %u\n", node->key);
    for (int i = 1; i < KEY_BITS; ++i){
        __xlogd("node->finger[%d] start_key=%u key=%u\n", 
            i, (node->key + (1 << (i-1))) % KEY_SPACE, node->finger_table[i]->key);
    }
    __xlogd("node->predecessor=%u\n", node->predecessor->key);
    __xlogd("node->key >>>>-----------------------> exit %u\n", node->key);
}

void node_print_all(Node_Ptr node)
{
    __xlogd("node_print_all >>>>------------------------------------------------> enter\n");
    Node_Ptr n = node->predecessor;
    while (n != node){
        Node_Ptr t = n->predecessor;
        print_node(n);
        n = t;
        if (n == node){
            print_node(n);
            break;
        }        
    }   
    __xlogd("node_print_all >>>>------------------------------------------------> exit\n"); 
}

void update_others(Node_Ptr node)
{
    __xlogd("update_others enter\n");
    node_print_all(node);
    uint32_t start;
    Node_Ptr prev, next, finger_node;
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

Node_Ptr node_create(uint32_t key)
{
    __xlogd("node_create enter\n");

    Node_Ptr node = (Node_Ptr)calloc(1, sizeof(struct Node));

    node->key = key;
    node->predecessor = node;

    for (int i = 0; i < KEY_BITS; i++) {
        node->finger_table[i] = node;
    }

    __xlogd("node_create exit\n");

    return node;
}


int node_join(Node_Ptr ring, Node_Ptr node)
{
    __xlogd("node_join >>>>--------------------> enter %u\n", node->key);

    uint32_t start;
    Node_Ptr successor = NULL;

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

void node_remove(Node_Ptr ring, uint32_t key)
{
    __xlogd("node_remove enter %u\n", key);
    uint32_t start;
    Node_Ptr node, prev, next, finger_node, successor;
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


typedef struct XTask* xTask_Ptr;
typedef void (*XTaskEntry)(xTask_Ptr task);

typedef struct XTask {
    void *msg;
    void *ctx;
    XTaskEntry entry;
    xchannel_ptr channel;
    struct xpeer *server;
    xmaker_t maker;
};

typedef struct xpeer{
    int sock;
    __atom_bool runnig;
    struct __xipaddr addr;
    char ip[16];
    uint16_t port;
    Node_Ptr ring;
    xmsger_ptr msger;
    xTask_Ptr tasks;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid, listen_pid;
    struct xmsgercb listener;
    // 通信录，好友ID列表，一个好友ID可以对应多个设备地址，但是只与主设备建立一个 channel，设备地址是动态更新的，不需要存盘。好友列表需要写数据库
    // 设备列表，用户自己的所有设备，每个设备建立一个 channel，channel 的消息实时共享，设备地址是动态更新的，设备 ID 需要存盘
    // 任务树，每个请求（发起的请求或接收的请求）都是一个任务，每个任务都有开始，执行和结束。请求是任务的开始，之后可能有多次消息传递，数据传输完成，双方各自结束任务，释放资源。
    // 单个任务本身的消息是串行的，但是多个任务之间是并行的，所以用树来维护任务列表，以便多个任务切换时，可以快速定位任务。
    // 每个任务都有一个唯一ID，通信双方维护各自的任务ID，发型消息时，要携带自身的任务ID并且指定对方的任务ID
    // 每个任务都有执行进度

}*xpeer_ptr;


static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xTask_Ptr task = xmsger_get_channel_ctx(channel);
    if (task){
        free(task);
    }
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
    xTask_Ptr task = xmsger_get_channel_ctx(channel);
    task->msg = msg;
    xpipe_write(task->server->task_pipe, &task, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void make_disconnect_task(xpeer_ptr server)
{
    xmsger_disconnect(server->msger, server->tasks->channel);
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xTask_Ptr task = xmsger_get_channel_ctx(channel);
    task->channel = channel;
    xpipe_write(task->server->task_pipe, &task, __sizeof_ptr);
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xTask_Ptr task = (xTask_Ptr)malloc(sizeof(struct XTask));
    task->channel = channel;
    task->server = listener->ctx;
    xmsger_set_channel_ctx(channel, task);
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


typedef struct task_type {
    int type;
    void *context;
}task_type;

#include <arpa/inet.h>
static int req_join_node(xTask_Ptr task)
{
    __xlogd("req_join_node enter\n");
    char *ip = xline_find_word(&task->maker, "ip");
    __xlogd("add ip=%s\n", ip);
    unsigned port = xline_find_number(&task->maker, "port");
    __xlogd("add port=%u\n", port);
    uint32_t key = xline_find_number(&task->maker, "key");
    __xlogd("add key=%u\n", key);
    xmaker_t ctx = xline_make(0);
    xline_add_word(&ctx, "msg", "join");
    xline_add_word(&ctx, "ip", ip);
    xline_add_number(&ctx, "port", port);
    xline_add_number(&ctx, "key", key);
    xTask_Ptr new_task = (xTask_Ptr)malloc(sizeof(struct XTask));
    new_task->server = task->server;
    new_task->msg = ctx.head;
    xmsger_connect(task->server->msger, ip, port, new_task);
    __xlogd("req_join_node exit\n");
    return 0;
}

static int msg_join_node(xTask_Ptr task)
{
    __xlogd("msg_join_node enter\n");
    uint32_t key = xline_find_number(&task->maker, "key");
    __xlogd("add key=%u\n", key);

    xmaker_t maker = xline_make(0);
    xline_add_word(&maker, "msg", "req");
    xline_add_word(&maker, "task", "invite");
    uint64_t pos = xline_hold_list(&maker, "nodes");
    Node_Ptr n = task->server->ring->predecessor;
    while (n != task->server->ring) {
        uint64_t tpos = xline_list_hold_tree(&maker);
        __xipaddr_ptr addr = xmsger_get_channel_ipaddr(n->channel);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr->ip, ip, sizeof(ip));
        uint16_t port = ntohs(addr->port);
        xline_add_word(&maker, "ip", ip);
        xline_add_number(&maker, "port", port);
        xline_add_number(&maker, "key", n->key);
        xline_list_save_tree(&maker, tpos);
        n = n->predecessor;
    }

    uint64_t tpos = xline_list_hold_tree(&maker);
    xline_add_word(&maker, "ip", task->server->ip);
    xline_add_number(&maker, "port",task->server->port);
    xline_add_number(&maker, "key", task->server->ring->key);
    xline_list_save_tree(&maker, tpos);

    xline_save_list(&maker, pos);
    xmsger_send_message(task->server->msger, task->channel, maker.head, maker.wpos);

    Node_Ptr node = node_create(key);
    node->channel = task->channel;
    int ret = node_join(task->server->ring, node);
    if (ret == -1){
        return -1;
    }
    node_print_all(task->server->ring);
    __xlogd("msg_join_node exit\n");

    return 0;
}

static int req_add_node(xTask_Ptr task)
{
    __xlogd("req_add_node enter\n");
    uint32_t key = xline_find_number(&task->maker, "key");
    Node_Ptr node = node_create(key);
    node->channel = task->channel;
    node_join(task->server->ring, node);
    node_print_all(task->server->ring);
    __xlogd("req_add_node exit\n");
}

static int msg_invite_node(xTask_Ptr task)
{
    __xlogd("msg_invite_node enter\n");
    Node_Ptr node = xline_find_pointer(&task->maker, "node");
    __xlogd("msg_invite_node 1\n");
    node->channel = task->channel;
    __xlogd("msg_invite_node 2\n");
    xmaker_t new_node = xline_make(0);
    __xlogd("msg_invite_node 3\n");
    xline_add_word(&new_node, "msg", "req");
    __xlogd("msg_invite_node 4\n");
    xline_add_word(&new_node, "task", "add");
    __xlogd("msg_invite_node 5\n");
    xline_add_number(&task->maker, "key", task->server->ring->key);
    __xlogd("msg_invite_node 6\n");
    xmsger_send_message(task->server->msger, node->channel, new_node.head, new_node.wpos);
    __xlogd("msg_invite_node 7\n");
    node_print_all(task->server->ring);
    __xlogd("msg_invite_node exit\n");
    return 0;
}

static int req_invite_node(xTask_Ptr task)
{
    __xlogd("req_invite_node enter\n");
    xline_ptr nodes = xline_find(&task->maker, "nodes");

    xline_ptr xptr;
    xparser_t plist = xline_parse(nodes);

    __xipaddr_ptr addr = xmsger_get_channel_ipaddr(task->channel);

    while ((xptr = xline_list_next(&plist)) != NULL)
    {
        xparser_t node_parser = xline_parse(xptr);
        char *ip = xline_find_word(&node_parser, "ip");
        uint16_t port = xline_find_number(&node_parser, "port");
        uint32_t key = xline_find_number(&node_parser, "key");

        Node_Ptr node = node_create(key);
        __xapi->udp_make_ipaddr(ip, port, &node->ip);
        node_join(task->server->ring, node);
        if (node->ip.ip == addr->ip){
            node->channel = task->channel;
        }else {
            xmaker_t ctx = xline_make(0);
            xline_add_word(&ctx, "msg", "invite");
            xline_add_pointer(&ctx, "node", node);
            xTask_Ptr new_task = (xTask_Ptr)malloc(sizeof(struct XTask));
            new_task->server = task->server;
            new_task->msg = ctx.head;
            xmsger_connect(task->server->msger, ip, port, new_task);
        }
    }
    node_print_all(task->server->ring);
    __xlogd("req_invite_node exit\n");
    
    return 0;
}

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xTask_Ptr task;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &task, __sizeof_ptr) == __sizeof_ptr)
    {
        xline_ptr msg = (xline_ptr)task->msg;
        task->maker = xline_parse(msg);
        const char *cmd = xline_find_word(&task->maker, "msg");
        if (mcompare(cmd, "req", 3) == 0){
            cmd = xline_find_word(&task->maker, "task");
            if (mcompare(cmd, "join", slength("join")) == 0){
                req_join_node(task);
            }else if (mcompare(cmd, "invite", slength("invite")) == 0){
                req_invite_node(task);
            }else if (mcompare(cmd, "add", slength("add")) == 0){
                req_add_node(task);
            }
        }else if(mcompare(cmd, "res", 3) == 0){
            xline_printf(msg);
        }else if(mcompare(cmd, "join", slength("join")) == 0){
            msg_join_node(task);
        }else if (mcompare(cmd, "invite", slength("invite")) == 0){
            msg_invite_node(task);
        }
        free(msg);
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
    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_ptr server = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
    mcopy(server->ip, argv[1], slength(argv[1]));
    server->port = atoi(argv[2]);
    server->ring = node_create(atoi(argv[3]));

    __xlogi("ip=%s port=%u key=%u\n", server->ip, server->port, server->ring->key);

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

    server->tasks = (xTask_Ptr)calloc(1, sizeof(struct XTask));

    // const char *host = "127.0.0.1";
    // const char *host = "47.99.146.226";
    // const char *host = "18.138.128.58";
    // uint16_t port = atoi(argv[1]);
    // uint16_t port = 9256;

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

    server->listen_pid = __xapi->process_create(listen_loop, server);
    __xbreak(server->listen_pid == NULL);

    // if (host){
    //     make_connect_task(server, host, port);
    // }

    char str[1024];
    while (g_server->runnig)
    {
        __xlogi("Enter a value :\n");
        fgets(str, 1000, stdin);
        // size_t len = slength(str);
        // if (len == 2){
        //     if (str[0] == 'c'){
        //         make_connect_task(server, host, port);
        //     }else if (str[0] == 'd'){
        //         make_disconnect_task(server);
        //     }else if (str[0] == 's'){
        //         make_message_task(server);   
        //     }else if (str[0] == 'q'){
        //         break;
        //     }
        // }else {
        //     make_message_task(server);
        // }
    }

    if (server->task_pipe){
        xpipe_break(server->task_pipe);
    }

    if (server->task_pid){
        __xapi->process_free(server->task_pid);
    }

    if (server->listen_pid){
        __xapi->process_free(server->listen_pid);
    }

    if (server->task_pipe){
        while (xpipe_readable(server->task_pipe) > 0){
            // TODO 清理管道
        }
        xpipe_free(&server->task_pipe);
    }
    
    xmsger_free(&server->msger);

    free(server->tasks);

    if (server->sock > 0){
        __xapi->udp_close(server->sock);
    }
    
    free(server);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
