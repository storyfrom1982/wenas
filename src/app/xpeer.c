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

typedef struct xmsg_callback {
    struct avl_node node;
    uint64_t msgid;
    struct xmsg_processor *processor;
    void (*process)(struct xchannel_ctx*, void*);
    struct xmsg_callback *prev, *next;
}xmsg_callback_t;


typedef struct xmsg_processor {
    struct avl_node node;
    void *userctx;
    uint64_t uuid[4];    
    uint16_t port;
    char host[46];
    struct xchannel_ctx *ctx;
    struct xmsg_callback callbacklist;
}xmsg_processor_t;


typedef struct xchannel_ctx {
    uint8_t reconnected;
    xchannel_ptr channel;
    struct xpeer *server;
    void *handler;
    void (*process)(struct xchannel_ctx*, xlmsg_ptr msg);
    struct xchannel_ctx *prev, *next;
}xchannel_ctx_t;


typedef struct xpeer{
    // int sock;
    uint16_t port;
    char ip[46];
    // uint64_t key;
    uint8_t uuid[32];
    __atom_bool runnig;
    xlmsg_t parser;
    uint64_t task_id;
    // struct __xipaddr addr;
    xline_ptr chord_list;
    xmsger_ptr msger;

    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid;
    struct xmsgercb listener;
    
    uint64_t msgid;
    struct avl_tree msgid_table;
    struct avl_tree uuid_table;
    xtree send_api, recv_api;

    struct xchannel_ctx channels;
    xmsg_processor_t *server;

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

static inline int uuid_compare(const void *a, const void *b)
{
    uint64_t *x = ((xmsg_processor_t*)a)->uuid;
    uint64_t *y = ((xmsg_processor_t*)b)->uuid;
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
    uint64_t *y = ((xmsg_processor_t*)b)->uuid;
    for (int i = 0; i < 4; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline void uuid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, uuid_compare, uuid_find_compare, sizeof(xmsg_processor_t), AVL_OFFSET(xmsg_processor_t, node));
}

static inline int msgid_compare(const void *a, const void *b)
{
	return ((xmsg_callback_t*)a)->msgid - ((xmsg_callback_t*)b)->msgid;
}

static inline int msgid_find_compare(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xmsg_callback_t*)b)->msgid;
}

static inline void msgid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, msgid_compare, msgid_find_compare, sizeof(xmsg_callback_t), AVL_OFFSET(xmsg_callback_t, node));
}

static inline xchannel_ctx_t* xchannel_ctx_create(xpeer_t *peer, xchannel_ptr channel)
{
    xchannel_ctx_t *ctx = (xchannel_ctx_t*)calloc(1, sizeof(xchannel_ctx_t));
    ctx->next = peer->channels.next;
    ctx->prev = peer->channels.next->prev;
    ctx->next->prev = ctx;
    ctx->prev->next = ctx;
    ctx->server = peer;
    ctx->channel = channel;
    return ctx;
}

static inline xmsg_processor_t* xmsg_processor_create(xchannel_ctx_t *ctx)
{
    xmsg_processor_t *th = (xmsg_processor_t*)calloc(1, sizeof(xmsg_processor_t));
    th->ctx = ctx;
    th->callbacklist.next = &th->callbacklist;
    th->callbacklist.prev = &th->callbacklist;
    return th;
}

