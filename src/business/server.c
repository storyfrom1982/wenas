#include <env/env.h>
#include <env/task.h>
#include <sys/struct/mtp.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>

// #include <sys/un.h>

// #include <netinet/tcp.h>
// #include <arpa/inet.h>
// #include <netdb.h>
// #include <errno.h>

// #include <string.h>
// #include <fcntl.h>
// #include <poll.h>

// #include <assert.h>
// #include <stdint.h>
#include <stdio.h>


typedef struct server {
    socklen_t addlen;
    struct sockaddr_in addr;
    struct msgaddr msgaddr;
    struct msglistener listener;
    linekv_ptr send_func;
    task_ptr send_task;
    msgtransmitter_ptr mtp;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}


static size_t send_msg(int socket, msgaddr_ptr addr, void *data, size_t size)
{
    return sendto(socket, data, size, 0, (struct sockaddr*)addr->addr, addr->addrlen);
}

static size_t recv_msg(int socket, msgaddr_ptr addr, void *buf, size_t size)
{
    return recvfrom(socket, buf, size, 0, (struct sockaddr*)addr->addr, (socklen_t*)&addr->addrlen);
}

static void connected(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void disconnected(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void message_arrived(msglistener_ptr listener, msgchannel_ptr channel, message_ptr msg)
{

}

static void update_status(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void send_task_func(linekv_ptr ctx)
{
    __logi("start send_task");
    server_t *server = linekv_find_ptr(ctx, "ctx");
    msgtransmitter_run(server->mtp);
}

int main(int argc, char *argv[])
{
    __logi("start server");

    uint16_t port = 3721;
    server_t server;
    physics_socket_ptr device = (physics_socket_ptr)malloc(sizeof(struct physics_socket));
    msgaddr_ptr addr = &server.msgaddr;
    msglistener_ptr listener = &server.listener;

    server.send_func = linekv_create(1024);
    linekv_add_ptr(server.send_func, "func", send_task_func);
    linekv_add_ptr(server.send_func, "ctx", &server);
    server.send_task = task_create();

    addr->keylen = 6;
    addr->ip = server.addr.sin_addr.s_addr;
    addr->port =server.addr.sin_port;
    addr->addr = &server.addr;
    addr->addrlen = sizeof(server.addr);

    server.addr.sin_family = AF_INET;
    server.addr.sin_port = htons(port);
    server.addr.sin_addr.s_addr = INADDR_ANY;


    listener->connected = connected;
    listener->disconnected = disconnected;
    listener->message = message_arrived;
    listener->status = update_status;

    int fd;
    int enable = 1;
    __pass((fd = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
    __pass(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == 0);
	__pass(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) == 0);

    device->socket = fd;
    device->send = send_msg;
    device->receive = recv_msg;
    
    server.mtp = msgtransmitter_create(device, &server.listener);

    listener->ctx = &server;
    msgtransmitter_connect(server.mtp, addr);

    task_post(server.send_task, server.send_func);

    char str[100];

    while (1)
    {
        printf( "Enter a value :");
        fgets(str, 100, stdin);

        if (str[0] == 'q'){
            break;
        }

        uint16_t u = 0;
        // printf( "\nsend msg len: %d", strlen(str));
        printf( "\nsend 1 - 255: %hu\n", u-65535U);
        // msgtransmitter_send();
    }

    msgtransmitter_release(&server.mtp);

    close(fd);

#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    return 0;
Reset:
    return -1;
}