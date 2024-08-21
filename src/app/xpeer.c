#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>
#include <xnet/uuid.h>
#include <xlib/xxhash.h>

#include <stdio.h>
#include <stdlib.h>


typedef struct xpeer_task {
    index_node_t node;
    xlkv_t taskctx;
    struct xpeer_ctx *pctx;
    struct xpeer_task *prev, *next;
    void (*enter)(struct xpeer_task*, struct xpeer_ctx*);
}*xpeer_task_ptr;

typedef struct xpeer_ctx {
    uint16_t reconnected;
    uuid_node_t node;
    xchannel_ptr channel;
    struct xpeer *server;
    xlkv_t xlparser;
    struct xpeer_task tasklist;
    void (*release)(struct xpeer_ctx*);
}*xpeer_ctx_ptr;


struct chord_node {
    char ip[16];
    uint16_t port;
    uint64_t key;
    struct chord_node *prev;
    struct chord_node *next;
};


typedef struct xpeer{
    int sock;
    uint64_t key;
    uint8_t uuid[32];
    __atom_bool runnig;
    uint64_t task_id;
    struct __xipaddr addr;
    xl_ptr chord_list;
    int cid;
    xchannel_ptr channel;
    xpeer_ctx_ptr pctx_list[4];
    xmsger_ptr msger;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid, listen_pid;
    struct xmsgercb listener;
    uuid_list_t uuid_table;
    uint64_t task_count;
    index_table_t task_table;
    search_table_t api_tabel;
    // 通信录，好友ID列表，一个好友ID可以对应多个设备地址，但是只与主设备建立一个 channel，设备地址是动态更新的，不需要存盘。好友列表需要写数据库
    // 设备列表，用户自己的所有设备，每个设备建立一个 channel，channel 的消息实时共享，设备地址是动态更新的，设备 ID 需要存盘
    // 任务树，每个请求（发起的请求或接收的请求）都是一个任务，每个任务都有开始，执行和结束。请求是任务的开始，之后可能有多次消息传递，数据传输完成，双方各自结束任务，释放资源。
    // 单个任务本身的消息是串行的，但是多个任务之间是并行的，所以用树来维护任务列表，以便多个任务切换时，可以快速定位任务。
    // 每个任务都有一个唯一ID，通信双方维护各自的任务ID，发型消息时，要携带自身的任务ID并且指定对方的任务ID
    // 每个任务都有执行进度
}*xpeer_ptr;


typedef struct xmsg_ctx {
    void *msg;
    xpeer_ctx_ptr pctx;
}xmsg_ctx_t;

