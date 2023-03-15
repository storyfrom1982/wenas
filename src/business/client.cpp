extern "C" {
    #include "env/env.h"
    #include "env/malloc.h"
}

#include <sys/struct/mtp.h>

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

#include <iostream>


typedef struct client{
    int socket;
    int local_socket;
    socklen_t addlen;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    struct msgaddr msgaddr;
    struct msglistener listener;
    msgtransmitter_ptr mtp;
}*client_ptr;



static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}

static void listening(struct physics_socket *socket)
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

static size_t send_msg(struct physics_socket *socket, msgaddr_ptr remote_addr, void *data, size_t size)
{
    // __logi("send_msg ip: %u port: %u", remote_addr->ip, remote_addr->port);
    client_ptr client = (client_ptr)socket->ctx;
    ssize_t result = sendto(client->socket, data, size, 0, (struct sockaddr*)remote_addr->addr, (socklen_t)remote_addr->addrlen);
    // __logi("send_msg result %d", result);
    return result;
}

static size_t recv_msg(struct physics_socket *socket, msgaddr_ptr remote_addr, void *buf, size_t size)
{
    client_ptr client = (client_ptr)socket->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)remote_addr->addr;
    remote_addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = recvfrom(client->socket, buf, size, 0, (struct sockaddr*)remote_addr->addr, (socklen_t*)&remote_addr->addrlen);
    if (result > 0){
        remote_addr->ip = fromaddr->sin_addr.s_addr;
        remote_addr->port = fromaddr->sin_port;
        remote_addr->keylen = 6;
    }
    // __logi("recv_msg ip: %u port: %u", remote_addr->ip, remote_addr->port);
    // __logi("recv_msg result %d", result);
    return result;
}

static void connected(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void disconnected(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void message_arrived(msglistener_ptr listener, msgchannel_ptr channel, message_ptr msg)
{
    __logi(">>>>---------------> recv msg: %s", msg->data);
    free(msg);
}

static void update_status(msglistener_ptr listener, msgchannel_ptr channel)
{

}


int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/client/log", NULL);
    __logi("start client");

    client_ptr client = (client_ptr)calloc(1, sizeof(struct client));

    const char *host = "127.0.0.1";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 3824;
    physics_socket_ptr device = (physics_socket_ptr)malloc(sizeof(struct physics_socket));
    msgaddr_ptr remote_addr = &client->msgaddr;
    msglistener_ptr listener = &client->listener;

    client->local_addr.sin_family = AF_INET;
    client->local_addr.sin_port = htons(3721);
    client->local_addr.sin_addr.s_addr = INADDR_ANY;
    if((client->local_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        __loge("socket error");
    }
    if (bind(client->local_socket, (const struct sockaddr *)&client->local_addr, sizeof(client->local_addr)) == -1){
        __loge("bind error");
    }    
	int event_flags = fcntl(client->local_socket, F_GETFL, 0);
    if (fcntl(client->local_socket, F_SETFL, event_flags | O_NONBLOCK) == -1){
        __loge("set no block failed");
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

    listener->connected = connected;
    listener->disconnected = disconnected;
    listener->message = message_arrived;
    listener->status = update_status;

    int fd;
    int enable = 1;
    if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        __loge("socket error");
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0){
        __loge("setsockopt error");
    }
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0){
        __loge("setsockopt error");
    }
	int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        __loge("set no block failed");
    }

    client->socket = fd;
    device->ctx = client;
    device->listening = listening;
    device->sendto = send_msg;
    device->recvfrom = recv_msg;
    
    listener->ctx = client;
    client->mtp = msgtransmitter_create(device, &client->listener);

    msgchannel_ptr channel = msgtransmitter_connect(client->mtp, remote_addr);

    char str[100];

    // for (size_t i = 0; i < 10000; i++)
    // {
    //     size_t len = rand() % 99;
    //     if (len < 10){
    //         len = 10;
    //     }
    //     memset(str, i % 256, len);
    //     str[len] = '\0';
    //     msgtransmitter_send(client->mtp, channel, str, strlen(str));
    // }


    while (1)
    {
        __logi("Enter a value :\n");
        fgets(str, 100, stdin);
        size_t len = strlen(str);
        if (len == 2 && str[0] == 'q'){
            break;
        }

        msgtransmitter_send(client->mtp, channel, str, strlen(str));
    }


    __loge("msgtransmitter_send finish");

    __logi("msgtransmitter_disconnect");
    msgtransmitter_disconnect(client->mtp, channel);

    ___set_false(&client->mtp->running);
    int data = 0;
    ssize_t result = sendto(client->local_socket, &data, sizeof(data), 0, (struct sockaddr*)&client->local_addr, (socklen_t)sizeof(client->local_addr));
    
    __logi("msgtransmitter_release");
    msgtransmitter_release(&client->mtp);

    __logi("free device");
    free(device);
    
    __logi("free client");
    free(client);

    close(fd);

    __logi("env_logger_stop");
    env_logger_stop();

    __logi("env_malloc_debug");
#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    __logi("exit");
    return 0;
}