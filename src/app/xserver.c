#include "ex/ex.h"

#include <sys/struct/xtree.h>
#include <sys/struct/xmsger.h>
#include <sys/struct/xbuf.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct xpeer {

    uint16_t node_count;
// keepalive_table[0] 当前正在工作的服务器，第一个加入链表的节点
// keepalive_table[1] 当前正在工作的服务器的左节点，第二个加入链的节点
// keepalive_table[3] 当前正在工作的服务器的左左节点，第三个加入链表的节点
// keepalive_table[2] 当前正在工作的服务器的右节点，最后一个假如链表的节点
// keepalive_table[4] 当前正在工作的服务器的右右节点，倒数第二个加入链表的节点
    void *keepalive_table[5];
}xpeer_t;
// 客户端登录发生重定向时，服务器会把keepalive_table的前三个服务器地址返回给客户端


// 全局一致性哈希表，1024台服务器，每个服务器百万并发，可以撑起100亿在线用户
xpeer_t *hash_table[1024];

// 当前有多少台实际的服务器
uint16_t current_dht_count = 0;
// 当前网络中实际的服务器，current_dht_count 个实际的服务器平分了 1024 个虚拟服务器的工作量
xpeer_t *dht[1024];


// dht 中每个节点，都是一个由多个服务组成的备份链表，每个服务器都可以与左右相邻的服务器通信，进行数据同步和保活。并且能在发现有服务器掉线时，更新这个链表
// dht 中的第一个节点向备份链表中的自己右边的服务发送 PING，然后右节点向自己右边的（右右节点）节点发送 PING，然后右右节点向 dht 的第二个节点和自己右边的节点同时发送 PING，dht 中的节点，会从头到尾的循环进行保活更新
// 当 dht 的实际服务器达到 1024 台时，如果一次PING需要10毫秒，整个网络循环更新一次的时间大概是10秒左右


typedef struct xtasklist {
    uint64_t pos, len;
    void *msg;
    xserver_ptr server;
    void (*task_enter)(struct xtasklist *);
    xchannel_ptr channel;
    struct xtasklist *prev, *next;
}*xtasklist_ptr;

typedef struct xserver{
    int sock;
    __atom_bool runnig;
    struct __xipaddr addr;
    xmsger_ptr msger;
    xtasklist_ptr tasks;
    xpipe_ptr task_pipe;
    __xprocess_ptr task_pid, listen_pid;
    struct xmsgercb listener;
}*xserver_ptr;

static void build_msg(xmaker_ptr maker)
{
    xline_add_word(maker, "cmd", "REQ");
    xline_add_word(maker, "api", "PUT");
    uint64_t ipos = xline_hold_tree(maker, "int");
    xline_add_integer(maker, "int8", 8);
    xline_add_integer(maker, "int16", 16);
    xline_add_number(maker, "uint32", 32);
    xline_add_number(maker, "uint64", 64);
    uint64_t fpos = xline_hold_tree(maker, "float");
    xline_add_float(maker, "real32", 32.3232);
    xline_add_float(maker, "real64", 64.6464);
    xline_save_tree(maker, fpos);
    xline_save_tree(maker, ipos);
    xline_add_number(maker, "uint64", 64);
    xline_add_float(maker, "real64", 64.6464);

    uint64_t lpos = xline_hold_list(maker, "list");
    for (int i = 0; i < 10; ++i){
        struct xline line = __n2l(i);
        xline_list_append(maker, &line);
    }
    xline_save_list(maker, lpos);

    lpos = xline_hold_list(maker, "list-tree");
    for (int i = 0; i < 10; ++i){
        ipos = xline_list_hold_tree(maker);
        xline_add_word(maker, "key", "tree");
        xline_add_integer(maker, "real32", i);
        xline_add_float(maker, "real64", 64.6464 * i);
        xline_list_save_tree(maker, ipos);
    }
    xline_save_list(maker, lpos);
    
}

static void make_message_task(xserver_ptr server)
{
    if (server->tasks->channel){
        struct xmaker maker = xline_make(1024);
        build_msg(&maker);
        xline_add_word(&maker, "msg", "UPDATE");
        server->tasks->len++;
        xline_add_number(&maker, "count", server->tasks->len);
        xmsger_send_message(server->msger, server->tasks->channel, maker.head, maker.wpos);
    }
}

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xtasklist_ptr task = xchannel_user_ctx(channel);
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

static void msg_from_peer(xtasklist_ptr task)
{
    void *msg = task->msg;
    xmaker_t maker = xline_parse((xline_ptr)msg);
    const char *cmd = xline_find_word(&maker, "cmd");
    if (mcompare(cmd, "REQ", 3) == 0){
        cmd = xline_find_word(&maker, "api");
        if (mcompare(cmd, "PUT", 3) == 0){
            xmaker_t builder = xline_make(1024);
            xline_add_word(&builder, "cmd", "RES");
            xline_add_integer(&builder, "code", 0);
            // uint64_t ipos = xline_hold_tree(&builder, "REQ");
            xline_add_tree(&builder, "REQ", &maker);
            // xline_save_tree(&builder, ipos);
            xmsger_send_message(task->server->msger, task->channel, builder.head, builder.wpos);
        }
    }else if(mcompare(cmd, "RES", 3) == 0){
        task->server->tasks->pos++;
        xline_printf(msg);
        __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>--------------------------------------------------------------> pos: %lu len: %lu\n", server->tasks->pos, server->tasks->len);
        // make_message_task(server);
    }
    free(msg);
}


