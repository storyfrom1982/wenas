#include "xpeer.h"

#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
// #include <xnet/xtable.h>
// #include <xnet/uuid.h>
#include <xlib/xxhash.h>
#include <xlib/xsha256.h>
#include <xlib/avlmini.h>

#include <stdio.h>
#include <stdlib.h>


typedef struct media_stream_task {

}media_stream_task_t;

typedef struct file_download_task {

}file_download_task_t;

typedef struct file_upload_task {

}file_upload_task_t;

typedef struct xmsg_back {
    struct avl_node node;
    uint64_t msgid;
    struct xmsg_task_handler *th;
    void (*callback)(struct xchannel_ctx*, void*);
    struct xmsg_back *prev, *next;
}xmsg_back_t;

typedef struct xchannel_ctx {
    uint8_t reconnected;
    xchannel_ptr channel;
    struct xpeer *server;
    void *handler;
    void (*task_handle)(struct xchannel_ctx*, xlmsg_ptr msg);
}xchannel_ctx_t;

typedef struct xmsg_task_handler {
    struct avl_node node;
    void *userctx;
    uint64_t uuid[4];    
    uint16_t port;
    char ip[__XAPI_IP_STR_LEN];
    xchannel_ctx_t *ctx;
    struct xmsg_back handlelist;
}xmsg_task_handler_t;


struct chord_node {
    char ip[16];
    uint16_t port;
    uint64_t key;
    struct chord_node *prev;
    struct chord_node *next;
};


typedef struct xpeer{
    // int sock;
    uint16_t port;
    char ip[__XAPI_IP_STR_LEN];
    // uint64_t key;
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
    
    uint64_t msgid;
    struct avl_tree msgid_table;
    struct avl_tree uuid_table;
    xtree api;

    char *bootstrap_host;
    struct xchannel_ctx *xltp;
    xmsg_task_handler_t *server;

    char password[16];
    // 通信录，好友ID列表，一个好友ID可以对应多个设备地址，但是只与主设备建立一个 channel，设备地址是动态更新的，不需要存盘。好友列表需要写数据库
    // 设备列表，用户自己的所有设备，每个设备建立一个 channel，channel 的消息实时共享，设备地址是动态更新的，设备 ID 需要存盘
    // 任务树，每个请求（发起的请求或接收的请求）都是一个任务，每个任务都有开始，执行和结束。请求是任务的开始，之后可能有多次消息传递，数据传输完成，双方各自结束任务，释放资源。
    // 单个任务本身的消息是串行的，但是多个任务之间是并行的，所以用树来维护任务列表，以便多个任务切换时，可以快速定位任务。
    // 每个任务都有一个唯一ID，通信双方维护各自的任务ID，发型消息时，要携带自身的任务ID并且指定对方的任务ID
    // 每个任务都有执行进度
};

#define UUID_BIN_BUF_LEN       32  
#define UUID_HEX_BUF_LEN       65


static void* uuid_generate(void *uuid_bin_buf, const char *user_name)
{
    int name_len = slength(user_name);
    char seed[80] = {0};
    SHA256_CTX shactx;
    uint64_t *millisecond = (uint64_t *)seed;
    *millisecond = __xapi->time();
    millisecond++;
    *millisecond = __xapi->clock();
    if (name_len > 64){
        name_len = 64;
    }
    mcopy(seed + 16, user_name, name_len);
    sha256_init(&shactx);
    sha256_update(&shactx, seed, name_len + 16);
    sha256_finish(&shactx, uuid_bin_buf);
    return uuid_bin_buf;
}


static inline unsigned char* str2uuid(const char* hexstr, uint8_t *uuid) {
    for (size_t i = 0; i < UUID_BIN_BUF_LEN; ++i) {
        char a = hexstr[i * 2];
        char b = hexstr[i * 2 + 1];
        a = (a >= '0' && a <= '9') ? a - '0' : a - 'A' + 10;
        b = (b >= '0' && b <= '9') ? b - '0' : b - 'A' + 10;
        uuid[i] = ((a << 4) | b);
    }
    return uuid;
}

