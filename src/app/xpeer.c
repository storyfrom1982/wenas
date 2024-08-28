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
    xlinekv_t taskctx;
    struct xchannel_ctx *pctx;
    struct xpeer_task *prev, *next;
    void (*enter)(struct xpeer_task*, struct xpeer_ctx*);
}*xpeer_task_ptr;

typedef struct xchannel_ctx {
    uint16_t reconnected;
    uuid_node_t node;
    xchannel_ptr channel;
    struct xpeer *server;
    xlinekv_t xlparser;
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
    xline_ptr chord_list;
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

    char password[16];
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

static void on_channel_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 3\n");
    char *ip = xchannel_get_ip(channel);
    uint16_t port = xchannel_get_port(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> port=%u\n", port);
    xmsg_ptr msg = xmsg_maker();
    msg->channel = channel;
    msg->ctx = xchannel_get_ctx(channel);
    xl_add_word(&msg->lkv, "api", "timeout");
    xl_add_word(&msg->lkv, "ip", ip);
    xl_add_uint(&msg->lkv, "port", port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 5\n");
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xmsg_ptr msg = xl_maker();
    xl_add_word(&msg->lkv, "api", "break");
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_to_peer(xmsgercb_ptr listener, xchannel_ptr channel, void* msg, size_t len)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xmsg_free(msg);
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, xmsg_ptr msg)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static inline xpeer_ctx_ptr xpeer_ctx_create(xpeer_ptr server, xchannel_ptr channel)
{
    xpeer_ctx_ptr pctx = (xpeer_ctx_ptr)calloc(1, sizeof(struct xchannel_ctx));
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
    xpeer_ctx_ptr ctx = xchannel_get_ctx(channel);
    if (ctx){
        __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> 1\n");
        ctx->reconnected = 0;
        ctx->channel = channel;
    }else {
        __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> 2\n");
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
    xpeer_ctx_ptr pctx = xpeer_ctx_create(listener->ctx, channel);
    xchannel_set_ctx(channel, pctx);
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

    xmsg_ptr msg;
    xpeer_ctx_ptr ctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &msg, __sizeof_ptr) == __sizeof_ptr)
    {
        ctx = msg->ctx;
        // ctx->xlparser = xmsg_parser(msg);
        xl_printf(&msg->lkv.line);
        __xlogd("task_loop 1\n");
        ctx->xlparser = xl_parser(&msg->lkv.line);
        __xlogd("task_loop 2\n");
        // xmsg_printf(msg);
        const char *api = xl_find_word(&ctx->xlparser, "api");
        __xlogd("task_loop api=%s\n", api);
        api_task_enter enter = search_table_find(&ctx->server->api_tabel, api);
        if (enter){
            enter(ctx);
        }
        xmsg_free(msg);
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
    free(task);
}

static void res_forward(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.body);
    remove_task(task);
}

static void req_forward(xpeer_ctx_ptr pctx, uint64_t key)
{
    __xlogd("req_messaging ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_forward);
    xmsg_ptr msg = xmsg_maker();
    xl_add_word(&msg->lkv, "api", "forward");
    xl_add_uint(&msg->lkv, "tid", task->node.index);
    xl_add_uint(&msg->lkv, "key", key);
    xl_add_word(&msg->lkv, "text", "Hello World");
    xmsger_send_message(peer->msger, peer->channel, msg);
    __xlogd("req_messaging ----------------------- exit\n");
}

static void res_chord_list(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xpeer_ptr server = pctx->server;
    xl_printf(pctx->xlparser.body);

    xline_ptr xlnoes = xl_find(&pctx->xlparser, "nodes");
    if (server->chord_list != NULL){
        free(server->chord_list);
    }
    uint64_t size = __xl_sizeof_line(xlnoes);
    server->chord_list = malloc(size);
    mcopy(server->chord_list, xlnoes, size);

    remove_task(task);
}

static void chord_list(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_list ----------------------- enter\n");
    xpeer_task_ptr task = add_task(pctx, res_chord_list);
    xmsg_ptr req = xmsg_maker();
    xl_add_word(&req->lkv, "api", "command");
    xl_add_word(&req->lkv, "cmd", "chord_list");
    xl_add_uint(&req->lkv, "tid", task->node.index);
    xl_add_word(&req->lkv, "password", pctx->server->password);
    xmsger_send_message(pctx->server->msger, pctx->channel, req);
    __xlogd("chord_list ----------------------- exit\n");
}

static void res_chord_invite(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.body);
    remove_task(task);
}

static void chord_invite(xpeer_ctx_ptr pctx)
{
    __xlogd("chord_invite ----------------------- enter\n");
    xpeer_task_ptr task = add_task(pctx, res_chord_invite);
    xmsg_ptr req = xmsg_maker();
    xl_add_word(&req->lkv, "api", "command");
    xl_add_word(&req->lkv, "cmd", "chord_invite");
    xl_add_uint(&req->lkv, "tid", task->node.index);
    xl_add_word(&req->lkv, "password", pctx->server->password);
    xl_add_obj(&req->lkv, "nodes", pctx->server->chord_list);
    xmsger_send_message(pctx->server->msger, pctx->channel, req);
    __xlogd("chord_invite ----------------------- exit\n");
}

static void res_login(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.body);
    remove_task(task);
}

