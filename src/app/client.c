#include "ex/ex.h"

#include <sys/struct/xmsger.h>

// #include <sys/time.h>
// #include <sys/types.h>
// #include <sys/ioctl.h>
// #include <sys/socket.h>
// #include <unistd.h>
// #include <netinet/in.h>
// #include <sys/select.h>
// #include <fcntl.h>

// // #include <sys/un.h>

// // #include <netinet/tcp.h>
// #include <arpa/inet.h>

#include <stdio.h>


typedef struct client{
    int lsock, rsock, maxsock;
    // socklen_t addlen;
    // struct sockaddr_in laddr;
    // struct sockaddr_in raddr;
    struct xmsgaddr xmsgaddr;
    struct xmsglistener listener;
    xmsger_ptr msger;
    xchannel_ptr channel;
    uint8_t lmsgpack[PACK_ONLINE_SIZE];
}*client_ptr;



static void malloc_debug_cb(const char *debug)
{
    __xloge("%s\n", debug);
}


// static void recv_local_msg(client_ptr client)
// {
//     struct sockaddr_in client_addr;
//     socklen_t addr_len = sizeof(client_addr);
//     // 从客户端接收数据
//     long result = recvfrom(client->lsock, client->lmsgpack, PACK_ONLINE_SIZE, 0, (struct sockaddr*)&client_addr, &addr_len);
//     __xapi->udp_recvfrom
// }

static void listening(struct xmsgsocket *rsock)
{
    client_ptr client = (client_ptr)rsock->ctx;
	// fd_set fds;
	// FD_ZERO(&fds);
    // FD_SET(client->lsock, &fds);
	// FD_SET(client->rsock, &fds);
    // struct timeval timeout;
    // // timeout.tv_sec  = 10;
    // // timeout.tv_usec = 0;
    // // select(client->rsock + 1, &fds, NULL, NULL, &timeout);
    // select(client->maxsock + 1, &fds, NULL, NULL, NULL);

    __xapi->udp_listen(client->rsock);

    // // 有数据可读
    // if (FD_ISSET(client->lsock, &fds)) {

    //     recv_local_msg(client);

    // }else {

    //     xmsger_wake(client->msger);
    // }

}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *rsock, xmsgaddr_ptr raddr, void *data, size_t size)
{
    // // send_number++;
    // // uint64_t randtime = __ex_clock() / 1000ULL;
    // // if ((send_number & 0xdf) == (randtime & 0xdf)){
    // //     // __logi("send_msg clock: %x number: %x lost number: %llu", randtime, send_number, ++lost_number);
    // //     return size;
    // // }
    client_ptr client = (client_ptr)rsock->ctx;
    // ssize_t result = sendto(client->rsock, data, size, 0, (struct sockaddr*)raddr->addr, (socklen_t)raddr->addrlen);
    // // __logi("send_msg result %d", result);
    long result = __xapi->udp_sendto(client->rsock, raddr, data, size);
    return result;
}

static size_t recv_msg(struct xmsgsocket *rsock, xmsgaddr_ptr addr, void *buf, size_t size)
{
    client_ptr client = (client_ptr)rsock->ctx;
    // if (addr->addr == NULL){
    //     addr->addr = malloc(sizeof(struct sockaddr_in));
    //     addr->keylen = 6;
    // }    
    // struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    // addr->addrlen = sizeof(struct sockaddr_in);
    // ssize_t result = recvfrom(client->rsock, buf, size, 0, (struct sockaddr*)addr->addr, (socklen_t*)&addr->addrlen);
    // if (result > 0){
    //     addr->ip = fromaddr->sin_addr.s_addr;
    //     addr->port = fromaddr->sin_port;
    //     addr->keylen = 6;
    // }
    // // __logi("recv_msg ip: %u port: %u", raddr->ip, raddr->port);
    // // __logi("recv_msg result %d", result);
    long result = __xapi->udp_recvfrom(client->rsock, addr, buf, size);
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
    xmsgaddr_ptr raddr = &client->xmsgaddr;
    xmsglistener_ptr listener = &client->listener;

    __xlogi("start client 2\n");

    client->xmsgaddr = __xapi->udp_build_addr(host, port);

    // client->laddr.sin_family = AF_INET;
    // client->laddr.sin_port = htons(9613);
    // client->laddr.sin_addr.s_addr = INADDR_ANY;
    // if((client->lsock = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
    //     __xloge("rsock error\n");
    // }
    // if (bind(client->lsock, (const struct sockaddr *)&client->laddr, sizeof(client->laddr)) == -1){
    //     __xloge("bind error\n");
    // }    
	// int event_flags = fcntl(client->lsock, F_GETFL, 0);
    // if (fcntl(client->lsock, F_SETFL, event_flags | O_NONBLOCK) == -1){
    //     __xloge("set no block failed\n");
    // }

    __xlogi("start client 3\n");

    // client->raddr.sin_family = AF_INET;
    // client->raddr.sin_port = htons(port);
    // // client->raddr.sin_addr.s_addr = INADDR_ANY;
    // inet_aton(host, &client->raddr.sin_addr);

    // raddr->keylen = 6;
    // raddr->ip = client->raddr.sin_addr.s_addr;
    // raddr->port =client->raddr.sin_port;
    // raddr->addr = &client->raddr;
    // raddr->addrlen = sizeof(client->raddr);

    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onReceiveMessage = on_receive_message;
    listener->onSendable = on_sendable;
    listener->onIdle = on_idle;

    __xlogi("start client 4\n");

    int fd = __xapi->udp_open();
    // int enable = 1;
    // if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
    //     __xloge("rsock error\n");
    // }
    // if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0){
    //     __xloge("setsockopt error\n");
    // }
	// if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0){
    //     __xloge("setsockopt error\n");
    // }
	// int flags = fcntl(fd, F_GETFL, 0);
    // if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
    //     __xloge("set no block failed\n");
    // }

    __xlogi("start client 5\n");

    client->rsock = fd;
    client->maxsock = (client->rsock > client->lsock) ? client->rsock : client->lsock;

    msgsock->ctx = client;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    listener->ctx = client;
    __xlogi("start client 2\n");
    client->msger = xmsger_create(msgsock, &client->listener);
    xmsger_run(client->msger);

    // std::thread thread([&](){
    //     xmsger_wake(client->mtp);
    // });

    xmsger_connect(client->msger, raddr);

    char str[1024];

    // // for (size_t x = 0; x < 10; x++)
    // {
    //     for (size_t i = 0; i < 10000; i++)
    //     {
    //         size_t len = rand() % 256;
    //         if (len < 8){
    //             len = 8;
    //         }
    //         memset(str, i % 256, len);
    //         str[len] = '\0';
    //         xmsger_send(client->mtp, channel, str, len);
    //     }
    // }
    
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


    __xloge("xmsger_send finish\n");

    __xlogi("xmsger_disconnect\n");
    // xmsger_disconnect(client->mtp, channel);

    __set_false(client->msger->running);
    // int data = 0;
    // long result = sendto(client->lsock, &data, sizeof(data), 0, (struct sockaddr*)&client->laddr, (socklen_t)sizeof(client->laddr));
    
    __xlogi("xmsger_free\n");
    xmsger_free(&client->msger);

    __xlogi("free msgsock\n");
    free(msgsock);

    __xapi->udp_destoy_addr(client->xmsgaddr);
    
    __xlogi("free client\n");
    free(client);

    __xapi->udp_close(fd);


    __xlogi("env_logger_stop\n");
    __xlog_close();

    __xlogi("exit\n");
    return 0;
}