#include "ex/ex.h"

#include <sys/struct/xtree.h>
#include <sys/struct/xmsger.h>
#include <sys/struct/xbuf.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct xtasklist {
    uint64_t pos, len;
    xchannel_ptr channel;
    struct xtasklist *prev, *next;
}*xtasklist_ptr;

typedef struct xfollower{
    xmsger_ptr msger;
    xtasklist_ptr tasks;
    __xprocess_ptr pid;
    xpipe_ptr taskpipe;
    struct xmsgercb listener;
}*xfollower_ptr;

static void on_connection_to_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xfollower_ptr server = (xfollower_ptr)listener->ctx;
    server->tasks = xchannel_context(channel);
    server->tasks->channel = channel;
    __xlogd("on_connection_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_connection_from_peer(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xfollower_ptr server = (xfollower_ptr)listener->ctx;
    xtasklist_ptr task = xchannel_context(channel);
    __xlogd("on_connection_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_channel_break(xmsgercb_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_channel_break >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xfollower_ptr server = (xfollower_ptr)listener->ctx;
    xtasklist_ptr task = xchannel_context(channel);
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
    xfollower_ptr server = (xfollower_ptr)listener->ctx;
    xmaker_t maker = xline_parse((xline_ptr)msg);
    const char *cmd = xline_find_word(&maker, "cmd");
    if (mcompare(cmd, "REQ", 3) == 0){
        cmd = xline_find_word(&maker, "api");
        if (mcompare(cmd, "PUT", 3) == 0){
            xmaker_t builder = xmaker_build(1024);
            xline_add_word(&builder, "cmd", "RES");
            xline_add_int(&builder, "code", 0);
            // uint64_t ipos = xmaker_hold_tree(&builder, "REQ");
            xline_add_map(&builder, "REQ", &maker);
            // xmaker_save_tree(&builder, ipos);
            xmsger_send_message(server->msger, channel, builder.head, builder.wpos);
        }
    }else if(mcompare(cmd, "RES", 3) == 0){
        server->tasks->pos++;
        xline_printf(msg);
        __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>--------------------------------------------------------------> pos: %lu len: %lu\n", server->tasks->pos, server->tasks->len);
    }
    free(msg);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void build_msg(xmaker_ptr maker)
{
    xline_add_word(maker, "cmd", "REQ");
    xline_add_word(maker, "api", "PUT");
    uint64_t ipos = xmaker_hold_tree(maker, "int");
    xline_add_int(maker, "int8", 8);
    xline_add_int(maker, "int16", 16);
    xline_add_uint(maker, "uint32", 32);
    xline_add_uint(maker, "uint64", 64);
    uint64_t fpos = xmaker_hold_tree(maker, "float");
    xline_add_float(maker, "real32", 32.3232);
    xline_add_float(maker, "real64", 64.6464);
    xmaker_save_tree(maker, fpos);
    xmaker_save_tree(maker, ipos);
    xline_add_uint(maker, "uint64", 64);
    xline_add_float(maker, "real64", 64.6464);

    uint64_t lpos = xmaker_hold_list(maker, "list");
    for (int i = 0; i < 10; ++i){
        struct xline line = __n2l(i);
        xline_list_append(maker, &line);
    }
    xmaker_save_list(maker, lpos);

    lpos = xmaker_hold_list(maker, "list-tree");
    for (int i = 0; i < 10; ++i){
        ipos = xmaker_list_hold_tree(maker);
        xline_add_word(maker, "key", "tree");
        xline_add_int(maker, "real32", i);
        xline_add_float(maker, "real64", 64.6464 * i);
        xmaker_list_save_tree(maker, ipos);
    }
    xmaker_save_list(maker, lpos);
    
}

static void find_msg(xline_ptr msg){

    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;

    uint64_t u64 = xline_find_uint(maker, "uint64");
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

static void make_connect_task(xfollower_ptr server, const char *ip, uint16_t port)
{
    xmsger_connect(server->msger, server->tasks, ip, port);
}

static void make_disconnect_task(xfollower_ptr server)
{
    xmsger_disconnect(server->msger, server->tasks->channel);
}

static void make_message_task(xfollower_ptr server)
{
    if (server->tasks->channel){
        struct xmaker maker = xmaker_build(1024);
        build_msg(&maker);
        xline_add_word(&maker, "msg", "UPDATE");
        server->tasks->len++;
        xline_add_uint(&maker, "count", server->tasks->len);
        xmsger_send_message(server->msger, server->tasks->channel, maker.head, maker.wpos);
    }
}

int main(int argc, char *argv[])
{
    char *host = NULL;
    uint16_t port = 0;

    xlog_recorder_open("./tmp/follow/log", NULL);

    xfollower_ptr server = (xfollower_ptr)calloc(1, sizeof(struct xfollower));
    __xlogi("server: 0x%X\n", server);

    server->tasks = (xtasklist_ptr)malloc(sizeof(struct xtasklist));

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
    
    server->msger = xmsger_create(&server->listener);

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
    
    xmsger_free(&server->msger);

    free(server->tasks);
    
    free(server);

    if (host){
        free(host);
    }

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}