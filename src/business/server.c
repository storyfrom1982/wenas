#include <env/env.h>
#include <env/linetask.h>
#include <sys/struct/antenna.h>

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
    struct msgchannel_listener listener;
    linekv_ptr recv_func, send_func;
    linetask_ptr recv_task, send_task;
    antenna_ptr antenna;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}


static size_t send_msg(int socket, msgaddr_ptr addr, void *data, size_t size)
{
    return sendto(socket, data, size, 0, (struct sockaddr*)addr->byte, addr->size);
}

static size_t recv_msg(int socket, msgaddr_ptr addr, void *buf, size_t size)
{
    return recvfrom(socket, buf, size, 0, (struct sockaddr*)addr->byte, (socklen_t*)&addr->size);
}

static void connected(void *ctx, msgchannel_ptr channel)
{

}

static void disconnected(void *ctx, msgchannel_ptr channel)
{

}

static void message_arrived(void *ctx, msgchannel_ptr channel, void *data, size_t size)
{

}

static void update_status(void *ctx, msgchannel_ptr channel)
{

}

static void recv_task_func(linekv_ptr ctx)
{
    __logi("start recv_task");
    server_t *server = linekv_find_ptr(ctx, "ctx");
    antenna_start_receive(server->antenna);
}

static void send_task_func(linekv_ptr ctx)
{
    __logi("start send_task");
    server_t *server = linekv_find_ptr(ctx, "ctx");
    antenna_start_send(server->antenna);
}

int main(int argc, char *argv[])
{
    __logi("start server");

    uint16_t port = 3721;
    server_t server;
    physics_socket_ptr device = (physics_socket_ptr)malloc(sizeof(struct physics_socket));
    msgaddr_ptr addr = &server.msgaddr;
    msgchannel_listener_ptr listener = &server.listener;

    server.recv_func = linekv_create(1024);
    linekv_add_ptr(server.recv_func, "func", recv_task_func);
    linekv_add_ptr(server.recv_func, "ctx", &server);
    server.recv_task = linetask_create();

    server.send_func = linekv_create(1024);
    linekv_add_ptr(server.send_func, "func", send_task_func);
    linekv_add_ptr(server.send_func, "ctx", &server);
    server.send_task = linetask_create();

    addr->size = sizeof(struct sockaddr_in);
    addr->byte = &server.addr;
    memset(addr->byte, 0, addr->size);
    server.addr.sin_family = AF_INET;
    server.addr.sin_port = htons(port);
    server.addr.sin_addr.s_addr = INADDR_ANY;


    listener->connected = connected;
    listener->disconnected = disconnected;
    listener->message_arrived = message_arrived;
    listener->update_status = update_status;

    int fd;
    int enable = 1;
    __pass((fd = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
    __pass(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == 0);
	__pass(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) == 0);

    device->socket = fd;
    device->send = send_msg;
    device->receive = recv_msg;
    
    server.antenna = antenna_create(device);

    antenna_connect(server.antenna, addr, listener, &server);

    linetask_post(server.recv_task, server.recv_func);
    linetask_post(server.send_task, server.send_func);

    char str[100];

    while (1)
    {
        printf( "Enter a value :");
        fgets(str, 100, stdin);

        if (str[0] == 'q'){
            break;
        }

        printf( "\nsend msg len: %d", strlen(str));
        // antenna_send_message();
    }

    antenna_release(&server.antenna);

    close(fd);

#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    return 0;
Reset:
    return -1;
}