#include "ex/ex.h"

extern "C" {
    #include "ex/malloc.h"
    #include <sys/struct/xmsger.h>
}

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>

// #include <sys/un.h>

// #include <netinet/tcp.h>
#include <arpa/inet.h>

#include <stdio.h>


typedef struct client{
    int socket;
    int local_socket;
    socklen_t addlen;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    struct xmsgaddr xmsgaddr;
    struct xmsglistener listener;
    xmsger_ptr msger;
    xchannel_ptr channel;
}*client_ptr;



static void malloc_debug_cb(const char *debug)
{
    __xloge("%s\n", debug);
}

static void listening(struct xmsgsocket *socket)
{
    client_ptr client = (client_ptr)socket->ctx;
	fd_set fds;
	FD_ZERO(&fds);
    FD_SET(client->local_socket, &fds);
	FD_SET(client->socket, &fds);
    struct timeval timeout;
    // timeout.tv_sec  = 10;
    // timeout.tv_usec = 0;
    // select(client->socket + 1, &fds, NULL, NULL, &timeout);
    select(client->socket + 1, &fds, NULL, NULL, NULL);
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *socket, xmsgaddr_ptr remote_addr, void *data, size_t size)
{
    // send_number++;
    // uint64_t randtime = __ex_clock() / 1000ULL;
    // if ((send_number & 0xdf) == (randtime & 0xdf)){
    //     // __logi("send_msg clock: %x number: %x lost number: %llu", randtime, send_number, ++lost_number);
    //     return size;
    // }
    client_ptr client = (client_ptr)socket->ctx;
    ssize_t result = sendto(client->socket, data, size, 0, (struct sockaddr*)remote_addr->addr, (socklen_t)remote_addr->addrlen);
    // __logi("send_msg result %d", result);
    return result;
}

static size_t recv_msg(struct xmsgsocket *socket, xmsgaddr_ptr addr, void *buf, size_t size)
{
    client_ptr client = (client_ptr)socket->ctx;
    if (addr->addr == NULL){
        addr->addr = malloc(sizeof(struct sockaddr_in));
        addr->keylen = 6;
    }    
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = recvfrom(client->socket, buf, size, 0, (struct sockaddr*)addr->addr, (socklen_t*)&addr->addrlen);
    if (result > 0){
        addr->ip = fromaddr->sin_addr.s_addr;
        addr->port = fromaddr->sin_port;
        addr->keylen = 6;
    }
    // __logi("recv_msg ip: %u port: %u", remote_addr->ip, remote_addr->port);
    // __logi("recv_msg result %d", result);
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
    __ex_backtrace_setup();
    __xlog_open("./tmp/client/log", NULL);
    __xlogi("start client");

    client_ptr client = (client_ptr)calloc(1, sizeof(struct client));

    // const char *host = "127.0.0.1";
    const char *host = "47.99.146.226";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 3824;
    xmsgsocket_ptr msgsock = (xmsgsocket_ptr)malloc(sizeof(struct xmsgsocket));
    xmsgaddr_ptr remote_addr = &client->xmsgaddr;
    xmsglistener_ptr listener = &client->listener;

    client->local_addr.sin_family = AF_INET;
    client->local_addr.sin_port = htons(3721);
    client->local_addr.sin_addr.s_addr = INADDR_ANY;
    if((client->local_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        __xloge("socket error\n");
    }
    if (bind(client->local_socket, (const struct sockaddr *)&client->local_addr, sizeof(client->local_addr)) == -1){
        __xloge("bind error\n");
    }    
	int event_flags = fcntl(client->local_socket, F_GETFL, 0);
    if (fcntl(client->local_socket, F_SETFL, event_flags | O_NONBLOCK) == -1){
        __xloge("set no block failed\n");
    }

    client->remote_addr.sin_family = AF_INET;
    client->remote_addr.sin_port = htons(port);
    // client->remote_addr.sin_addr.s_addr = INADDR_ANY;
    inet_aton(host, &client->remote_addr.sin_addr);

    remote_addr->keylen = 6;
    remote_addr->ip = client->remote_addr.sin_addr.s_addr;
    remote_addr->port =client->remote_addr.sin_port;
    remote_addr->addr = &client->remote_addr;
    remote_addr->addrlen = sizeof(client->remote_addr);

    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onReceiveMessage = on_receive_message;
    listener->onSendable = on_sendable;
    listener->onIdle = on_idle;

    int fd;
    int enable = 1;
    if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        __xloge("socket error\n");
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0){
        __xloge("setsockopt error\n");
    }
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0){
        __xloge("setsockopt error\n");
    }
	int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        __xloge("set no block failed\n");
    }

    client->socket = fd;
    msgsock->ctx = client;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    listener->ctx = client;
    client->msger = xmsger_create(msgsock, &client->listener);
    xmsger_run(client->msger);

    // std::thread thread([&](){
    //     xmsger_wake(client->mtp);
    // });

    xmsger_connect(client->msger, remote_addr);

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
        size_t len = strlen(str);
        if (len == 2 && str[0] == 'q'){
            break;
        }
        str[len-1] = '\0';
        xmaker maker;
        xline_maker_setup(&maker, NULL, 1024);
        xline_add_text(&maker, "msg", str, slength(str));
        xmsger_send(client->msger, client->channel, maker.addr, maker.wpos + 9);
        xline_maker_clear(&maker);
    }


    __xloge("xmsger_send finish\n");

    __xlogi("xmsger_disconnect\n");
    // xmsger_disconnect(client->mtp, channel);

    ___set_false(&client->msger->running);
    int data = 0;
    ssize_t result = sendto(client->local_socket, &data, sizeof(data), 0, (struct sockaddr*)&client->local_addr, (socklen_t)sizeof(client->local_addr));
    
    __xlogi("xmsger_free\n");
    xmsger_free(&client->msger);

    __xlogi("free msgsock\n");
    free(msgsock);
    
    __xlogi("free client\n");
    free(client);

    close(fd);

    // thread.join();

    __xlogi("env_logger_stop\n");
    __xlog_close();

    __xlogi("exit\n");
    return 0;
}