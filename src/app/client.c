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


static void build_msg(xmaker_ptr maker)
{
    xline_add_text(maker, "type", "udp", slength("udp"));
    xline_add_text(maker, "api", "pull", slength("pull"));
    uint64_t ipos = xline_maker_hold(maker, "int");
    xline_add_int8(maker, "int8", 8);
    xline_add_int16(maker, "int16", 16);
    xline_add_uint32(maker, "uint32", 32);
    xline_add_uint64(maker, "uint64", 64);
    uint64_t fpos = xline_maker_hold(maker, "float");
    xline_add_real32(maker, "real32", 32.3232);
    xline_add_real64(maker, "real64", 64.6464);
    xline_maker_update(maker, fpos);
    xline_maker_update(maker, ipos);
}

static void parse_msg(xline_ptr msg, uint64_t len)
{
    struct xmaker m = xline_parse(msg);
    xmaker_ptr maker = &m;
    xline_ptr ptr;
    while ((ptr = xline_next(maker)) != NULL)
    {
        __xlogd("xline ----------------- key: %s\n", maker->key->byte);
        if (__xline_typeif_number(ptr)){

            if (__xline_num_typeif_integer(ptr)){

                if (__xline_number_64bit(ptr)){
                    __xlogd("xline key: %s value: %ld\n", maker->key->byte, __b2n16(ptr));
                }else {
                    __xlogd("xline key: %s value: %d\n", maker->key->byte, __b2n16(ptr));
                }
                
            }else if (__xline_num_typeif_natural(ptr)){

                if (__xline_number_64bit(ptr)){
                    __xlogd("xline key: %s value: %lu\n", maker->key->byte, __b2n16(ptr));
                }else {
                    __xlogd("xline key: %s value: %u\n", maker->key->byte, __b2n16(ptr));
                }

            }else if (__xline_num_typeif_real(ptr)){

                if (__xline_number_64bit(ptr)){
                    __xlogd("xline key: %s value: %lf\n", maker->key->byte, __b2f64(ptr));
                }else {
                    __xlogd("xline key: %s value: %f\n", maker->key->byte, __b2f32(ptr));
                }                
            }

        }else if (__xline_typeif_object(ptr)){

            if (__xline_obj_typeif_map(ptr)){

                parse_msg(ptr, 0);
                
            }else if (__xline_obj_typeif_text(ptr)){

                __xlogd("xline text key: %s value: %s\n", maker->key->byte, ptr->byte);

            }

        }else {
            __xlogd("xline type error\n");
        }
    }
    
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
        xmaker_ptr maker = xline_maker_create(2);
        build_msg(maker);
        xline_add_text(maker, "msg", str, slength(str));
        parse_msg((xline_ptr)maker->head, maker->wpos);
        xchannel_push_task(client->channel, maker->wpos);
        xmsger_send(client->msger, client->channel, maker->head, maker->wpos);
        xline_maker_free(maker);
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