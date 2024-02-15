#include "ex/ex.h"

#include <sys/struct/xmsger.h>

#include <stdio.h>


typedef struct client{
    int sock;
    struct __xipaddr ipaddr;
    struct xmsglistener listener;
    xmsger_ptr msger;
    xchannel_ptr channel;
}*client_ptr;



static void malloc_debug_cb(const char *debug)
{
    __xloge("%s\n", debug);
}

static void listening(struct xmsgsocket *rsock)
{
    client_ptr client = (client_ptr)rsock->ctx;
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

static void on_receive_message(xmsglistener_ptr listener, xchannel_ptr channel, xmsg_ptr msg)
{

}

static void on_sendable(xmsglistener_ptr listener, xchannel_ptr channel)
{

}

static void on_idle(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogi(">>>>---------------> channel timeout: 0x%x\n", channel);
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
        struct xmaker maker;
        xline_maker_setup(&maker, NULL, 1024);
        xline_add_text(&maker, "msg", str, slength(str));
        xchannel_push_task(client->channel, maker.wpos + 9);
        xmsger_send(client->msger, client->channel, maker.addr, maker.wpos + 9);
        xline_maker_clear(&maker);
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