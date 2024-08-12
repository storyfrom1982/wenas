#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xnet/xtable.h>

#include <stdio.h>
#include <stdlib.h>



typedef struct XTask {
    struct avl_node node;
    uint64_t task_id;
    void *msg;
    xchannel_ptr channel;
    struct xpeer *peer;
}*xTask_Ptr;

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
    xTask_Ptr task = xmsger_get_channel_ctx(channel);
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
    xTask_Ptr task = (xTask_Ptr)malloc(sizeof(struct XTask));
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

    xtask_enter_ptr task;
    xpeer_ptr server = (xpeer_ptr)ptr;

    while (xpipe_read(server->task_pipe, &task, __sizeof_ptr) == __sizeof_ptr)
    {

    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}


void login(xpeer_ptr peer)
{
    xline_t xl = xline_maker(1024);
    xline_add_word(&xl, "api", "login");
    xTask_Ptr task = xmsger_get_channel_ctx(peer->channel);
    task->task_id = peer->task_id++;
    avl_tree_add(&peer->task_tree, task);
    xmsger_send_message(peer->msger, peer->channel, xl.byte, xl.wpos);
}

void logout(xpeer_ptr peer)
{
    xline_t xl = xline_maker(1024);
    xline_add_word(&xl, "api", "logout");
    xmsger_send_message(peer->msger, peer->channel, xl.byte, xl.wpos);
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
	xTask_Ptr x = (xTask_Ptr)a;
	xTask_Ptr y = (xTask_Ptr)b;
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

    avl_tree_init(&server->task_tree, number_compare, sizeof(struct XTask), AVL_OFFSET(struct XTask, node));

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

    xTask_Ptr task = (xTask_Ptr)malloc(sizeof(struct XTask));
    task->peer = server;
    // xmsger_connect(server->msger, "47.99.146.226", 9256, task);
    xmsger_connect(server->msger, "192.168.43.173", 9256, task);
    // make_connect_task(server, "47.92.77.19", 9256);
    // make_connect_task(server, "120.78.155.213", 9256);

    char str[1024];
    char input[256];
    char command[256];
    char ip[256];
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
            login(server);
        } else if (strcmp(command, "logout") == 0) {
            logout(server);
        } else if (strcmp(command, "join") == 0) {
            __xlogi("输入IP地址: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                strcpy(ip, input);
            } else {
                __xlogi("读取IP地址失败\n");
                continue;
            }

            __xlogi("输入端口号: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                port = atoi(input);
            } else {
                printf("读取端口号失败\n");
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

            struct xline maker = xline_maker(1024);
            xline_add_word(&maker, "msg", "req");
            xline_add_word(&maker, "task", "join");
            xline_add_word(&maker, "ip", ip);
            xline_add_uint64(&maker, "port", port);
            xline_add_uint64(&maker, "key", key);
            xmsger_send_message(server->msger, server->channel, maker.head, maker.wpos);

            // handle_join(ip, port, key);
        }else if (strcmp(command, "turn") == 0) {
            __xlogi("输入键值: ");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                key = atoi(input);
            } else {
                __xlogi("读取键值失败\n");
                continue;
            }

            struct xline maker = xline_maker(1024);
            xline_add_word(&maker, "msg", "req");
            xline_add_word(&maker, "task", "turn");
            xline_add_uint64(&maker, "key", key);
            xline_add_word(&maker, "text", "Hello World");
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
