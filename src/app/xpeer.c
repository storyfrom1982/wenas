#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>
#include <xnet/uuid.h>
#include <xlib/xxhash.h>

#include <stdio.h>
#include <stdlib.h>


typedef struct msg_timer {

}msg_timer_t;

typedef struct message_task {

}message_task_t;

typedef struct media_stream_task {

}media_stream_task_t;

typedef struct file_download_task {

}file_download_task_t;

typedef struct file_upload_task {

}file_upload_task_t;

typedef struct xpeer_task {
    index_node_t node;
    xlmsg_t taskctx;
    int type;
    void *task_ctx;
    struct xchannel_ctx *pctx;
    struct xpeer_task *prev, *next;
    void (*handle)(struct xpeer_task*, struct xchannel_ctx*);
}*xpeer_task_ptr;

typedef struct xpeer_ctx {
    uuid_node_t node;
    uint16_t port;
    char ip[__XAPI_IP_STR_LEN];    
    struct xpeer_task tasklist;
    struct xchannel_ctx *ctx;
}xpeer_ctx_t;

typedef struct xchannel_ctx {
    // uuid_node_t node;
    // xlmsg_t xlparser;
    // struct xpeer_task tasklist;
    uint8_t reconnected;
    xchannel_ptr channel;
    struct xpeer *server;
    void *processor;
    void (*process)(struct xchannel_ctx*, xlmsg_ptr msg);
}*xchannel_ctx_ptr;


struct chord_node {
    char ip[16];
    uint16_t port;
    uint64_t key;
    struct chord_node *prev;
    struct chord_node *next;
};


typedef struct xpeer{
    int sock;
    uint16_t port;
    char ip[__XAPI_IP_STR_LEN];
    uint64_t key;
    uint8_t uuid[32];
    __atom_bool runnig;
    xlmsg_t parser;
    uint64_t task_id;
    struct __xipaddr addr;
    xline_ptr chord_list;
    xmsger_ptr msger;

    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid;
    struct xmsgercb listener;
    uuid_list_t uuid_table;
    uint64_t task_count;
    index_table_t task_table;
    xtree api;

    char *bootstrap_host;
    // struct xchannel_ctx *ctx;
    xpeer_ctx_t *server;

    char password[16];
    // 通信录，好友ID列表，一个好友ID可以对应多个设备地址，但是只与主设备建立一个 channel，设备地址是动态更新的，不需要存盘。好友列表需要写数据库
    // 设备列表，用户自己的所有设备，每个设备建立一个 channel，channel 的消息实时共享，设备地址是动态更新的，设备 ID 需要存盘
    // 任务树，每个请求（发起的请求或接收的请求）都是一个任务，每个任务都有开始，执行和结束。请求是任务的开始，之后可能有多次消息传递，数据传输完成，双方各自结束任务，释放资源。
    // 单个任务本身的消息是串行的，但是多个任务之间是并行的，所以用树来维护任务列表，以便多个任务切换时，可以快速定位任务。
    // 每个任务都有一个唯一ID，通信双方维护各自的任务ID，发型消息时，要携带自身的任务ID并且指定对方的任务ID
    // 每个任务都有执行进度
}*xpeer_ptr;

static inline xchannel_ctx_ptr xchannel_ctx_create(xpeer_ptr server, xchannel_ptr channel)
{
    xchannel_ctx_ptr ctx = (xchannel_ctx_ptr)malloc(sizeof(struct xchannel_ctx));
    ctx->server = server;
    ctx->channel = channel;
    return ctx;
}

static inline xpeer_ctx_t* xpeer_ctx_create(xchannel_ctx_ptr ctx)
{
    xpeer_ctx_t *user = (xpeer_ctx_t*)calloc(1, sizeof(xpeer_ctx_t));
    user->ctx = ctx;
    user->tasklist.next = &user->tasklist;
    user->tasklist.prev = &user->tasklist;
    return user;
}

