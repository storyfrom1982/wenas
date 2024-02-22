#include "ex/ex.h"

#include <sys/struct/xmsger.h>

#include <stdio.h>


typedef struct client{
    struct xmsglistener listener;
    xmsger_ptr msger;
    xchannel_ptr channel;
}*client_ptr;


// static uint64_t send_number = 0, lost_number = 0;
// static size_t send_msg(struct xmsgsocket *rsock, __xipaddr_ptr raddr, void *data, size_t size)
// {
//     // // send_number++;
//     // // uint64_t randtime = __ex_clock() / 1000ULL;
//     // // if ((send_number & 0xdf) == (randtime & 0xdf)){
//     // //     // __logi("send_msg clock: %x number: %x lost number: %llu", randtime, send_number, ++lost_number);
//     // //     return size;
//     // // }
//     client_ptr client = (client_ptr)rsock->ctx;
//     long result = __xapi->udp_sendto(client->sock, raddr, data, size);
//     return result;
// }

// static size_t recv_msg(struct xmsgsocket *rsock, __xipaddr_ptr addr, void *buf, size_t size)
// {
//     client_ptr client = (client_ptr)rsock->ctx;
//     long result = __xapi->udp_recvfrom(client->sock, addr, buf, size);
//     return result;
// }

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

static void on_message_to_peer(xmsglistener_ptr listener, xchannel_ptr channel, void* msg)
{
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    
    __xlogd("on_message_to_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
}

static void on_message_from_peer(xmsglistener_ptr listener, xchannel_ptr channel, xmsg_ptr msg)
{
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> enter\n");
    client_ptr client = (client_ptr)listener->ctx;
    parse_msg((xline_ptr)msg->data, msg->wpos);
    // xchannel_free_msg(msg);
    __xlogd("on_message_from_peer >>>>>>>>>>>>>>>>>>>>---------------> exit\n");
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
    xmsglistener_ptr listener = &client->listener;

    __xlogi("start client 2\n");

    // __xapi->udp_make_ipaddr(host, port, &client->ipaddr);

    __xlogi("start client 3\n");

    listener->ctx = client;
    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onMessageFromPeer = on_message_from_peer;
    listener->onMessageToPeer = on_message_to_peer;
    
    client->msger = xmsger_create(&client->listener);
    // xmsger_run(client->msger);

    xmsger_connect(client->msger, host, port);

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
        xmsger_send(client->msger, client->channel, maker->head, maker->wpos);
        // xmaker_free(maker);
    }
;

    __xlogi("xmsger_disconnect\n");

    __set_false(client->msger->running);
    
    __xlogi("xmsger_free\n");
    xmsger_free(&client->msger);
    
    __xlogi("free client\n");
    free(client);

    __xlogi("env_logger_stop\n");
    __xlog_close();

    __xlogi("exit\n");
    return 0;
}