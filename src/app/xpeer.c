#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct XTask {
    uint64_t pos, len;
    xchannel_ptr channel;
    struct XTask *prev, *next;
}*xTask_Ptr;

typedef struct xpeer{
    int sock;
    __atom_bool runnig;
    struct __xipaddr addr;
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

static void build_msg(xline_ptr maker)
{
    xline_add_word(maker, "cmd", "REQ");
    xline_add_word(maker, "api", "PUT");
    uint64_t ipos = xline_hold_tree(maker, "int");
    xline_add_integer(maker, "int8", 8);
    xline_add_integer(maker, "int16", 16);
    xline_add_unsigned(maker, "uint32", 32);
    xline_add_unsigned(maker, "uint64", 64);
    uint64_t fpos = xline_hold_tree(maker, "float");
    xline_add_real(maker, "real32", 32.3232);
    xline_add_real(maker, "real64", 64.6464);
    xline_save_tree(maker, fpos);
    xline_save_tree(maker, ipos);
    xline_add_unsigned(maker, "uint64", 64);
    xline_add_real(maker, "real64", 64.6464);

    uint64_t lpos = xline_hold_list(maker, "list");
    for (int i = 0; i < 10; ++i){
        struct xbyte line = __n2l(i);
        xline_list_append(maker, &line);
    }
    xline_save_list(maker, lpos);

    lpos = xline_hold_list(maker, "list-tree");
    for (int i = 0; i < 10; ++i){
        ipos = xline_list_hold_tree(maker);
        xline_add_word(maker, "key", "tree");
        xline_add_integer(maker, "real32", i);
        xline_add_real(maker, "real64", 64.6464 * i);
        xline_list_save_tree(maker, ipos);
    }
    xline_save_list(maker, lpos);
    
}

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
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    xline_t maker = xline_parser((xbyte_ptr)msg);
    const char *cmd = xline_find_word(&maker, "msg");
    if (mcompare(cmd, "req", 3) == 0){
        xline_printf(msg);
    }else if(mcompare(cmd, "res", 3) == 0){
        server->tasks->pos++;
        xline_printf(msg);
    }
    free(msg);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void find_msg(xbyte_ptr msg){

    struct xline m = xline_parser(msg);
    xline_ptr maker = &m;
    xbyte_ptr ptr;

    uint64_t u64 = xline_find_unsigned(maker, "uint64");
    __xlogd("xline find uint = %lu\n", u64);

    double f64 = xline_find_float(maker, "real64");
    __xlogd("xline find real64 = %lf\n", f64);

    const char *text = xline_find_word(maker, "cmd");
    __xlogd("xline find type = %s\n", text);
    text = xline_find_word(maker, "api");
    __xlogd("xline find api = %s\n", text);
}

static void make_tasklist()
{

}

static void make_connect_task(xpeer_ptr server, const char *ip, uint16_t port)
{
    xmsger_connect(server->msger, ip, port, server->tasks);
}

static void make_disconnect_task(xpeer_ptr server)
{
    xmsger_disconnect(server->msger, server->tasks->channel);
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xpeer_ptr server = (xpeer_ptr)listener->ctx;
    server->tasks = xmsger_get_channel_ctx(channel);
    server->tasks->channel = channel;
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    // xpeer_ptr server = (xpeer_ptr)listener->ctx;
    // xmsger_set_channel_ctx(channel, server->tasks);
    // server->tasks->channel = channel;
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


static void make_message_task(xpeer_ptr server)
{
    if (server->tasks->channel){
        struct xline maker = xline_maker(1024);
        build_msg(&maker);
        xline_add_word(&maker, "msg", "UPDATE");
        server->tasks->len++;
        xline_add_unsigned(&maker, "count", server->tasks->len);
        xmsger_send_message(server->msger, server->tasks->channel, maker.head, maker.wpos);
    }
}

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

    server->tasks = (xTask_Ptr)calloc(1, sizeof(struct XTask));

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

    make_connect_task(server, "47.99.146.226", 9256);
    // make_connect_task(server, "192.168.43.173", 9256);
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

        if (strcmp(command, "help") == 0) {
            // print_help();
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
            xline_add_unsigned(&maker, "port", port);
            xline_add_unsigned(&maker, "key", key);
            xmsger_send_message(server->msger, server->tasks->channel, maker.head, maker.wpos);

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
            xline_add_unsigned(&maker, "key", key);
            xline_add_word(&maker, "text", "Hello World");
            xmsger_send_message(server->msger, server->tasks->channel, maker.head, maker.wpos);

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

    free(server->tasks);

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