static inline char* uuid2str(const unsigned char *uuid, char *hexstr) {
    const static char hex_chars[] = "0123456789ABCDEF";
    for (size_t i = 0; i < UUID_BIN_BUF_LEN; ++i) {
        hexstr[i * 2] = hex_chars[(uuid[i] >> 4) & 0x0F];
        hexstr[i * 2 + 1] = hex_chars[uuid[i] & 0x0F];
    }
    hexstr[UUID_HEX_BUF_LEN-1] = '\0';
    return hexstr;
}

static inline xchannel_ctx_t* xchannel_ctx_create(xpeer_t *server, xchannel_ptr channel)
{
    xchannel_ctx_t *ctx = (xchannel_ctx_t*)calloc(1, sizeof(xchannel_ctx_t));
    ctx->server = server;
    ctx->channel = channel;
    return ctx;
}

static inline xmsg_task_handler_t* xmsg_task_handle_create(xchannel_ctx_t *ctx)
{
    xmsg_task_handler_t *th = (xmsg_task_handler_t*)calloc(1, sizeof(xmsg_task_handler_t));
    th->ctx = ctx;
    th->handlelist.next = &th->handlelist;
    th->handlelist.prev = &th->handlelist;
    return th;
}

static void on_msg_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    // __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 3\n");
    const char *ip = xchannel_get_ip(channel);
    uint16_t port = xchannel_get_port(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> port=%u\n", port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_disconnect(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
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
    xpeer_t *server = (xpeer_t*)listener->ctx;
    msg->ctx = xchannel_get_ctx(channel);
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    xchannel_ctx_t *ctx = xchannel_get_ctx(channel);
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
    xchannel_ctx_t *pctx = xchannel_ctx_create(listener->ctx, channel);
    xchannel_set_ctx(channel, pctx);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

typedef void(*xapi_handle_t)(xchannel_ctx_t*);

static void xapi_processor(xchannel_ctx_t *ctx, xlmsg_ptr msg)
{
    static xapi_handle_t handle;
    xl_printf(&msg->line);
    ctx->server->parser = xl_parser(&msg->line);
    const char *api = xl_find_word(&ctx->server->parser, "api");
    handle = xtree_find(ctx->server->api, api, slength(api));
    if (handle){
        handle(ctx);
    }
}

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xlmsg_ptr msg;
    xchannel_ctx_t *ctx;
    xapi_handle_t handle;
    xpeer_t *server = (xpeer_t*)ptr;

    while (xpipe_read(server->task_pipe, &msg, __sizeof_ptr) == __sizeof_ptr)
    {
        ctx = msg->ctx;
        if (ctx->task_handle != NULL){
            ctx->task_handle(ctx, msg);
        }else {
            xl_printf(&msg->line);
            server->parser = xl_parser(&msg->line);
            const char *api = xl_find_word(&server->parser, "api");
            handle = xtree_find(ctx->server->api, api, slength(api));
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

inline static xmsg_back_t* xpeer_add_msg_feedback(xmsg_task_handler_t *th, void(*callback)(void*))
{
    __xlogd("xpeer_add_msg_feedback enter\n");
    xmsg_back_t *handle = (xmsg_back_t*)malloc(sizeof(struct xmsg_back));
    handle->th = th;
    handle->msgid = th->ctx->server->msgid++;
    handle->callback = callback;
    handle->next = th->handlelist.next;
    handle->prev = &th->handlelist;
    handle->prev->next = handle;
    handle->next->prev = handle;
    avl_tree_add(&th->ctx->server->msgid_table, handle);
    __xlogd("xpeer_add_msg_feedback exit\n");
    return handle;
}

inline static xmsg_back_t* xpeer_del_msg_feedback(xmsg_task_handler_t *th, xmsg_back_t *handle)
{
    __xlogd("xpeer_del_msg_feedback tid=%lu\n", handle->msgid);
    avl_tree_remove(&th->ctx->server->msgid_table, handle);
    handle->prev->next = handle->next;
    handle->next->prev = handle->prev;
    free(handle);
}

#include <string.h>

static void res_logout(void *userctx)
{
    xmsg_task_handler_t *peerctx = (xmsg_task_handler_t*)userctx;
    // char hostip[16] = {0};
    // uint16_t port = xchannel_get_port(pctx->channel);
    // const char *ip = xchannel_get_ip(pctx->channel);
    // mcopy(hostip, ip, slength(ip));
    xmsger_disconnect(peerctx->ctx->server->msger, peerctx->ctx->channel);
}

static void req_logout(xchannel_ctx_t *ctx)
{
    // xpeer_t *peer = ctx->server;
    // xmsg_back_t *task = xpeer_add_msg_feedback(ctx, ctx->handler, res_logout);
    // xlmsg_ptr req = xl_maker();
    // xl_add_word(req, "api", "logout");
    // xl_add_uint(req, "tid", task->msgid);
    // // xl_add_uint(req, "key", peer->key);
    // xl_add_bin(req, "uuid", peer->uuid, UUID_BIN_BUF_LEN);
    // uint8_t buf[2048];
    // memset(buf, 'l', 2047);
    // buf[2047] = '\0';
    // xl_add_str(req, "str", buf, 2048);
    // xmsger_send_message(peer->msger, ctx->channel, req);
}

static void req_echo(xchannel_ctx_t *ctx, const char *text);
static void res_echo(xmsg_task_handler_t *th, void *userctx)
{
    req_echo(th->ctx, "hello world");
}

static void req_echo(xchannel_ctx_t *ctx, const char *text)
{
    xmsg_back_t *cb = xpeer_add_msg_feedback(ctx->handler, res_echo);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "echo");
    xl_add_uint(req, "tid", cb->msgid);
    xl_add_word(req, "text", text);
    xmsger_send_message(ctx->server->msger, ctx->channel, req);
}

static void res_login(xmsg_task_handler_t *th, void *userctx)
{
    req_echo(th->ctx, "Hello......");
}

// static void req_login(xpeer_t *peer, const char *ip, uint16_t port)
// {
//     __xlogd("req_login ----------------------- enter\n");
//     // peer->ctx = xchannel_ctx_create(peer, NULL);
//     // peer->ctx->processor = xpeer_ctx_create(peer->ctx);
//     // peer->ctx->process = api_processor;
//     // msg_ctx_t *task = add_task(peer->ctx, res_login);
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

static void api_disconnect(xchannel_ctx_t *pctx)
{
    __xlogd("api_disconnect ----------------------- enter\n");
    // if (pctx == pctx->server->ctx){
    //     xpeer_t *server = pctx->server;
    //     req_login(server, server->ip, server->port);
    // }
    free(pctx);
    __xlogd("api_disconnect ----------------------- exit\n");
}

static void api_timeout(xchannel_ctx_t *pctx)
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

static void api_response(xchannel_ctx_t *ctx)
{
    __xlogd("api_response enter\n");
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xmsg_back_t *handle = (xmsg_back_t *)avl_tree_find(&ctx->server->msgid_table, &tid);
    if (handle){
        handle->callback(handle->th, handle->th->userctx);
        xpeer_del_msg_feedback(handle->th, handle);
    }
    __xlogd("api_response exit\n");
}

static inline int uuid_compare(const void *a, const void *b)
{
    uint64_t *x = ((xmsg_task_handler_t*)a)->uuid;
    uint64_t *y = ((xmsg_task_handler_t*)b)->uuid;
    for (int i = 0; i < 4; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline int uuid_find_compare(const void *a, const void *b)
{
    uint64_t *x = (uint64_t*)a;
    uint64_t *y = ((xmsg_task_handler_t*)b)->uuid;
    for (int i = 0; i < 4; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

void uuid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, uuid_compare, uuid_find_compare, sizeof(xmsg_task_handler_t), AVL_OFFSET(xmsg_task_handler_t, node));
}

static inline int msgid_compare(const void *a, const void *b)
{
	return ((xmsg_back_t*)a)->msgid - ((xmsg_back_t*)b)->msgid;
}

static inline int msgid_find_compare(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xmsg_back_t*)b)->msgid;
}

void msgid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, msgid_compare, msgid_find_compare, sizeof(xmsg_back_t), AVL_OFFSET(xmsg_back_t, node));
}