static void on_msg_timeout(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    // __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> 3\n");
    const char *ip = xchannel_get_host(channel);
    uint16_t port = xchannel_get_port(channel);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> port=%u\n", port);
    __xlogd("on_channel_timeout >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_disconnect(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    xlmsg_ptr msg = xl_maker();
    msg->ctx = xchannel_get_ctx(channel);
    xl_add_word(msg, "api", "disconnect");
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_to_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr msg)
{
    // __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xl_free(msg);
    // __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, xlmsg_ptr msg)
{
    // __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    msg->flag = XLMSG_FLAG_RECV;
    msg->ctx = xchannel_get_ctx(channel);
    xpipe_write(server->task_pipe, &msg, __sizeof_ptr);
    // __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
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

inline static xmsg_callback_t* xpeer_add_msg_callback(xmsg_processor_t *mp, uint64_t msgid, void(*callback)(void*))
{
    // __xlogd("xpeer_add_msg_callback enter tid=%lu\n", msgid);
    xmsg_callback_t *handle = (xmsg_callback_t*)malloc(sizeof(struct xmsg_callback));
    handle->processor = mp;
    handle->msgid = msgid;
    handle->process = callback;
    handle->next = mp->callbacklist.next;
    handle->prev = &mp->callbacklist;
    handle->prev->next = handle;
    handle->next->prev = handle;
    avl_tree_add(&mp->ctx->server->msgid_table, handle);
    // __xlogd("xpeer_add_msg_callback exit\n");
    return handle;
}

inline static xmsg_callback_t* xpeer_del_msg_callback(xmsg_processor_t *mp, xmsg_callback_t *handle)
{
    // __xlogd("xpeer_del_msg_callback tid=%lu\n", handle->msgid);
    avl_tree_remove(&mp->ctx->server->msgid_table, handle);
    handle->prev->next = handle->next;
    handle->next->prev = handle->prev;
    free(handle);
}

typedef void(*xapi_handle_t)(xchannel_ctx_t*, xlmsg_ptr);

static inline void xapi_processor(xchannel_ctx_t *ctx, xlmsg_ptr msg)
{
    static uint64_t msgid;
    static xapi_handle_t handle;
    static xmsg_processor_t *processor;
    xl_printf(&msg->line);

    ctx->server->parser = xl_parser(&msg->line);
    const char *api = xl_find_word(&ctx->server->parser, "api");

    if (msg->flag == XLMSG_FLAG_RECV){
        handle = xtree_find(ctx->server->recv_api, api, slength(api));
        if (handle){
            handle(ctx, msg);
        }
        xl_free(msg);
    }else {
        if (msg->wpos > 0 && msg->cb != NULL){
            msgid = xl_find_uint(&ctx->server->parser, "tid");
            xpeer_add_msg_callback(ctx->handler, msgid, msg->cb);
        }
        if (msg->flag == XLMSG_FLAG_SEND){
            xmsger_send(ctx->server->msger, ctx->channel, msg);
        }else if (msg->flag == XLMSG_FLAG_CONNECT){
            processor = (xmsg_processor_t *)ctx->handler;
            xmsger_connect(ctx->server->msger, msg);
        }else if (msg->flag == XLMSG_FLAG_DISCONNECT){
            xmsger_disconnect(ctx->server->msger, ctx->channel, msg);
        }
    }
}

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xlmsg_ptr msg;
    xchannel_ctx_t *ctx;
    xpeer_t *server = (xpeer_t*)ptr;

    while (xpipe_read(server->task_pipe, &msg, __sizeof_ptr) == __sizeof_ptr)
    {
        ctx = msg->ctx;
        if (ctx->process != NULL){
            ctx->process(ctx, msg);
        }else {
            xapi_processor(ctx, msg);
            xl_free(msg);
        }
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

uint64_t xserver_get_msgid(xpeer_t *server)
{
    return __atom_add(server->msgid, 1);
}

#include <string.h>

static void res_logout(void *userctx)
{
    xmsg_processor_t *peerctx = (xmsg_processor_t*)userctx;
    // char hostip[16] = {0};
    // uint16_t port = xchannel_get_port(pctx->channel);
    // const char *ip = xchannel_get_ip(pctx->channel);
    // mcopy(hostip, ip, slength(ip));
    // xmsger_disconnect(peerctx->ctx->server->msger, peerctx->ctx->channel, );
}

static void send_logout(xpeer_t *peer)
{
    xchannel_ctx_t *next, *ctx = peer->channels.next;
    while (ctx != &peer->channels)
    {
        __xlogd("send_logout ----------------------- >>>>>>\n");
        next = ctx->next;
        ctx->next->prev = ctx->prev;
        ctx->prev->next = ctx->next;
        xlmsg_ptr msg = xl_maker();
        msg->flag = XLMSG_FLAG_DISCONNECT;
        // msg->cb = res_login;
        msg->ctx = ctx;
        // xl_add_word(msg, "api", "login");
        // xl_add_uint(msg, "tid", msgid);
        // xl_add_word(msg, "host", mp->host);
        // xl_add_uint(msg, "port", mp->port);
        // xl_add_bin(msg, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
        xpipe_write(peer->task_pipe, &msg, __sizeof_ptr);
        ctx = next;
    }
    
}

static int sendnb = 0;
static void send_echo(xchannel_ctx_t *ctx, const char *text);
static void res_echo(xmsg_processor_t *th, void *userctx)
{
    if (sendnb++ < 10)
    {
        send_echo(th->ctx, "hello world");
    }
}

static void send_echo(xchannel_ctx_t *ctx, const char *text)
{
    xlmsg_ptr msg = xl_maker();
    msg->flag = XLMSG_FLAG_SEND;
    msg->ctx = ctx;
    msg->cb = res_echo;
    xl_add_word(msg, "api", "echo");
    xl_add_uint(msg, "tid", xserver_get_msgid(ctx->server));
    xl_add_word(msg, "text", text);
    xpipe_write(ctx->server->task_pipe, &msg, __sizeof_ptr);
}

static void res_detect(xmsg_processor_t *th, void *userctx)
{
    send_echo(th->ctx, "Hello......");
}

static void send_detect(xpeer_t *peer, const char *host, uint16_t port)
{
    uint64_t msgid = xserver_get_msgid(peer);
    xchannel_ctx_t *ctx = xchannel_ctx_create(peer, NULL);
    xmsg_processor_t *mp = xmsg_processor_create(ctx);
    mp->userctx = NULL;
    ctx->handler = mp;
    ctx->process = xapi_processor;
    // __xapi->udp_hostbyname(pctx->ip, __XAPI_IP_STR_LEN, host);
    mcopy(mp->host, host, slength(host));
    mp->host[slength(host)] = '\0';    
    mp->port = port;
    xlmsg_ptr msg = xl_maker();
    msg->flag = XLMSG_FLAG_CONNECT;
    msg->cb = res_detect;
    msg->ctx = mp->ctx;
    xl_add_word(msg, "api", "detect");
    xl_add_uint(msg, "tid", msgid);
    xl_add_word(msg, "host", mp->host);
    xl_add_uint(msg, "port", mp->port);
    xl_add_bin(msg, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
    xpipe_write(peer->task_pipe, &msg, __sizeof_ptr);
}

static void res_login(xmsg_processor_t *th, void *userctx)
{
    // char *host = xl_find_word(&th->ctx->server->parser, "host");
    // uint16_t port = xl_find_uint(&th->ctx->server->parser, "port");
    send_detect(th->ctx->server, th->ctx->server->ip, 9257);
    send_echo(th->ctx, "Hello......");
}

static void api_disconnect(xchannel_ctx_t *pctx)
{
    __xlogd("api_disconnect ----------------------- enter\n");
    // if (pctx == pctx->server->ctx){
    //     xpeer_t *server = pctx->server;
    //     req_login(server, server->ip, server->port);
    // }
    if (pctx->handler){
        free(pctx->handler);
    }
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
            xmsger_connect(pctx->server->msger, xkv);
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
    // __xlogd("api_response enter\n");
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xmsg_callback_t *handle = (xmsg_callback_t *)avl_tree_find(&ctx->server->msgid_table, &tid);
    if (handle){
        handle->process(handle->processor, handle->processor->userctx);
        xpeer_del_msg_callback(handle->processor, handle);
    }
    // __xlogd("api_response exit\n");
}

static void api_login(xchannel_ctx_t *pctx)
{
    uint64_t tid = xl_find_uint(&pctx->server->parser, "tid");
    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_word(res, "req", "login");
    xl_add_uint(res, "tid", tid);
    xl_add_uint(res, "code", 200);
    xmsger_send(pctx->server->msger, pctx->channel, res);
}

static void api_echo(xchannel_ctx_t *pctx)
{
    uint64_t tid = xl_find_uint(&pctx->server->parser, "tid");
    xlmsg_ptr res = xl_maker();
    xl_add_word(res, "api", "res");
    xl_add_word(res, "req", "echo");
    xl_add_uint(res, "tid", tid);
    xl_add_word(res, "host", xchannel_get_host(pctx->channel));
    xl_add_uint(res, "port", xchannel_get_port(pctx->channel));
    xl_add_uint(res, "code", 200);
    xmsger_send(pctx->server->msger, pctx->channel, res);
}

xchannel_ctx_t* xltp_bootstrap(xpeer_t *peer, const char *host, uint16_t port)
{
    uint64_t msgid = xserver_get_msgid(peer);
    xchannel_ctx_t *ctx = xchannel_ctx_create(peer, NULL);
    xmsg_processor_t *mp = xmsg_processor_create(ctx);
    mp->userctx = NULL;
    ctx->handler = mp;
    ctx->process = xapi_processor;
    // __xapi->udp_hostbyname(pctx->ip, __XAPI_IP_STR_LEN, host);
    mcopy(mp->host, host, slength(host));
    mp->host[slength(host)] = '\0';    
    mp->port = port;
    xlmsg_ptr msg = xl_maker();
    msg->flag = XLMSG_FLAG_CONNECT;
    msg->cb = res_login;
    msg->ctx = mp->ctx;
    xl_add_word(msg, "api", "login");
    xl_add_uint(msg, "tid", msgid);
    xl_add_word(msg, "host", mp->host);
    xl_add_uint(msg, "port", mp->port);
    xl_add_bin(msg, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
    xpipe_write(peer->task_pipe, &msg, __sizeof_ptr);
    return ctx;
}

void xpeer_regisger_send_api(xpeer_t *peer, const char *api, xapi_handle_t handle)
{
    xtree_add(peer->send_api, api, slength(api), handle);
}

void xpeer_regisger_recv_api(xpeer_t *peer, const char *api, xapi_handle_t handle)
{
    xtree_add(peer->recv_api, api, slength(api), handle);
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

    peer->channels.prev = &peer->channels;
    peer->channels.next = &peer->channels;

    mcopy(peer->password, "123456", slength("123456"));
    uuid_generate(peer->uuid, "PEER1");
    // peer->key = xxhash64(peer->uuid, 32, 0);

    uuid_table_init(&peer->uuid_table);
    msgid_table_init(&peer->msgid_table);
    peer->send_api = xtree_create();
    peer->recv_api = xtree_create();

    // xpeer_regisger_send_api(peer, "login", send_login);
    // xpeer_regisger_send_api(peer, "login", send_logout);

    xpeer_regisger_recv_api(peer, "res", api_response);
    xpeer_regisger_recv_api(peer, "disconnect", api_disconnect);
    xpeer_regisger_recv_api(peer, "timeout", api_timeout);
    xpeer_regisger_recv_api(peer, "login", api_login);
    xpeer_regisger_recv_api(peer, "echo", api_echo);

    xmsgercb_ptr listener = &peer->listener;

    listener->ctx = peer;
    listener->on_connection_to_peer = on_connection_to_peer;
    listener->on_connection_from_peer = on_connection_from_peer;
    listener->on_connection_timeout = on_msg_timeout;
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;
    listener->on_msg_timeout = on_msg_timeout;
    listener->on_disconnection = on_disconnect;

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
    
    peer->msger = xmsger_create(&peer->listener, 1);

    // server->listen_pid = __xapi->process_create(listen_loop, server);
    // __xbreak(server->listen_pid == NULL);

    // __xapi->udp_addrinfo(hostip, 16, hostname);
    peer->port = 9256;
    // __xapi->udp_addrinfo(peer->ip, hostname);
    // __xlogd("host ip = %s port=%u\n", peer->ip, peer->port);

    const char *cip = "192.168.1.6";
    // const char *cip = "120.78.155.213";
    // const char *cip = "47.92.77.19";
    // const char *cip = "47.99.146.226";
    // const char *cip = hostname;
    // const char *cip = "2409:8a14:8743:9750:350f:784f:8966:8b52";
    // const char *cip = "2409:8a14:8743:9750:7193:6fc2:f49d:3cdb";
    // const char *cip = "2409:8914:8669:1bf8:5c20:3ccc:1d88:ce38";

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
            xltp_bootstrap(peer, peer->ip, peer->port);

        } else if (strcmp(command, "logout") == 0) {
            send_logout(peer);

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
    xtree_clear(peer->send_api, NULL);
    xtree_free(&peer->send_api);
    xtree_clear(peer->recv_api, NULL);
    xtree_free(&peer->recv_api);

    // free(peer->xltp->handler);
    // free(peer->xltp);
    free(peer);

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}