static void req_login(xpeer_ctx_ptr ctx)
{
    __xlogd("req_login ----------------------- enter\n");
    xpeer_ptr peer = ctx->server;

    xpeer_task_ptr task = add_task(ctx, res_login);
    xmsg_ptr req = xmsg_maker();
    xl_add_word(&req->lkv, "api", "login");
    xl_add_uint(&req->lkv, "tid", task->node.index);
    xl_add_uint(&req->lkv, "key", peer->key);
    xl_add_bin(&req->lkv, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    __xlogd("req_logout req->lkv.wpos=%lu\n", req->lkv.wpos);
    xl_printf(&req->lkv.line);
    xmsger_send_message(peer->msger, ctx->channel, req);
    __xlogd("req_login ----------------------- exit\n");
}

static void res_logout(xpeer_task_ptr task, xpeer_ctx_ptr pctx)
{
    xl_printf(pctx->xlparser.body);
    remove_task(task);
}

static void req_logout(xpeer_ctx_ptr pctx)
{
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_logout);
    xmsg_ptr req = xmsg_maker();
    xl_add_word(&req->lkv, "api", "logout");
    xl_add_uint(&req->lkv, "tid", task->node.index);
    xl_add_uint(&req->lkv, "key", peer->key);
    xl_add_bin(&req->lkv, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    xmsger_send_message(peer->msger, pctx->channel, req);
}

static void api_timeout(xpeer_ctx_ptr pctx)
{
    __xlogd("api_timeout enter\n");
    if (pctx->reconnected < 3){
        pctx->reconnected++;
        char *ip = xl_find_word(&pctx->xlparser, "ip");
        uint16_t port = xl_find_uint(&pctx->xlparser, "port");
        xmsger_connect(pctx->server->msger, ip, port, pctx, NULL);
    }
    __xlogd("api_timeout exit\n");
}

static void api_response(xpeer_ctx_ptr pctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_uint(&pctx->xlparser, "tid");
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
    const char *hostname = "pindanci.com";
    char hostip[16] = {0};
    uint16_t port = 9256;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_ptr server = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
    __xlogi("server: 0x%X\n", server);

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

    mcopy(server->password, "123456", slength("123456"));
    uuid_generate(server->uuid, "PEER1");
    server->key = xxhash64(server->uuid, 32, 0);

    uuid_list_init(&server->uuid_table);
    index_table_init(&server->task_table, 256);
    search_table_init(&server->api_tabel, 256);

    search_table_add(&server->api_tabel, "login", req_login);
    search_table_add(&server->api_tabel, "logout", req_logout);
    search_table_add(&server->api_tabel, "res", api_response);
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
    __xbreak(!__xapi->udp_host_to_addr(NULL, 0, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    struct sockaddr_in server_addr; 
    socklen_t addr_len = sizeof(server_addr);
    __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    __xbreak(!__xapi->udp_host_to_addr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));

    server->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener, server->sock);

    server->listen_pid = __xapi->process_create(listen_loop, server);
    __xbreak(server->listen_pid == NULL);

    // __xapi->udp_addrinfo(hostip, 16, hostname);
    __xapi->udp_hostbyname(hostip, 16, hostname);
    __xlogd("host ip = %s port=%u\n", hostip, port);
    mcopy(hostip, "192.168.43.173", slength("192.168.43.173"));
    // mcopy(hostip, "120.78.155.213", slength("120.78.155.213"));
    // mcopy(hostip, "47.99.146.226", slength("47.99.146.226"));
    
    server->pctx_list[0] = xpeer_ctx_create(server, NULL);
    xmsger_connect(server->msger, hostip, port, server->pctx_list[0], NULL);

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
            req_login(server->pctx_list[0]);

        } else if (strcmp(command, "logout") == 0) {
            req_logout(server->pctx_list[0]);

        } else if (strcmp(command, "list") == 0) {
            
            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            chord_list(server->pctx_list[0]);

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

            server->pctx_list[server->cid] = xpeer_ctx_create(server, NULL);
            xmsger_connect(server->msger, "192.168.43.173", 9256, server->pctx_list[server->cid], NULL);

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

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