static void on_channel_timeout(xmsgercb_ptr listener, xchannel_ptr channel, xmessage_ptr delay_msg_list)
{
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 1\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 2\n");
    __xipaddr_ptr addr = xmsger_get_channel_ipaddr(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 3\n");
    char ip[16] = {0};
    uint16_t port = 0;
    __xapi->udp_ipaddr_to_host(addr, ip, 16, &port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 4\n");
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "timeout");
    xl_add_word(&xl, "ip", ip);
    xl_add_number(&xl, "port", port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 5\n");
    xmsg_ctx_t mctx;
    mctx.pctx = pctx;
    mctx.msg = xl.head;
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 6\n");
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    if (pctx->xlparser.head){
        free(pctx->xlparser.head);
        pctx->xlparser.head = NULL;
    }
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "break");
    xmsg_ctx_t mctx;
    mctx.pctx = pctx;
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
    mctx.pctx = xmsger_get_channel_ctx(channel);
    mctx.msg = msg;
    xpipe_write(server->task_pipe, &mctx, sizeof(xmsg_ctx_t));
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static inline xpeer_ctx_ptr xpeer_ctx_create(xpeer_ptr server, xchannel_ptr channel)
{
    xpeer_ctx_ptr pctx = (xpeer_ctx_ptr)calloc(1, sizeof(struct xpeer_ctx));
    pctx->server = server;
    pctx->channel = channel;
    pctx->tasklist.next = &pctx->tasklist;
    pctx->tasklist.prev = &pctx->tasklist;
    return pctx;
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    server->channel = channel;
    xpeer_ctx_ptr pctx = xmsger_get_channel_ctx(channel);
    pctx->reconnected = 0;
    pctx->channel = channel;
    if (pctx->xlparser.head){
        xmsg_ctx_t mctx;
        mctx.msg = pctx->xlparser.head;
        pctx->xlparser.head = NULL;
        mctx.pctx = xpeer_ctx_create(server, channel);        
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

typedef void(*api_task_enter)(xpeer_ctx_ptr *pctx);

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xmsg_ctx_t mctx;
    xpeer_ctx_ptr pctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

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
    task->taskctx = xl_maker(1024);
    index_table_add(&pctx->server->task_table, &task->node);
    __xlogd("add_task exit\n");
    return task;
}

inline static xpeer_task_ptr remove_task(xpeer_task_ptr task)
{
    xpeer_ptr server = task->pctx->server;
    __xlogd("remove_task tid=%lu\n", task->node.index);
    index_table_del(&server->task_table, task);
    task->prev->next = task->next;
    task->next->prev = task->prev;
    free(task->taskctx.head);
    free(task);
}

static void res_forward(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.head);
    remove_task(task);
}

static void req_forward(xpeer_ctx_ptr pctx, uint64_t key)
{
    __xlogd("req_messaging ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_forward);
    struct xlkv maker = xl_maker(1024);
    xl_add_word(&maker, "api", "forward");
    xl_add_number(&maker, "tid", task->node.index);
    xl_add_number(&maker, "key", key);
    xl_add_word(&maker, "text", "Hello World");
    xmsger_send_message(peer->msger, peer->channel, maker.head, maker.wpos);
    __xlogd("req_messaging ----------------------- exit\n");
}

static void res_chord_list(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xpeer_ptr server = pctx->server;
    xl_printf(pctx->xlparser.head);

    xl_ptr xlnoes = xl_find(&pctx->xlparser, "nodes");
    if (server->chord_list != NULL){
        free(server->chord_list);
    }
    uint64_t size = __sizeof_line(xlnoes);
    server->chord_list = malloc(size);
    mcopy(server->chord_list, xlnoes, size);

    remove_task(task);
}

static void chord_list(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_list ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_chord_list);
    xlkv_t maker = xl_maker(1024);
    xl_add_word(&maker, "api", "command");
    xl_add_word(&maker, "cmd", "chord_list");
    xl_add_number(&maker, "tid", task->node.index);
    xmsger_send_message(peer->msger, peer->channel, maker.head, maker.wpos);
    __xlogd("chord_list ----------------------- exit\n");
}

static void res_chord_invite(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.head);
    remove_task(task);
}

static void chord_invite(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_invite ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_chord_invite);
    xlkv_t req = xl_maker(1024);
    xl_add_word(&req, "api", "command");
    xl_add_word(&req, "cmd", "chord_invite");
    xl_add_number(&req, "tid", task->node.index);
    xlkv_t nodes = xl_parser(peer->chord_list);
    xl_add_list(&req, "nodes", &nodes);
    xmsger_send_message(peer->msger, peer->channel, req.head, req.wpos);
    __xlogd("chord_invite ----------------------- exit\n");
}

static void res_login(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.head);
    remove_task(task);
}

static void hello(xpeer_ctx_ptr pctx)
{
    __xlogd("hello ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_login);
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "hello");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_word(&xl, "password", "123456");
    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
    __xlogd("hello ----------------------- exit\n");
}

static void bye(xpeer_ctx_ptr pctx)
{
    __xlogd("bye ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;

    xpeer_task_ptr task = add_task(pctx, res_login);
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "bye");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_word(&xl, "password", "123456");
    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
    __xlogd("bye ----------------------- exit\n");
}

static void req_login(xpeer_ctx_ptr pctx)
{
    __xlogd("req_login ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;

    xpeer_task_ptr task = add_task(pctx, res_login);
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "login");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_number(&xl, "key", peer->key);
    xl_add_bin(&xl, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
    __xlogd("req_login ----------------------- exit\n");
}

static void res_logout(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.head);
    remove_task(task);
}

static void req_logout(xpeer_ctx_ptr pctx)
{
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_logout);
    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "logout");
    xl_add_number(&xl, "tid", task->node.index);
    xl_add_number(&xl, "key", peer->key);
    xl_add_bin(&xl, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
}