static void on_message_from_peer(xmsgercb_ptr listener, xchannel_ptr channel, void *msg, size_t len)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xtasklist_ptr task = xchannel_user_ctx(channel);

    task->msg = msg;
    task->task_enter = msg_from_peer;
    __xbreak(xpipe_write(server->task_pipe, &task, __sizeof_ptr) != __sizeof_ptr);

    // xmaker_t maker = xline_parse((xline_ptr)msg);
    // const char *cmd = xline_find_word(&maker, "cmd");
    // if (mcompare(cmd, "REQ", 3) == 0){
    //     cmd = xline_find_word(&maker, "api");
    //     if (mcompare(cmd, "PUT", 3) == 0){
    //         xmaker_t builder = xline_make(1024);
    //         xline_add_word(&builder, "cmd", "RES");
    //         xline_add_integer(&builder, "code", 0);
    //         // uint64_t ipos = xline_hold_tree(&builder, "REQ");
    //         xline_add_tree(&builder, "REQ", &maker);
    //         // xline_save_tree(&builder, ipos);
    //         xmsger_send_message(server->msger, channel, builder.head, builder.wpos);
    //     }
    // }else if(mcompare(cmd, "RES", 3) == 0){
    //     server->tasks->pos++;
    //     xline_printf(msg);
    //     __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>--------------------------------------------------------------> pos: %lu len: %lu\n", server->tasks->pos, server->tasks->len);
    //     // make_message_task(server);
    // }
    // free(msg);

Clean:
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void find_msg(xline_ptr msg){

    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;

    uint64_t u64 = xline_find_number(maker, "uint64");
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

static void make_connect_task(xserver_ptr server, const char *ip, uint16_t port)
{
    xmsger_connect(server->msger, ip, port);
}

static void make_disconnect_task(xserver_ptr server)
{
    xmsger_disconnect(server->msger, server->tasks->channel);
}

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    xmsger_set_channel_ctx(channel, server->tasks);
    server->tasks->channel = channel;
    server->tasks->server = server;
    for (int i = 0; i < 10; ++i){
        make_message_task(server);
    }
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xserver_ptr server = (xserver_ptr)listener->ctx;
    server->tasks->server = server;
    xmsger_set_channel_ctx(channel, server->tasks);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}


static void* listen_loop(void *ptr)
{
    xserver_ptr server = (xserver_ptr)ptr;
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

    xtasklist_ptr task;
    xserver_ptr server = (xserver_ptr)ptr;

    while (xpipe_read(server->task_pipe, &task, __sizeof_ptr) == __sizeof_ptr)
    {
        task->task_enter(task);
    }

    __xlogd("task_loop exit\n");

Clean:

    return NULL;
}


static xserver_ptr g_server = NULL;

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

int main(int argc, char *argv[])
{
    char *host = NULL;
    uint16_t port = 0;

    xlog_recorder_open("./tmp/xserver/log", NULL);

    xserver_ptr server = (xserver_ptr)calloc(1, sizeof(struct xserver));
    __xlogi("server: 0x%X\n", server);

    __set_true(server->runnig);
    g_server = server;
    sigint_setup(__sigint_handler);

    server->tasks = (xtasklist_ptr)calloc(1, sizeof(struct xtasklist));

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
    __xbreak(!__xapi->udp_make_ipaddr(NULL, 9256, &server->addr));
    __xbreak(__xapi->udp_bind(server->sock, &server->addr) == -1);
    __xbreak(!__xapi->udp_make_ipaddr("127.0.0.1", 9256, &server->addr));

    server->task_pipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xbreak(server->task_pipe == NULL);

    server->task_pid = __xapi->process_create(task_loop, server);
    __xbreak(server->task_pid == NULL);
    
    server->msger = xmsger_create(&server->listener, server->sock);

    server->listen_pid = __xapi->process_create(listen_loop, server);
    __xbreak(server->listen_pid == NULL);

    if (host){
        make_connect_task(server, host, port);
    }

    char str[1024];
    while (1)
    {
        __xlogi("Enter a value :\n");
        fgets(str, 1000, stdin);
        size_t len = slength(str);
        if (len == 2){
            if (str[0] == 'c'){
                make_connect_task(server, host, port);
            }else if (str[0] == 'd'){
                make_disconnect_task(server);
            }else if (str[0] == 's'){
                make_message_task(server);   
            }else if (str[0] == 'q'){
                break;
            }
        }else {
            make_message_task(server);
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