// static xmsg_task_handler_t* req_login(xpeer_t *peer, const char *host, uint16_t port, void *userctx, void(*cb)(void*))
static void req_login(xchannel_ctx_t *ctx)
{
    // xchannel_ctx_t *ctx = xchannel_ctx_create(peer, NULL);
    // xmsg_task_handler_t *th = xtask_handle_create(ctx);
    // th->userctx = userctx;
    // th->ctx->handler = th;
    // th->ctx->task_handle = xapi_processor;
    // // __xapi->udp_hostbyname(pctx->ip, __XAPI_IP_STR_LEN, host);
    // mcopy(th->ip, host, slength(host));
    // th->ip[slength(host)] = '\0';    
    // th->port = port;
    xmsg_task_handler_t *th = ctx->handler;
    xmsg_back_t *handle = xpeer_add_msg_feedback(th, res_login);
    xlmsg_ptr req = xl_maker();
    xl_add_word(req, "api", "login");
    xl_add_uint(req, "tid", handle->msgid);
    xl_add_bin(req, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
    xmsger_connect(ctx->server->msger, th->ip, th->port, ctx, req);
}

xchannel_ctx_t* xltp_bootstrap(xpeer_t *peer, const char *host, uint16_t port)
{
    xchannel_ctx_t *ctx = xchannel_ctx_create(peer, NULL);
    xmsg_task_handler_t *mth = xmsg_task_handle_create(ctx);
    mth->userctx = NULL;
    ctx->handler = mth;
    ctx->task_handle = xapi_processor;
    // __xapi->udp_hostbyname(pctx->ip, __XAPI_IP_STR_LEN, host);
    mcopy(mth->ip, host, slength(host));
    mth->ip[slength(host)] = '\0';    
    mth->port = port;
    xlmsg_ptr req = xl_maker();
    req->ctx = mth->ctx;
    xl_add_word(req, "api", "login");
    // xl_add_word(req, "host", host);
    // xl_add_uint(req, "port", port);
    xpipe_write(peer->task_pipe, &req, __sizeof_ptr);
    return ctx;
}


void xpeer_regisger_api(xpeer_t *peer, const char *api, xapi_handle_t handle)
{
    xtree_add(peer->api, api, slength(api), handle);
}


extern void sigint_setup(void (*handler)());

#include <arpa/inet.h>
#include <string.h>
int main(int argc, char *argv[])
{
    const char *hostname = "healthtao.cn";
    // char hostip[16] = {0};
    uint16_t port = 9256;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_t *peer = (xpeer_t*)calloc(1, sizeof(struct xpeer));
    __set_true(peer->runnig);

    mcopy(peer->password, "123456", slength("123456"));
    uuid_generate(peer->uuid, "PEER1");
    // peer->key = xxhash64(peer->uuid, 32, 0);

    uuid_table_init(&peer->uuid_table);
    msgid_table_init(&peer->msgid_table);
    peer->api = xtree_create();

    xpeer_regisger_api(peer, "login", req_login);
    // xpeer_regisger_api(peer, "logout", req_logout);
    xpeer_regisger_api(peer, "res", api_response);
    xpeer_regisger_api(peer, "disconnect", api_disconnect);
    xpeer_regisger_api(peer, "timeout", api_timeout);
    xpeer_regisger_api(peer, "bootstrap", api_timeout);

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
            peer->xltp = xltp_bootstrap(peer, peer->ip, peer->port);

        } else if (strcmp(command, "logout") == 0) {
            req_logout(peer->server);

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

    avl_tree_clear(&peer->msgid_table, NULL);
    avl_tree_clear(&peer->uuid_table, NULL);
    xtree_clear(peer->api, NULL);
    xtree_free(&peer->api);

    free(peer->xltp->handler);
    free(peer->xltp);
    free(peer);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