static void api_timeout(xpeer_ctx_ptr pctx)
{
    __xlogd("api_timeout enter\n");
    if (pctx->reconnected < 3){
        pctx->reconnected++;
        char *ip = xl_find_word(&pctx->xlparser, "ip");
        uint16_t port = xl_find_number(&pctx->xlparser, "port");
        struct __xipaddr addr;
        __xapi->udp_host_to_ipaddr(ip, port, &addr);
        xmsger_connect(pctx->server->msger, &addr, pctx);
    }
    __xlogd("api_timeout exit\n");
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
    char *host = NULL;
    uint16_t port = 0;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_ptr server = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
    __xlogi("server: 0x%X\n", server);

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

    uuid_generate(server->uuid, "PEER1");
    server->key = xxhash64(server->uuid, 32, 0);

    uuid_list_init(&server->uuid_table);
    index_table_init(&server->task_table, 256);
    search_table_init(&server->api_tabel, 256);

    search_table_add(&server->api_tabel, "login", req_login);
    search_table_add(&server->api_tabel, "logout", req_logout);
    search_table_add(&server->api_tabel, "res", api_response);
    search_table_add(&server->api_tabel, "timeout", api_timeout);

    if (argc == 3){
        host = strdup(argv[1]);
        port = atoi(argv[2]);
    }

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
    listener->on_channel_timeout = on_channel_timeout;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;

    server->sock = __xapi->udp_open();
    __xbreak(server->sock < 0);
    __xbreak(!__xapi->udp_host_to_ipaddr(NULL, 0, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    struct sockaddr_in server_addr; 
    socklen_t addr_len = sizeof(server_addr);
    __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    __xbreak(!__xapi->udp_host_to_ipaddr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));

    server->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener, server->sock);

    server->listen_pid = __xapi->process_create(listen_loop, server);
    __xbreak(server->listen_pid == NULL);

    struct __xipaddr addr;
    const char *hosts[] = {"47.99.146.226", "47.92.77.19", "120.78.155.213", "18.138.128.58"};
    for (int i = 0; i < 4; ++i){
        __xapi->udp_host_to_ipaddr(hosts[i], 9256, &addr);
        server->pctx_list[i] = xpeer_ctx_create(server, NULL);
        xmsger_connect(server->msger, &addr, server->pctx_list[i]);
    }

    server->cid = 0;
    server->channel = server->pctx_list[server->cid]->channel;

    char str[1024];
    char input[256];
    char command[256];
    char ip[256] = {0};
    uint64_t cid, key;
    while (g_server->runnig)
    {
        printf("> "); // 命令提示符
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // 读取失败，退出循环
        }

        // 去掉输入字符串末尾的换行符
        input[strcspn(input, "\n")] = 0;

        // 分割命令和参数
        sscanf(input, "%s", command);

        if (strcmp(command, "login") == 0) {

            for (int i = 0; i < 4; ++i){
                hello(server->pctx_list[i]);
            }

        } else if (strcmp(command, "logout") == 0) {

            for (int i = 0; i < 4; ++i){
                bye(server->pctx_list[i]);
            }

        } else if (strcmp(command, "list") == 0) {
            
            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            chord_list(server->pctx_list[cid]);

        }  else if (strcmp(command, "invite") == 0) {

            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            chord_invite(server->pctx_list[cid]);

        }else if (strcmp(command, "msg") == 0) {

            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            __xlogi("输入键值: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("读取键值失败\n");
                continue;
            }

            req_forward(server->pctx_list[cid], key);

        } else if (strcmp(command, "con") == 0) {

            struct __xipaddr addr;
            __xapi->udp_host_to_ipaddr(hosts[server->cid], 9256, &addr);
            server->pctx_list[server->cid] = xpeer_ctx_create(server, NULL);
            xmsger_connect(server->msger, &addr, server->pctx_list[server->cid]);

        } else if (strcmp(command, "discon") == 0) {

            xmsger_disconnect(server->msger, server->channel);
            
        } else if (strcmp(command, "exit") == 0) {
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        mclear(command, 256);
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

    search_table_clear(&server->api_tabel);
    index_table_clear(&server->task_table);
    uuid_list_clear(&server->uuid_table);
    
    xmsger_free(&server->msger);

    if (server->sock > 0){
        __xapi->udp_close(server->sock);
    }
    
    free(server);

    if (host){
        free(host);
    }

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
