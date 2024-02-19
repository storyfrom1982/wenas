#include "ex/ex.h"

#include <sys/struct/xmsger.h>

#include <stdio.h>


typedef struct client{
    int sock;
    struct __xipaddr ipaddr;
    struct xmsglistener listener;
    xmsger_ptr msger;
    xchannel_ptr channel;
    xtask_ptr task;
    xtask_ptr listen_task;
}*client_ptr;



static void malloc_debug_cb(const char *debug)
{
    __xloge("%s\n", debug);
}

static void listening(xtask_enter_ptr task_ctx)
{
    client_ptr client = (client_ptr)task_ctx->ctx;
    __xapi->udp_listen(client->sock);
    xmsger_wake(client->msger);
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *rsock, __xipaddr_ptr raddr, void *data, size_t size)
{
    // // send_number++;
    // // uint64_t randtime = __ex_clock() / 1000ULL;
    // // if ((send_number & 0xdf) == (randtime & 0xdf)){
    // //     // __logi("send_msg clock: %x number: %x lost number: %llu", randtime, send_number, ++lost_number);
    // //     return size;
    // // }
    client_ptr client = (client_ptr)rsock->ctx;
    long result = __xapi->udp_sendto(client->sock, raddr, data, size);
    return result;
}

static size_t recv_msg(struct xmsgsocket *rsock, __xipaddr_ptr addr, void *buf, size_t size)
{
    client_ptr client = (client_ptr)rsock->ctx;
    long result = __xapi->udp_recvfrom(client->sock, addr, buf, size);
    return result;
}

static void on_connection_to_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_connection_to_peer: 0x%x\n", channel);
    client_ptr client = (client_ptr)listener->ctx;
    client->channel = channel;

}

static void on_connection_from_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_connection_from_peer: 0x%x\n", channel);
}

static void on_disconnection(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> on_disconnection: 0x%x\n", channel);
}


