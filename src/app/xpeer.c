#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>
#include <xlib/xxhash.h>
#include <xlib/xsha256.h>

#include <stdio.h>
#include <stdlib.h>


typedef struct xpeer_task {
    uint8_t tid;
    void (*req)(struct xpeer_task*);
    void (*enter)(struct xpeer_task*);
    uint64_t len, pos;
    struct xpeer_ctx *peerctx;
    struct xpeer_task *prev, *next;
}xpeer_task_t;

typedef struct xpeer_ctx {
    struct avl_node node;
    uint64_t task_id;
    void *msg;
    xchannel_ptr channel;
    struct xpeer *peer;
    xlkv_t msgparser;
    xpeer_task_t head;
}*xpeer_ctx_ptr;

typedef struct xpeer{
    int sock;
    __atom_bool runnig;
    uint64_t task_id;
    struct __xipaddr addr;
    xchannel_ptr channel;
    xmsger_ptr msger;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid, listen_pid;
    struct xmsgercb listener;
    struct avl_tree task_tree;
    uint16_t task_count;
    uint8_t task_index;
    xpeer_task_t *task_table[256];
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
    xpeer_ctx_ptr task = xmsger_get_channel_ctx(channel);
    if (task){

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
    xpeer_ctx_ptr task = xmsger_get_channel_ctx(channel);
    task->msg = msg;
    xpipe_write(task->peer->task_pipe, &task, __sizeof_ptr);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void make_disconnect_task(xpeer_ptr server)
{
    xmsger_disconnect(server->msger, server->channel);
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xpeer_ctx_ptr task = xmsger_get_channel_ctx(channel);
    task->channel = channel;
    if (task->peer == server){
        server->channel = channel;
    }else {
        task->peer = server;
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ctx_ptr task = (xpeer_ctx_ptr)malloc(sizeof(struct xpeer_ctx));
    task->peer = (xpeer_ptr)listener->ctx;
    task->channel = channel;
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

static void* task_loop(void *ptr)
{
    __xlogd("task_loop enter\n");

    xpeer_ctx_ptr ctx;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &ctx, __sizeof_ptr) == __sizeof_ptr)
    {
        xl_printf(ctx->msg);
        ctx->msgparser = xl_parser(ctx->msg);
        char *api = xl_find_word(&ctx->msgparser, "api");
        if (mcompare(api, "res", slength("res")) == 0){
            uint8_t tid = xl_find_number(&ctx->msgparser, "tid");
            __xlogd("task_loop tid=%u\n", tid);
            xpeer_task_t *task = server->task_table[tid];
            __xlogd("task_loop task=%p\n", task);
            if (task && task->enter){
                __xlogd("task_loop res=%p\n", task->enter);
                task->enter(task);
            }
        }
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}

static void res_login(xpeer_task_t *task)
{
    xl_printf(task->peerctx->msg);
}

static void req_login(xpeer_task_t *task)
{
    __xlogd("req_login ----------------------- enter\n");
    xpeer_ptr peer = task->peerctx->peer;
    char text[1024] = {0};
    uint8_t sha256[32];
    SHA256_CTX shactx;

    uint64_t millisecond = __xapi->time();
    __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);

    millisecond = __xapi->clock();
    __xapi->strftime(text, 1024, millisecond / NANO_SECONDS);

    int n = snprintf(text, 1024, "%lu%lu%s", __xapi->time(), __xapi->clock(), "Hello");

    sha256_init(&shactx);
    sha256_update(&shactx, text, n);
    sha256_finish(&shactx, sha256);

    while (peer->task_table[peer->task_index] != NULL){
        peer->task_index++;
    }
    peer->task_table[peer->task_index] = task;
    task->tid = peer->task_index;
    peer->task_index++;
    task->enter = res_login;

    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "login");
    xl_add_number(&xl, "tid", task->tid);
    xl_add_number(&xl, "key", xxhash64(sha256, 32, 0));
    xl_add_number(&xl, "len", 32);
    xl_add_bin(&xl, "uuid", sha256, 32);

    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
    __xlogd("req_login ----------------------- exit\n");
}

static void res_logout(xpeer_task_t *task)
{
    xl_printf(task->peerctx->msg);
}

static void req_logout(xpeer_task_t *task)
{
    xpeer_ptr peer = task->peerctx->peer;
    while (peer->task_table[peer->task_index] != NULL){
        peer->task_index++;
    }
    peer->task_table[peer->task_index] = task;
    task->tid = peer->task_index;
    peer->task_index++;
    task->enter = res_logout;

    xlkv_t xl = xl_maker(1024);
    xl_add_word(&xl, "api", "logout");
    xl_add_number(&xl, "tid", task->tid);
    xmsger_send_message(peer->msger, peer->channel, xl.head, xl.wpos);
}

void create_task(xpeer_ptr peer, void(*api)(xpeer_task_t*))
{
    xpeer_ctx_ptr ctx = xmsger_get_channel_ctx(peer->channel);
    xpeer_task_t *task = (xpeer_task_t*)malloc(sizeof(xpeer_task_t));
    task->peerctx = ctx;
    if (peer->task_count == 256){
        task->req = api;
        task->next = ctx->head.next;
        task->prev = &ctx->head;
        task->next->prev = task;
        task->prev->next = task;
    }else {
        peer->task_count++;
        api(task);
    }
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

static inline int number_compare(const void *a, const void *b)
{
	xpeer_ctx_ptr x = (xpeer_ctx_ptr)a;
	xpeer_ctx_ptr y = (xpeer_ctx_ptr)b;
	return x->task_id - y->task_id;
}

#include <arpa/inet.h>
int main(int argc, char *argv[])
{
    char *host = NULL;
    uint16_t port = 0;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_ptr server = (xpeer_ptr)calloc(1, sizeof(struct xpeer));
    __xlogi("server: 0x%X\n", server);

    avl_tree_init(&server->task_tree, number_compare, sizeof(struct xpeer_ctx), AVL_OFFSET(struct xpeer_ctx, node));

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

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
    listener->on_msg_from_peer = on_message_from_peer;
    listener->on_msg_to_peer = on_message_to_peer;

    server->sock = __xapi->udp_open();
    __xbreak(server->sock < 0);
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 0, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    struct sockaddr_in server_addr; 
    socklen_t addr_len = sizeof(server_addr);
    __xbreak(getsockname(server->sock, (struct sockaddr*)&server_addr, &addr_len) == -1);
    __xlogd("自动分配的端口号: %d\n", ntohs(server_addr.sin_port));
    __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", ntohs(server_addr.sin_port), &server->addr));

    server->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener, server->sock);

    server->listen_pid = __xapi->process_create(listen_loop, server);
    __xbreak(server->listen_pid == NULL);

    xpeer_ctx_ptr task = (xpeer_ctx_ptr)malloc(sizeof(struct xpeer_ctx));
    task->peer = server;
    // xmsger_connect(server->msger, "47.99.146.226", 9256, task);
    xmsger_connect(server->msger, "192.168.43.173", 9256, task);
    // make_connect_task(server, "47.92.77.19", 9256);
    // make_connect_task(server, "120.78.155.213", 9256);
    // make_connect_task(server, "18.138.128.58", 9256);

    char str[1024];
    char input[256];
    char command[256];
    char ip[256] = {0};
    uint32_t key;
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
            __xlogd("command login ----------------------- enter\n");
            create_task(server, req_login);
            __xlogd("command login ----------------------- exit\n");
        } else if (strcmp(command, "logout") == 0) {
            create_task(server, req_logout);
        } else if (strcmp(command, "join") == 0) {
            __xlogi("输入IP地址: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                strcpy(ip, input);
            } else {
                __xlogi("读取IP地址失败\n");
                continue;
            }

            __xlogi("输入键值: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                key = atoi(input);
            } else {
                __xlogi("读取键值失败\n");
                continue;
            }

            xpeer_task_t *task = (xpeer_task_t *)malloc(sizeof(xpeer_task_t));
            server->task_table[0] = task;
            task->enter = res_logout;
            task->peerctx = xmsger_get_channel_ctx(server->channel);

            struct xlkv maker = xl_maker(1024);
            xl_add_word(&maker, "api", "chord_join");
            xl_add_number(&maker, "tid", 0);
            xl_add_word(&maker, "ip", ip);
            xl_add_number(&maker, "port", 9256);
            xl_add_number(&maker, "key", key);
            xmsger_send_message(server->msger, server->channel, maker.head, maker.wpos);

            // handle_join(ip, port, key);
        }else if (strcmp(command, "messaging") == 0) {
            __xlogi("输入键值: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                key = atoi(input);
            } else {
                __xlogi("读取键值失败\n");
                continue;
            }

            struct xlkv maker = xl_maker(1024);
            xl_add_word(&maker, "api", "messaging");
            xl_add_number(&maker, "key", key);
            xl_add_word(&maker, "text", "Hello World");
            xmsger_send_message(server->msger, server->channel, maker.head, maker.wpos);

            // handle_join(ip, port, key);
        } else if (strcmp(command, "exit") == 0) {
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }
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

    avl_tree_clear(&server->task_tree, NULL);

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
