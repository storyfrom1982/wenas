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

#include <stdio.h>


typedef struct client{
    int socket;
    int local_socket;
    socklen_t addlen;
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    struct msgaddr msgaddr;
    struct msglistener listener;
    msgtransport_ptr mtp;
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

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct physics_socket *socket, msgaddr_ptr remote_addr, void *data, size_t size)
{
    send_number++;
    uint64_t randtime = ___sys_clock() / 1000ULL;
    if ((send_number & 0xdf) == (randtime & 0xdf)){
        // __logi("send_msg clock: %x number: %x lost number: %llu", randtime, send_number, ++lost_number);
        return size;
    }
    client_ptr client = (client_ptr)socket->ctx;
    ssize_t result = sendto(client->socket, data, size, 0, (struct sockaddr*)remote_addr->addr, (socklen_t)remote_addr->addrlen);
    // __logi("send_msg result %d", result);
    return result;
}

static size_t recv_msg(struct physics_socket *socket, msgaddr_ptr addr, void *buf, size_t size)
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

static void channel_connection(msglistener_ptr listener, msgchannel_ptr channel)
{
    __logi(">>>>---------------> channel connection: 0x%x", channel);
}

static void channel_disconnection(msglistener_ptr listener, msgchannel_ptr channel)
{
    __logi(">>>>---------------> channel disconnection: 0x%x", channel);
    msgchannel_termination(&channel);
}

static void channel_message(msglistener_ptr listener, msgchannel_ptr channel, transmsg_ptr msg)
{
    struct linekv parser;
    linekv_parser(&parser, msg->data, msg->size);
    __logi(">>>>---------------> channel msg: %llu  content %s", msg->size, linekv_find_string(&parser, "msg"));
    free(msg);
}

static void channel_timeout(msglistener_ptr listener, msgchannel_ptr channel)
{
    __logi(">>>>---------------> channel timeout: 0x%x", channel);
    msgchannel_termination(&channel);
}


int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/client/log", NULL);
    __logi("start client");

    client_ptr client = (client_ptr)calloc(1, sizeof(struct client));

    const char *host = "127.0.0.1";
    // const char *host = "47.98.176.55";
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

    listener->connection = channel_connection;
    listener->disconnection = channel_disconnection;
    listener->message = channel_message;
    listener->timeout = channel_timeout;

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
    client->mtp = msgtransport_create(device, &client->listener);

    std::thread thread([&](){
        msgtransport_recv_loop(client->mtp);
    });

    msgchannel_ptr channel = msgtransport_connect(client->mtp, remote_addr);

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
    //         msgtransport_send(client->mtp, channel, str, len);
    //     }
    // }
    
    while (1)
    {
        __logi("Enter a value :\n");
        fgets(str, 100, stdin);
        size_t len = strlen(str);
        if (len == 2 && str[0] == 'q'){
            break;
        }
        str[len-1] = '\0';
        linekv_ptr msg = linekv_create(1024);
        linekv_add_string(msg, "msg", str);
        msgtransport_send(client->mtp, channel, msg->head, msg->pos);
        linekv_release(&msg);
    }


    __loge("msgtransport_send finish");

    __logi("msgtransport_disconnect");
    msgtransport_disconnect(client->mtp, channel);

    ___set_false(&client->mtp->running);
    int data = 0;
    ssize_t result = sendto(client->local_socket, &data, sizeof(data), 0, (struct sockaddr*)&client->local_addr, (socklen_t)sizeof(client->local_addr));
    
    __logi("msgtransport_release");
    msgtransport_release(&client->mtp);

    __logi("free device");
    free(device);
    
    __logi("free client");
    free(client);

    close(fd);

    thread.join();

    __logi("env_logger_stop");
    env_logger_stop();

    __logi("env_malloc_debug");
#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    __logi("exit");
    return 0;
}