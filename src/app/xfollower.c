#include "ex/ex.h"

#include <sys/struct/xtree.h>
#include <sys/struct/xmsger.h>
#include <sys/struct/xbuf.h>

#include <stdio.h>
#include <stdlib.h>

typedef struct resource {
    uint64_t len;
    void *data;
}resource_ptr;

typedef struct task {
    uint64_t pos, len;
    xchannel_ptr channel;
    resource_ptr resource;
    struct task *prev, *next;
}*task_ptr;

typedef struct xfollower{
    xmsger_ptr msger;
    __xprocess_ptr pid;
    xpipe_ptr taskpipe;
    xtree channels, resources;
    struct xmsglistener listener;
}*xfollower_ptr;

static void on_connection_to_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_connection_to_peer: 0x%x\n", channel);
    xfollower_ptr follow = (xfollower_ptr)listener->ctx;
    task_ptr task = (task_ptr)malloc(sizeof(struct task));
    task->prev = task;
    task->next = task;
    task->channel = channel;    
    xtree_save(follow->channels, channel, __sizeof_ptr, task);
}

static void on_connection_from_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_connection_from_peer: 0x%x\n", channel);
    xfollower_ptr follow = (xfollower_ptr)listener->ctx;
    task_ptr task = (task_ptr)malloc(sizeof(struct task));
    task->prev = task;
    task->next = task;
    task->channel = channel;
    xtree_save(follow->channels, channel, __sizeof_ptr, task);    
}

static void on_channel_timeout(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_channel_timeout: 0x%x\n", channel);
    xfollower_ptr follow = (xfollower_ptr)listener->ctx;
    task_ptr task = xtree_take(follow->channels, channel, __sizeof_ptr);
    free(task);
}

static void on_disconnection(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_disconnection: 0x%x\n", channel);
    xfollower_ptr follow = (xfollower_ptr)listener->ctx;
    task_ptr task = xtree_take(follow->channels, channel, __sizeof_ptr);
    free(task);    
}


static void parse_msg(xline_ptr msg, uint64_t len)
{
    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;
    while ((ptr = xline_next(maker)) != NULL)
    {
        // __xlogd("xline ----------------- key: %s\n", maker->key);
        if (__typeis_int(ptr)){

            __xlogd("xline key: %s value: %ld\n", maker->key, __l2i(ptr));

        }else if (__typeis_float(ptr)){

            __xlogd("xline key: %s value: %lf\n", maker->key, __l2f(ptr));

        }else if (__typeis_word(ptr)){

            __xlogd("xline text key: %s value: %s\n", maker->key, __l2data(ptr));

        }else if (__typeis_tree(ptr)){

            parse_msg(ptr, 0);

        }else if (__typeis_list(ptr)){

            __xlogd("xline list key: %s\n", maker->key);
            struct xmaker list = xline_parse(ptr);

            while ((ptr = xline_list_next(&list)) != NULL)
            {
                if (__typeis_int(ptr)){

                    __xlogd("xline list value: %d\n", __l2i(ptr));

                }else if (__typeis_tree(ptr)){
                    
                    parse_msg(ptr, 0);
                }
            }

        }else {
            __xlogd("xline type error\n");
        }
    }
}

static void on_message_to_peer(xmsglistener_ptr listener, xchannel_ptr channel, void* msg)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    free(msg);
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsglistener_ptr listener, xchannel_ptr channel, void *msg, size_t len)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    xfollower_ptr follow = (xfollower_ptr)listener->ctx;
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
            xmsger_send_message(follow->msger, channel, builder.head, builder.wpos);
        }
    }else if(mcompare(cmd, "RES", 3) == 0){
        parse_msg((xline_ptr)msg, len);
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

int main(int argc, char *argv[])
{
    char *host = NULL;
    uint16_t port = 0;

    xlog_recorder_open("./tmp/follow/log", NULL);

    xfollower_ptr follow = (xfollower_ptr)calloc(1, sizeof(struct xfollower));

    follow->channels = xtree_create();
    __xbreak(follow->channels == NULL);
    follow->resources = xtree_create();
    __xbreak(follow->resources == NULL);

    if (argc == 3){
        host = strdup(argv[1]);
        port = atoi(argv[2]);
    }

    // const char *host = "127.0.0.1";
    // const char *host = "47.99.146.226";
    // const char *host = "18.138.128.58";
    // uint16_t port = atoi(argv[1]);
    // uint16_t port = 9256;

    xmsglistener_ptr listener = &follow->listener;

    listener->ctx = follow;
    listener->onChannelToPeer = on_connection_to_peer;
    listener->onChannelFromPeer = on_connection_from_peer;
    listener->onChannelBreak = on_disconnection;
    listener->onChannelTimeout = on_channel_timeout;
    listener->onMessageFromPeer = on_message_from_peer;
    listener->onMessageToPeer = on_message_to_peer;
    
    follow->msger = xmsger_create(&follow->listener);

    if (host && port){
        xmsger_connect(follow->msger, host, port);
    }

    char str[1024];
    
    while (1)
    {
        __xlogi("Enter a value :\n");
        fgets(str, 1000, stdin);
        size_t len = slength(str);
        if (len == 2 && str[0] == 'q'){
            break;
        }
        str[len-1] = '\0';
        struct xmaker maker = xmaker_build(2);
        if (maker.head == NULL){
            break;
        }
        build_msg(&maker);
        xline_add_word(&maker, "msg", str);
        // parse_msg((xline_ptr)maker->head, maker->wpos);
        // find_msg((xline_ptr)maker.head);
        task_ptr task = (task_ptr)tree_min(follow->channels);
        xmsger_send_message(follow->msger, task->channel, maker.head, maker.wpos);
    }

    xtree_free(&follow->channels);

    xtree_free(&follow->resources);
    
    xmsger_free(&follow->msger);
    
    free(follow);

    if (host){
        free(host);
    }

    xlog_recorder_close();

Clean:

    __xlogi("exit\n");

    return 0;
}