static void on_msg_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    // __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 3\n");
    const char *ip = xchannel_get_ip(channel);
    uint16_t port = xchannel_get_port(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> port=%u\n", port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_disconnect(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xlmsg_ptr xkv = xl_maker();
    xkv->ctx = xchannel_get_ctx(channel);
    xl_add_word(xkv, "api", "disconnect");
    xpipe_write(server->task_pipe, &xkv, __sizeof_ptr);
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_to_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr xkv)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xl_free(xkv);
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr msg)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    msg->ctx = xchannel_get_ctx(channel);
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xchannel_ctx_ptr ctx = xchannel_get_ctx(channel);
    if (ctx){
        // __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> 1\n");
        ctx->reconnected = 0;
        ctx->channel = channel;
    }else {
        __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> 2\n");
        ctx = xchannel_ctx_create(listener->ctx, channel);
        ctx->server = server;
        ctx->channel = channel;
        xchannel_set_ctx(channel, ctx);
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xchannel_ctx_ptr pctx = xchannel_ctx_create(listener->ctx, channel);
    xchannel_set_ctx(channel, pctx);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

typedef void(*api_handle)(xchannel_ctx_ptr);

static void api_processor(xchannel_ctx_ptr ctx, xlmsg_ptr msg)
{
    xl_printf(&msg->line);
    ctx->server->parser = xl_parser(&msg->line);
    const char *api = xl_find_word(&ctx->server->parser, "api");
    api_handle func = xtree_find(ctx->server->api, api, slength(api));
    if (func){
        func(ctx);
    }
}

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xlmsg_ptr msg;
    xchannel_ctx_ptr ctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &msg, __sizeof_ptr) == __sizeof_ptr)
    {
        ctx = msg->ctx;
        if (ctx->process != NULL){
            ctx->process(ctx, msg);
        }else {
            xl_printf(&msg->line);
            server->parser = xl_parser(&msg->line);
            const char *api = xl_find_word(&server->parser, "api");
            api_handle func = xtree_find(ctx->server->api, api, slength(api));
            if (func){
                func(ctx);
            }
        }
        xl_free(msg);
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

inline static xpeer_task_ptr add_task(xchannel_ctx_ptr ctx, void(*handle)(xpeer_task_ptr, xchannel_ctx_ptr))
{
    __xlogd("add_task enter\n");
    xpeer_ctx_t *user = (xpeer_ctx_t *)ctx->processor;
    xpeer_task_ptr task = (xpeer_task_ptr)malloc(sizeof(struct xpeer_task));
    task->pctx = ctx;
    task->node.index = ctx->server->task_count++;
    task->handle = handle;
    task->next = user->tasklist.next;
    task->prev = &user->tasklist;
    task->prev->next = task;
    task->next->prev = task;
    index_table_add(&ctx->server->task_table, &task->node);
    __xlogd("add_task exit\n");
    return task;
}

inline static xpeer_task_ptr remove_task(xpeer_task_ptr task)
{
    xpeer_ptr server = task->pctx->server;
    __xlogd("remove_task tid=%lu\n", task->node.index);
    index_table_del(&server->task_table, (index_node_t*)task);
    task->prev->next = task->next;
    task->next->prev = task->prev;
    free(task);
}

static void res_forward(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{
    // xl_printf(pctx->xlparser.body);
    remove_task(task);
}

static void req_forward(xchannel_ctx_ptr pctx, uint64_t key)
{
    __xlogd("req_messaging ----------------------- enter\n");
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_forward);
    xlmsg_ptr msg = xl_maker();
    xl_add_word(msg, "api", "forward");
    xl_add_uint(msg, "tid", task->node.index);
    xl_add_uint(msg, "key", key);
    xl_add_word(msg, "text", "Hello World");
    // xmsger_send_message(peer->msger, peer->channel, msg);
    __xlogd("req_messaging ----------------------- exit\n");
}

static void res_chord_list(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{
    xpeer_ptr server = pctx->server;
    // xl_printf(pctx->xlparser.body);

    xline_ptr xlnoes = xl_find(&server->parser, "nodes");
    if (server->chord_list != NULL){
        free(server->chord_list);
    }
    uint64_t size = __xl_sizeof_line(xlnoes);
    server->chord_list = malloc(size);
    mcopy(server->chord_list, xlnoes, size);

    remove_task(task);
}

static void chord_list(xchannel_ctx_ptr pctx)
{
    __xlogd("chord_list ----------------------- enter\n");
    xpeer_task_ptr task = add_task(pctx, res_chord_list);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "command");
    xl_add_word(req, "cmd", "chord_list");
    xl_add_uint(req, "tid", task->node.index);
    xl_add_word(req, "password", pctx->server->password);
    xmsger_send_message(pctx->server->msger, pctx->channel, req);
    __xlogd("chord_list ----------------------- exit\n");
}

static void res_chord_invite(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{
    remove_task(task);
}

static void chord_invite(xchannel_ctx_ptr pctx)
{
    __xlogd("chord_invite ----------------------- enter\n");
    xpeer_task_ptr task = add_task(pctx, res_chord_invite);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "command");
    xl_add_word(req, "cmd", "chord_invite");
    xl_add_uint(req, "tid", task->node.index);
    xl_add_word(req, "password", pctx->server->password);
    xl_add_obj(req, "nodes", pctx->server->chord_list);
    xmsger_send_message(pctx->server->msger, pctx->channel, req);
    __xlogd("chord_invite ----------------------- exit\n");
}

#include <string.h>
static void req_login(xpeer_ptr peer, const char *ip, uint16_t port);

static void res_logout(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{
    // char hostip[16] = {0};
    // uint16_t port = xchannel_get_port(pctx->channel);
    // const char *ip = xchannel_get_ip(pctx->channel);
    // mcopy(hostip, ip, slength(ip));
    xmsger_disconnect(pctx->server->msger, pctx->channel);
    remove_task(task);
}

static void req_logout(xchannel_ctx_ptr pctx)
{
    xpeer_ptr peer = pctx->server;
    xpeer_task_ptr task = add_task(pctx, res_logout);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "logout");
    xl_add_uint(req, "tid", task->node.index);
    xl_add_uint(req, "key", peer->key);
    xl_add_bin(req, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    uint8_t buf[2048];
    memset(buf, 'l', 2047);
    buf[2047] = '\0';
    xl_add_str(req, "str", buf, 2048);
    xmsger_send_message(peer->msger, pctx->channel, req);
}

static void req_echo(xchannel_ctx_ptr pctx, const char *text);
static void res_echo(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{
    req_echo(pctx, "hello world");
    remove_task(task);
}

static void req_echo(xchannel_ctx_ptr pctx, const char *text)
{
    xpeer_task_ptr task = add_task(pctx, res_echo);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "echo");
    xl_add_uint(req, "tid", task->node.index);
    xl_add_word(req, "text", text);
    xmsger_send_message(pctx->server->msger, pctx->channel, req);
}

static void res_login(xpeer_task_ptr task, xchannel_ctx_ptr pctx)
{

    // req_logout(pctx);
    req_echo(pctx, "Hello......");
    remove_task(task);
}

// static void req_login(xpeer_ptr peer, const char *ip, uint16_t port)
// {
//     __xlogd("req_login ----------------------- enter\n");
//     // peer->ctx = xchannel_ctx_create(peer, NULL);
//     // peer->ctx->processor = xpeer_ctx_create(peer->ctx);
//     // peer->ctx->process = api_processor;
//     // xpeer_task_ptr task = add_task(peer->ctx, res_login);
//     // xlmsg_ptr req = xline_maker();
//     // xline_add_word(req, "api", "login");
//     // xline_add_uint(req, "tid", task->node.index);
//     // xline_add_uint(req, "key", peer->key);
//     // xline_add_bin(req, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
//     // // uint8_t buf[2048];
//     // // memset(buf, 'l', 2047);
//     // // buf[2047] = '\0';
//     // // xline_add_str(req, "str", buf, 2048);
//     // // xmsger_send_message(peer->msger, ctx->channel, req);
//     // xmsger_connect(peer->msger, ip, port, peer->ctx, req);
//     __xlogd("req_login ----------------------- exit\n");
// }

static void api_disconnect(xchannel_ctx_ptr pctx)
{
    __xlogd("api_disconnect ----------------------- enter\n");
    // if (pctx == pctx->server->ctx){
    //     xpeer_ptr server = pctx->server;
    //     req_login(server, server->ip, server->port);
    // }
    free(pctx->processor);
    free(pctx);
    __xlogd("api_disconnect ----------------------- exit\n");
}

static void api_timeout(xchannel_ctx_ptr pctx)
{
    __xlogd("api_timeout enter\n");
    char *ip = xl_find_word(&pctx->server->parser, "ip");
    uint16_t port = xl_find_uint(&pctx->server->parser, "port");
    xlmsg_ptr xkv = xl_find_ptr(&pctx->server->parser, "msg");

    if (xchannel_get_keepalive(pctx->channel)){
        if (pctx->reconnected < 3){
            pctx->reconnected++;
            xmsger_connect(pctx->server->msger, ip, port, pctx, xkv);
        }else {
            if (xkv){
                xl_printf(&xkv->line);
            }
        }
    }else {

    }
    __xlogd("api_timeout exit\n");
}

static void api_response(xchannel_ctx_ptr pctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_uint(&pctx->server->parser, "tid");
    xpeer_task_ptr task = (xpeer_task_ptr )index_table_find(&pctx->server->task_table, tid);
    if (task){
        task->handle(task, pctx);
    }
    __xlogd("api_response exit\n");
}

// static xpeer_ptr g_server = NULL;

// static void __sigint_handler()
// {
//     if(g_server){
//         __set_false(g_server->runnig);
//         if (g_server->sock > 0){
//             int sock = __xapi->udp_open();
//             for (int i = 0; i < 10; ++i){
//                 __xapi->udp_sendto(sock, &g_server->addr, &i, sizeof(int));
//             }
//             __xapi->udp_close(sock);
//         }
//     }
// }


static void req_login(xpeer_ptr peer, const char *host, uint16_t port)
{
    xchannel_ctx_ptr ctx = xchannel_ctx_create(peer, NULL);
    peer->server = xpeer_ctx_create(ctx);
    peer->server->ctx->processor = peer->server;
    peer->server->ctx->process = api_processor;    
    // __xapi->udp_hostbyname(pctx->ip, __XAPI_IP_STR_LEN, host);
    mcopy(peer->server->ip, host, slength(host));
    peer->server->ip[slength(host)] = '\0';    
    peer->server->port = port;

    xpeer_task_ptr task = add_task(peer->server->ctx, res_login);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "login");
    xl_add_uint(req, "tid", task->node.index);
    xl_add_uint(req, "key", peer->key);
    xl_add_bin(req, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    xmsger_connect(peer->msger, peer->server->ip, port, peer->server->ctx, req);
}


extern void sigint_setup(void (*handler)());

#include <arpa/inet.h>
#include <string.h>
int main(int argc, char *argv[])
{
    const char *hostname = "pindanci.com";
    // char hostip[16] = {0};
    uint16_t port = 9256;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_ptr peer = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
    __set_true(peer->runnig);

    mcopy(peer->password, "123456", slength("123456"));
    uuid_generate(peer->uuid, "PEER1");
    peer->key = xxhash64(peer->uuid, 32, 0);

    uuid_list_init(&peer->uuid_table);
    index_table_init(&peer->task_table, 256);
    peer->api = xtree_create();

    xtree_add(peer->api, "login", slength("login"), req_login);
    xtree_add(peer->api, "logout", slength("logout"), req_logout);
    xtree_add(peer->api, "res", slength("res"), api_response);
    xtree_add(peer->api, "disconnect", slength("disconnect"), api_disconnect);
    xtree_add(peer->api, "timeout", slength("timeout"), api_timeout);

    xmsgercb_ptr listener = &peer->listener;

    listener->ctx = peer;
    listener->on_connect_to_peer = on_connection_to_peer;
    listener->on_connect_from_peer = on_connection_from_peer;
    listener->on_connect_timeout = on_msg_timeout;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;
    listener->on_msg_timeout = on_msg_timeout;
    listener->on_disconnect = on_disconnect;

    // server->sock = __xapi->udp_open();
    // __xbreak(server->sock < 0);
    // __xbreak(!__xapi->udp_host_to_addr(NULL, 0, &server->addr));
    // __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    // struct sockaddr_in server_addr; 
    // socklen_t addr_len = sizeof(server_addr);
    // __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    // __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    // __xbreak(!__xapi->udp_host_to_addr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));

    peer->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(peer->task_pipe == NULL);

    peer->task_pid = __xapi->process_create(task_loop, peer);
    __xbreak(peer->task_pid == NULL);
    
    peer->msger = xmsger_create(&peer->listener);

    // server->listen_pid = __xapi->process_create(listen_loop, server);
    // __xbreak(server->listen_pid == NULL);

    // __xapi->udp_addrinfo(hostip, 16, hostname);
    peer->port = 9256;
    __xapi->udp_hostbyname(peer->ip, 16, hostname);
    __xlogd("host ip = %s port=%u\n", peer->ip, peer->port);
    const char *cip = "192.168.1.6";
    // const char *cip = "120.78.155.213";
    // const char *cip = "47.92.77.19";
    // const char *cip = "47.99.146.226";

    mcopy(peer->ip, cip, slength(cip));
    peer->ip[slength(cip)] = '\0';

    // xlmsg_ptr msg = xline_maker();
    // xline_add_word(msg, "api", "testreconnect");
    
    // server->pctx_list[0] = xpeer_ctx_create(server, NULL);
    // xmsger_connect(server->msger, hostip, port, server->pctx_list[0], msg);

    // server->cid = 0;
    // server->channel = server->pctx_list[server->cid]->channel;

    char str[1024];
    char input[256];
    char command[256];
    char ip[256] = {0};
    uint64_t cid, key;
    while (peer->runnig)
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
            req_login(peer, peer->ip, peer->port);

        } else if (strcmp(command, "logout") == 0) {
            req_logout(peer->server->ctx);

        } else if (strcmp(command, "list") == 0) {
            
            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            // chord_list(server->ctx);

        }  else if (strcmp(command, "invite") == 0) {

            __xlogi("输入网络节点: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                cid = atoi(input);
            } else {
                __xlogi("输入网络节点失败\n");
                continue;
            }

            // chord_invite(server->ctx);

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

            // req_forward(server->ctx, key);

        }else if (strcmp(command, "exit") == 0) {
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        mclear(command, 256);
    }

    xmsger_free(&peer->msger);

    if (peer->task_pipe){
        xpipe_break(peer->task_pipe);
    }

    if (peer->task_pid){
        __xapi->process_free(peer->task_pid);
    }

    if (peer->task_pipe){
        while (xpipe_readable(peer->task_pipe) > 0){
            // TODO 清理管道
        }
        xpipe_free(&peer->task_pipe);
    }

    xtree_clear(peer->api, NULL);
    xtree_free(&peer->api);
    index_table_clear(&peer->task_table);
    uuid_list_clear(&peer->uuid_table);
    
    // free(server->ctx);
    free(peer);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
