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

typedef struct xchannel_ctx {
    struct avl_node node;
    uint8_t reconnected;
    void *userctx;
    uint64_t uuid[4];    
    // uint16_t port;
    // char host[46];
    xchannel_ptr channel;
    struct xpeer *server;
    void *handler;
    void (*process)(struct xchannel_ctx*, xline_t *msg);
    xline_t msglist;
    struct xchannel_ctx *prev, *next;
}xpeer_ctx_t;

typedef void (*msg_cb_t)(xpeer_ctx_t*, void*);

typedef struct xpeer {
    // int sock;
    uint16_t port;
    char ip[46];
    uint16_t pub_port;
    char pub_ip[46];
    // uint64_t key;
    uint8_t uuid[32];
    __atom_bool runnig;
    xline_t parser;
    uint64_t task_id;
    // struct __xipaddr addr;
    xdata_t *chord_list;
    xmsger_ptr msger;

    xpipe_ptr mpipe;
    __xthread_ptr task_pid;
    struct xmsgercb listener;
    
    xline_t msglist;
    uint64_t msgid;
    struct avl_tree msgid_table;
    struct avl_tree uuid_table;
    xtree recv_api;

    uint64_t channel_count;
    struct xchannel_ctx channels;
    xpeer_ctx_t *server;

    char password[16];
    // 通信录，好友ID列表，一个好友ID可以对应多个设备地址，但是只与主设备建立一个 channel，设备地址是动态更新的，不需要存盘。好友列表需要写数据库
    // 设备列表，用户自己的所有设备，每个设备建立一个 channel，channel 的消息实时共享，设备地址是动态更新的，设备 ID 需要存盘
    // 任务树，每个请求（发起的请求或接收的请求）都是一个任务，每个任务都有开始，执行和结束。请求是任务的开始，之后可能有多次消息传递，数据传输完成，双方各自结束任务，释放资源。
    // 单个任务本身的消息是串行的，但是多个任务之间是并行的，所以用树来维护任务列表，以便多个任务切换时，可以快速定位任务。
    // 每个任务都有一个唯一ID，通信双方维护各自的任务ID，发型消息时，要携带自身的任务ID并且指定对方的任务ID
    // 每个任务都有执行进度
}xpeer_t;

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
    sha256_update(&shactx, (const uint8_t*)seed, name_len + 16);
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
    uint64_t *x = ((xpeer_ctx_t*)a)->uuid;
    uint64_t *y = ((xpeer_ctx_t*)b)->uuid;
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
    uint64_t *y = ((xpeer_ctx_t*)b)->uuid;
    for (int i = 0; i < 4; ++i){
        if (x[i] != y[i]){
            return (x[i] > y[i]) ? 1 : -1;
        }
    }
    return 0;
}

static inline void uuid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, uuid_compare, uuid_find_compare, sizeof(xpeer_ctx_t), AVL_OFFSET(xpeer_ctx_t, node));
}

static inline int msgid_compare(const void *a, const void *b)
{
	return ((xline_t*)a)->index - ((xline_t*)b)->index;
}

static inline int msgid_find(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xline_t*)b)->index;
}

static inline void msgid_table_init(struct avl_tree *table)
{
    avl_tree_init(table, msgid_compare, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));
}

uint64_t xserver_get_msgid(xpeer_t *server)
{
    return __atom_add(server->msgid, 1);
}

static inline xline_t* xmsg_new(xpeer_ctx_t *ctx, uint8_t flag, msg_cb_t cb)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = flag;
    msg->index = __atom_add(ctx->server->msgid, 1);
    msg->cb = cb;
    msg->ctx = ctx;
    return msg;
XClean:
    return NULL;
}