static void parse_msg(xline_ptr msg, uint64_t len)
{
    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;
    while ((ptr = xline_next(maker)) != NULL)
    {
        __xlogd("xline ----------------- key: %s\n", maker->key);
        if (__typeis_int(ptr)){

            __xlogd("xline key: %s value: %ld\n", maker->key, __l2i(ptr));

        }else if (__typeis_float(ptr)){

            __xlogd("xline key: %s value: %lf\n", maker->key, __l2f(ptr));

        }else if (__typeis_str(ptr)){

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

static void process_message(xtask_enter_ptr task_ctx)
{
    __xlogd("process_message enter\n");
    xmsg_ptr msg = (xmsg_ptr)task_ctx->xline;
    parse_msg((xline_ptr)msg->data, msg->wpos);
    xchannel_free_msg(msg);
    __xlogd("process_message exit\n");
}

static void on_receive_message(xmsglistener_ptr listener, xchannel_ptr channel, xmsg_ptr msg)
{
    client_ptr client = (client_ptr)listener->ctx;
    struct xtask_enter enter;
    enter.func = process_message;
    enter.ctx = client;
    enter.index = channel;
    enter.xline = msg;
    xtask_push(client->task, enter);   
}

static void send_channel_msg(xtask_enter_ptr task_ctx)
{
    client_ptr client = (client_ptr)task_ctx->ctx;
    xchannel_ptr channel = (xchannel_ptr)task_ctx->index;
    xchannel_send_msg(channel, channel->msg);
}

static void on_sendable(xmsglistener_ptr listener, xchannel_ptr channel)
{
    client_ptr client = (client_ptr)listener->ctx;
    struct xtask_enter enter;
    enter.func = send_channel_msg;
    enter.ctx = client;
    enter.index = channel;
    xtask_push(client->task, enter);       
}

static void on_idle(xmsglistener_ptr listener, xchannel_ptr channel)
{
    client_ptr client = (client_ptr)listener->ctx;
    struct xtask_enter enter;
    enter.func = listening;
    enter.ctx = client;
    xtask_push(client->listen_task, enter);
}


static void build_msg(xmaker_ptr maker)
{
    xline_add_text(maker, "type", "udp");
    xline_add_text(maker, "api", "pull");
    uint64_t ipos = xmaker_hold_tree(maker, "int");
    xline_add_int(maker, "int8", 8);
    xline_add_int(maker, "int16", 16);
    xline_add_uint(maker, "uint32", 32);
    xline_add_uint(maker, "uint64", 64);
    uint64_t fpos = xmaker_hold_tree(maker, "float");
    xline_add_float(maker, "real32", 32.3232);
    xline_add_float(maker, "real64", 64.6464);
    xmaker_submit_tree(maker, fpos);
    xmaker_submit_tree(maker, ipos);
    xline_add_uint(maker, "uint64", 64);
    xline_add_float(maker, "real64", 64.6464);

    uint64_t lpos = xmaker_hold_list(maker, "list");
    for (int i = 0; i < 10; ++i){
        struct xline line = __n2l(i);
        xline_list_append(maker, &line);
    }
    xmaker_submit_list(maker, lpos);

    lpos = xmaker_hold_list(maker, "list-tree");
    for (int i = 0; i < 10; ++i){
        ipos = xmaker_list_hold_tree(maker);
        xline_add_text(maker, "key", "tree");
        xline_add_int(maker, "real32", i);
        xline_add_float(maker, "real64", 64.6464 * i);
        xmaker_list_submit_tree(maker, ipos);
    }
    xmaker_submit_list(maker, lpos);
    
}

static void find_msg(xline_ptr msg){

    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;

    uint64_t u64 = xline_find_uint(maker, "uint64");
    __xlogd("xline find uint = %lu\n", u64);

    double f64 = xline_find_float(maker, "real64");
    __xlogd("xline find real64 = %lf\n", f64);

    ptr = xline_find(maker, "type");
    __xlogd("xline find type = %s\n", __l2data(ptr));
    ptr = xline_find(maker, "api");
    __xlogd("xline find api = %s\n", __l2data(ptr));
}

int main(int argc, char *argv[])
{
    __xlog_open("./tmp/client/log", NULL);
    __xlogi("start client\n");

    client_ptr client = (client_ptr)calloc(1, sizeof(struct client));

    __xlogi("start client 1\n");

    // const char *host = "127.0.0.1";
    const char *host = "47.99.146.226";
    // const char *host = "18.138.128.58";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 9256;
    xmsgsocket_ptr msgsock = (xmsgsocket_ptr)malloc(sizeof(struct xmsgsocket));
    __xipaddr_ptr raddr = &client->ipaddr;
    xmsglistener_ptr listener = &client->listener;

    __xlogi("start client 2\n");

    client->ipaddr = __xapi->udp_make_ipaddr(host, port);

    __xlogi("start client 3\n");

    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onReceiveMessage = on_receive_message;
    listener->onSendable = on_sendable;
    listener->onIdle = on_idle;

    __xlogi("start client 4\n");

    client->sock = __xapi->udp_open();

    __xlogi("start client 5\n");

    msgsock->ctx = client;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    listener->ctx = client;


    client->task = xtask_create();
    client->listen_task = xtask_create();

    __xlogi("start client 2\n");
    client->msger = xmsger_create(msgsock, &client->listener);
    xmsger_run(client->msger);

    xmsger_connect(client->msger, raddr);

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
        xmaker_ptr maker = xmaker_create(2);
        build_msg(maker);
        xline_add_text(maker, "msg", str);
        // parse_msg((xline_ptr)maker->head, maker->wpos);
        // find_msg((xline_ptr)maker->head);
        xchannel_push_task(client->channel, maker->wpos);
        xmsger_send(client->msger, client->channel, maker->head, maker->wpos);
        xmaker_free(maker);
    }
;

    __xlogi("xmsger_disconnect\n");

    __set_false(client->msger->running);
    
    __xlogi("xmsger_free\n");
    xmsger_free(&client->msger);

    __xlogi("free msgsock\n");
    free(msgsock);

    __xapi->udp_clear_ipaddr(client->ipaddr);
    __xapi->udp_close(client->sock);
    
    __xlogi("free client\n");
    free(client);

    __xlogi("env_logger_stop\n");
    __xlog_close();

    __xlogi("exit\n");
    return 0;
}