static inline void xpeer_ctx_free(xpeer_ctx_t *ctx)
{
    ctx->prev->next = ctx->next;
    ctx->next->prev = ctx->prev;
    xline_t *next, *msg = ctx->msglist.next;
    while (msg != &ctx->msglist){
        next = msg->next;
        msg->prev->next = msg->next;
        msg->next->prev = msg->prev;
        avl_tree_remove(&ctx->server->msgid_table, msg);
        if (msg->cb){
            ((msg_cb_t)(msg->cb))(ctx, ctx->userctx);
        }
        xl_free(&msg);
        msg = next;
    }
    free(ctx);
}

static inline xpeer_ctx_t* xpeer_ctx_create(xpeer_t *peer, xchannel_ptr channel)
{
    xpeer_ctx_t *ctx = (xpeer_ctx_t*)calloc(1, sizeof(xpeer_ctx_t));
    ctx->next = peer->channels.next;
    ctx->prev = peer->channels.next->prev;
    ctx->next->prev = ctx;
    ctx->prev->next = ctx;
    ctx->server = peer;
    ctx->channel = channel;
    if (channel){
        xchannel_set_ctx(channel, ctx);
    }
    ctx->msglist.next = &ctx->msglist;
    ctx->msglist.prev = &ctx->msglist;
    return ctx;
}

inline static void xpeer_add_msg_cb(xpeer_ctx_t *ctx, xline_t *msg)
{
    xl_hold(msg);
    msg->next = ctx->msglist.next;
    msg->prev = &ctx->msglist;
    msg->prev->next = msg;
    msg->next->prev = msg;
    avl_tree_add(&ctx->server->msgid_table, msg);
}

inline static void xpeer_del_msg_cb(xpeer_ctx_t *ctx, xline_t *msg)
{
    avl_tree_remove(&ctx->server->msgid_table, msg);
    msg->prev->next = msg->next;
    msg->next->prev = msg->prev;
    xl_free(&msg);
}

// static inline xmsg_processor_t* xmsg_processor_create(xchannel_ctx_t *ctx)
// {
//     xmsg_processor_t *th = (xmsg_processor_t*)calloc(1, sizeof(xmsg_processor_t));
//     th->ctx = ctx;
//     th->msglist.next = &th->msglist;
//     th->msglist.prev = &th->msglist;
//     return th;
// }

static int on_message_timeout(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    xpeer_ctx_t *ctx = xchannel_get_ctx(channel);
    if (ctx){
        if (msg != NULL){
            msg->flag = XL_MSG_FLAG_TIMEOUT;
            msg->args[0] = channel;
        }
        __xcheck(xpipe_write(((xpeer_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    }
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static void on_disconnect(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    xpeer_ctx_t *ctx = xchannel_get_ctx(channel);
    __xcheck(ctx == NULL);
    ctx->channel = channel;
    xline_t *msg = xl_maker();
    msg->flag = XL_MSG_FLAG_RECV;
    msg->ctx = ctx;
    xl_add_word(&msg, "api", "disconnect");
    __xcheck(xpipe_write(server->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> exit %lu\n", xpipe_readable(server->mpipe));
    return;
XClean:
    __xlogd("on_disconnect >>>>>>>>>>>>>>>>>>>>---------------> failed\n");
    return;
}

static int on_message_to_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XL_MSG_FLAG_BACK;
    msg->args[0] = channel;
    __xcheck(xpipe_write(((xpeer_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_from_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XL_MSG_FLAG_RECV;
    msg->args[0] = channel;
    __xcheck(xpipe_write(((xpeer_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_t *server = (xpeer_t*)listener->ctx;
    xpeer_ctx_t *ctx = xchannel_get_ctx(channel);
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
    xpeer_ctx_t *pctx = xpeer_ctx_create(listener->ctx, channel);
    xchannel_set_ctx(channel, pctx);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

typedef void(*xapi_handle_t)(xpeer_ctx_t*);

static inline int api_process(xpeer_t *server, xpeer_ctx_t *ctx, xline_t *msg)
{
    static char *api;
    static xapi_handle_t handle;
    server->parser = xl_parser(&msg->data);
    xl_printf(&msg->data);
    api = xl_find_word(&server->parser, "api");
    __xcheck(api == NULL);
    handle = xtree_find(server->recv_api, api, slength(api));
    if (handle){
        handle(ctx);
    }
    return 0;
XClean:
    return -1;
}

static void task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    char *api;
    xline_t *msg;
    uint64_t msgid;
    xpeer_ctx_t *ctx;
    xchannel_ptr channel;
    xapi_handle_t handle;
    xpeer_t *server = (xpeer_t*)ptr;

    while (server->runnig)
    {
        __xcheck(xpipe_read(server->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        channel = msg->args[0];

        if (msg->flag == XL_MSG_FLAG_RECV){

            if (msg->type == XPACK_TYPE_MSG){
                __xlogd("task_loop XL_MSG_TYPE_MSG\n");
                ctx = xchannel_get_ctx(channel);
                api_process(server, ctx, msg);
            }else if (msg->type == XPACK_TYPE_HELLO){
                __xlogd("task_loop XL_MSG_TYPE_HELLO\n");
                ctx = xpeer_ctx_create(server, channel);
                api_process(server, ctx, msg);
            }else if (msg->type == XPACK_TYPE_BYE){
                __xlogd("task_loop XL_MSG_TYPE_BYE\n");
                ctx = xchannel_get_ctx(channel);
                api_process(server, ctx, msg);
                xmsger_flush(server->msger, channel);
                xpeer_ctx_free(ctx);
            }
            xl_free(&msg);

        }else if(msg->flag == XL_MSG_FLAG_BACK){

            if (msg->type == XPACK_TYPE_MSG){
                __xlogd("task_loop XL_MSG_FLAG_BACK XL_MSG_TYPE_MSG\n");
            }else if (msg->type == XPACK_TYPE_HELLO){
                __xlogd("task_loop XL_MSG_FLAG_BACK XL_MSG_TYPE_HELLO\n");
                ctx = xchannel_get_ctx(channel);
                ctx->channel = channel;
                
            }else if (msg->type == XPACK_TYPE_BYE){
                __xlogd("task_loop XL_MSG_FLAG_BACK XL_MSG_TYPE_BYE\n");
                xmsger_flush(server->msger, channel);
                xpeer_ctx_free(ctx);
            }
            xl_free(&msg);

        }else if(msg->flag == XL_MSG_FLAG_TIMEOUT){

        }else if(msg->flag == XL_MSG_FLAG_SEND){
            xpeer_ctx_t *ctx = msg->ctx;
            if (msg->cb != NULL){
                xpeer_add_msg_cb(msg->ctx, msg);
            }
            if (msg->type == XPACK_TYPE_MSG){
                xmsger_send(server->msger, ctx->channel, msg);
            }else if (msg->type == XPACK_TYPE_HELLO){
                xmsger_connect(server->msger, msg->ctx, msg);
            }else if (msg->type == XPACK_TYPE_BYE){
                xmsger_disconnect(server->msger, ctx->channel, msg);
            }
        }

    }

    __xlogd("task_loop >>>>---------------> exit\n");

XClean:

    return;
}

xpeer_ctx_t* xltp_login(xpeer_t *peer, const char *host, uint16_t port);
static void xltp_logout(xpeer_t *peer);

static void res_logout(xpeer_ctx_t *ctx, void *userctx)
{
    if (!ctx->server->runnig){
        return;
    }
    if (--ctx->server->channel_count == 0){
        xltp_login(ctx->server, ctx->server->ip, ctx->server->port);
    }
    __xlogi("res_logout --------------------- \n");
}

static void req_hello(xpeer_ctx_t *ctx);
static void res_hello(xpeer_ctx_t *ctx, void *userctx)
{
    req_hello(ctx);
}
static void req_hello(xpeer_ctx_t *ctx)
{
    xline_t *msg = xmsg_new(ctx, XL_MSG_FLAG_SEND, res_hello);
    __xcheck(msg == NULL);
    msg->type = XPACK_TYPE_MSG;
    xl_add_word(&msg, "api", "hello");
    xl_add_uint(&msg, "tid", msg->index);
    xl_add_word(&msg, "host", ctx->server->ip);
    xl_add_uint(&msg, "port", ctx->server->port+1);
    uint8_t uuid[4096];
    xl_add_bin(&msg, "uuid", uuid, 4096);
    // xl_add_bin(&msg, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
    __xcheck(xpipe_write(ctx->server->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return;
}

static void res_echo(xpeer_ctx_t *ctx, void *userctx)
{
    ctx->server->channel_count ++;
    char *host = xl_find_word(&ctx->server->parser, "host");
    uint16_t port = xl_find_uint(&ctx->server->parser, "port");
    __xlogi("echo public address[%s:%u]\n", host, port);
    if (port == ctx->server->pub_port){
        __xlogi("enable puching\n");
    }else {
        __xlogi("disable puching\n");
    }
    xline_t *msg = xl_maker();
    msg->ctx = ctx;
    msg->type = XPACK_TYPE_BYE;
    xmsger_disconnect(ctx->server->msger, ctx->channel, msg);
}

static void res_login(xpeer_ctx_t *ctx, void *user)
{
    if (!ctx->server->runnig){
        return;
    }
    ctx->server->channel_count ++;
    char *host = xl_find_word(&ctx->server->parser, "host");
    ctx->server->pub_port = xl_find_uint(&ctx->server->parser, "port");
    mcopy(ctx->server->pub_ip, host, slength(host));
    __xlogi("public address[%s:%u]\n", host, ctx->server->pub_port);
    xpeer_ctx_t *new_ctx = xpeer_ctx_create(ctx->server, NULL);
    __xcheck(new_ctx == NULL);
    new_ctx->handler = NULL;
    new_ctx->process = NULL;
    xline_t *msg = xmsg_new(new_ctx, XL_MSG_FLAG_SEND, res_echo);
    __xcheck(msg == NULL);
    msg->type = XPACK_TYPE_HELLO;
    xl_add_word(&msg, "api", "echo");
    xl_add_uint(&msg, "tid", msg->index);
    xl_add_word(&msg, "host", new_ctx->server->ip);
    xl_add_uint(&msg, "port", new_ctx->server->port+1);
    xl_add_bin(&msg, "uuid", new_ctx->server->uuid, UUID_BIN_BUF_LEN);
    __xcheck(xpipe_write(new_ctx->server->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return;
XClean:
    __xlogd("res login error free 1\n");
    if (new_ctx != NULL){
        __xlogd("res login error free 2\n");
        xpeer_ctx_free(new_ctx);
    }
    if (msg != NULL){
        __xlogd("res login error free 3\n");
        xl_free(&msg);
    }
    return;
}

static void api_res(xpeer_ctx_t *ctx)
{
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xline_t *msg = (xline_t*)avl_tree_find(&ctx->server->msgid_table, &tid);
    if (msg && msg->cb){
        ((msg_cb_t)(msg->cb))(msg->ctx, NULL);
        xpeer_del_msg_cb(msg->ctx, msg);
    }
}

static void api_hello(xpeer_ctx_t *ctx)
{
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xline_t *res = xl_maker();
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "hello");
    xl_add_uint(&res, "tid", tid);
    xl_add_word(&res, "host", xchannel_get_host(ctx->channel));
    xl_add_uint(&res, "port", xchannel_get_port(ctx->channel));
    uint8_t uuid[4096];
    xl_add_bin(&res, "uuid", uuid, 4096);
    xl_add_uint(&res, "code", 200);
    xmsger_send(ctx->server->msger, ctx->channel, res);
}

static void api_login(xpeer_ctx_t *ctx)
{
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xline_t *res = xl_maker();
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "login");
    xl_add_uint(&res, "tid", tid);
    xl_add_word(&res, "host", xchannel_get_host(ctx->channel));
    xl_add_uint(&res, "port", xchannel_get_port(ctx->channel));
    xl_add_uint(&res, "code", 200);
    xmsger_send(ctx->server->msger, ctx->channel, res);
}

static void api_echo(xpeer_ctx_t *ctx)
{
    uint64_t tid = xl_find_uint(&ctx->server->parser, "tid");
    xline_t *res = xl_maker();
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "echo");
    xl_add_uint(&res, "tid", tid);
    xl_add_word(&res, "host", xchannel_get_host(ctx->channel));
    xl_add_uint(&res, "port", xchannel_get_port(ctx->channel));
    xl_add_uint(&res, "code", 200);
    xmsger_send(ctx->server->msger, ctx->channel, res);
}

static void xltp_logout(xpeer_t *peer)
{
    if (!peer->runnig){
        return;
    }
    xline_t *msg = NULL;
    xpeer_ctx_t *ctx = peer->channels.next;
    while (ctx != &peer->channels){
        // msg = xl_maker();
        // msg->flag = XLMSG_FLAG_DISCONNECT;
        // msg->index = xserver_get_msgid(peer);
        // msg->cb = res_logout;
        // msg->ctx = ctx;
        msg = xmsg_new(ctx, XL_MSG_FLAG_SEND, res_logout);
        __xcheck(msg == NULL);
        msg->type = XPACK_TYPE_BYE;
        __xcheck(xpipe_write(peer->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
        ctx = ctx->next;
    }
    return;
XClean:
    __xlogd("logout error free 1\n");
    if (msg != NULL){
        __xlogd("logout error free 2\n");
        xl_free(&msg);
    }
    return;
}

xpeer_ctx_t* xltp_login(xpeer_t *peer, const char *host, uint16_t port)
{
    if (!peer->runnig){
        return NULL;
    }
    xpeer_ctx_t *ctx = xpeer_ctx_create(peer, NULL);
    __xcheck(ctx == NULL);
    ctx->handler = NULL;
    ctx->process = NULL;
    xline_t *msg = xmsg_new(ctx, XL_MSG_FLAG_SEND, res_echo);
    __xcheck(msg == NULL);
    msg->type = XPACK_TYPE_HELLO;
    xl_add_word(&msg, "api", "echo");
    xl_add_uint(&msg, "tid", msg->index);
    xl_add_word(&msg, "host", host);
    xl_add_uint(&msg, "port", port);
    xl_add_bin(&msg, "uuid", ctx->server->uuid, UUID_BIN_BUF_LEN);
    __xcheck(xpipe_write(peer->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return ctx;
XClean:
    if (ctx != NULL){
        xpeer_ctx_free(ctx);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return NULL;
}

void xpeer_regisger_recv_api(xpeer_t *peer, const char *api, xapi_handle_t handle)
{
    xtree_add(peer->recv_api, (void*)api, slength(api), handle);
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
    __xcheck(peer == NULL);

    __set_true(peer->runnig);
    peer->channels.prev = &peer->channels;
    peer->channels.next = &peer->channels;

    mcopy(peer->password, "123456", slength("123456"));
    uuid_generate(peer->uuid, "PEER1");
    // peer->key = xxhash64(peer->uuid, 32, 0);

    uuid_table_init(&peer->uuid_table);
    msgid_table_init(&peer->msgid_table);
    peer->recv_api = xtree_create();

    xpeer_regisger_recv_api(peer, "res", api_res);
    xpeer_regisger_recv_api(peer, "login", api_login);
    xpeer_regisger_recv_api(peer, "echo", api_echo);
    xpeer_regisger_recv_api(peer, "hello", api_hello);

    xmsgercb_ptr listener = &peer->listener;

    listener->ctx = peer;
    // listener->on_connection_to_peer = on_connection_to_peer;
    // listener->on_connection_from_peer = on_connection_from_peer;
    listener->on_message_from_peer = on_message_from_peer;
    listener->on_message_to_peer = on_message_to_peer;
    // listener->on_disconnection = on_disconnect;
    listener->on_message_timeout = on_message_timeout;

    // server->sock = __xapi->udp_open();
    // __xbreak(server->sock < 0);
    // __xbreak(!__xapi->udp_host_to_addr(NULL, 0, &server->addr));
    // __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    // struct sockaddr_in server_addr; 
    // socklen_t addr_len = sizeof(server_addr);
    // __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    // __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    // __xbreak(!__xapi->udp_host_to_addr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));

    peer->mpipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xcheck(peer->mpipe == NULL);

    peer->task_pid = __xapi->thread_create(task_loop, peer);
    __xcheck(peer->task_pid == NULL);
    
    peer->msger = xmsger_create(&peer->listener, 0, 9256);

    // server->listen_pid = __xapi->process_create(listen_loop, server);
    // __xbreak(server->listen_pid == NULL);

    // __xapi->udp_addrinfo(hostip, 16, hostname);
    peer->port = 9256;

    // __xapi->udp_addrinfo(peer->ip, hostname);
    // __xlogd("host ip = %s port=%u\n", peer->ip, peer->port);

    // const char *cip = "192.168.1.6";
    // const char *cip = "120.78.155.213";
    const char *cip = "47.92.77.19";
    // const char *cip = "47.99.146.226";
    // const char *cip = hostname;
    // const char *cip = "2409:8a14:8743:9750:350f:784f:8966:8b52";
    // const char *cip = "2409:8a14:8743:9750:7193:6fc2:f49d:3cdb";
    // const char *cip = "2409:8914:8669:1bf8:5c20:3ccc:1d88:ce38";

    mcopy(peer->ip, cip, slength(cip));
    peer->ip[slength(cip)] = '\0';

    // xline_t *msg = xline_maker();
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
            xltp_login(peer, peer->ip, peer->port);

        } else if (strcmp(command, "logout") == 0) {
            // xltp_logout(peer);

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

        }else if (strcmp(command, "test") == 0) {
            for (int i = 0; i < 1000; i++)
            {
                xline_t *msg = xl_test(i);
                // xl_printf(&msg->data);
                xpipe_write(peer->mpipe, &msg, __sizeof_ptr);
            }

        }else if (strcmp(command, "exit") == 0) {
            __set_false(peer->runnig);
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        mclear(command, 256);
    }

    xmsger_free(&peer->msger);

    if (peer->mpipe){
        // xpipe_clear(peer->task_pipe);
        xpipe_break(peer->mpipe);
    }

    if (peer->task_pid){
        __xapi->thread_join(peer->task_pid);
    }

    xpeer_ctx_t *next, *ctx = peer->channels.next;
    while (ctx != &peer->channels){
        next = ctx->next;
        xpeer_ctx_free(ctx);
        ctx = next;
    }

    if (peer->mpipe){
        __xlogi("pipe readable = %u\n", xpipe_readable(peer->mpipe));
        xline_t *msg;
        while (__xpipe_read(peer->mpipe, &msg, __sizeof_ptr) == __sizeof_ptr){
            __xlogi("free msg\n");
            xl_free(&msg);
        }
        xpipe_free(&peer->mpipe);
    }

    avl_tree_clear(&peer->msgid_table, NULL);
    avl_tree_clear(&peer->uuid_table, NULL);
    // xtree_clear(peer->send_api, NULL);
    // xtree_free(&peer->send_api);
    xtree_clear(peer->recv_api, NULL);
    xtree_free(&peer->recv_api);

    // free(peer->xltp->handler);
    // free(peer->xltp);
    free(peer);

    xlog_recorder_close();

XClean:

    __xlogi("exit\n");

    return 0;
}
