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
    struct transaddr transaddr;
    struct transchannel_listener listener;
    linekv_ptr send_func;
    task_ptr send_task;
    mtp_ptr mtp;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}


static size_t send_msg(int socket, transaddr_ptr addr, void *data, size_t size)
{
    return sendto(socket, data, size, 0, (struct sockaddr*)addr->byte, addr->size);
}

static size_t recv_msg(int socket, transaddr_ptr addr, void *buf, size_t size)
{
    return recvfrom(socket, buf, size, 0, (struct sockaddr*)addr->byte, (socklen_t*)&addr->size);
}

static void connected(transchannel_listener_ptr listener, transchannel_ptr channel)
{

}

static void disconnected(transchannel_listener_ptr listener, transchannel_ptr channel)
{

}

static void message_arrived(transchannel_listener_ptr listener, transchannel_ptr channel, void *data, size_t size)
{

}

static void update_status(transchannel_listener_ptr listener, transchannel_ptr channel)
{

}

static void send_task_func(linekv_ptr ctx)
{
    __logi("start send_task");
    server_t *server = linekv_find_ptr(ctx, "ctx");
    mtp_run(server->mtp, &server->listener);
}

int main(int argc, char *argv[])
{
    __logi("start server");

    uint16_t port = 3721;
    server_t server;
    physics_socket_ptr device = (physics_socket_ptr)malloc(sizeof(struct physics_socket));
    transaddr_ptr addr = &server.transaddr;
    transchannel_listener_ptr listener = &server.listener;

    server.send_func = linekv_create(1024);
    linekv_add_ptr(server.send_func, "func", send_task_func);
    linekv_add_ptr(server.send_func, "ctx", &server);
    server.send_task = task_create();

    addr->size = 6;
    addr->byte = &server.addr.sin_addr;
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
    
    server.mtp = mtp_create(device);

    listener->ctx = &server;
    mtp_connect(server.mtp, addr);

    task_post(server.send_task, server.send_func);

    char str[100];

    while (1)
    {
        printf( "Enter a value :");
        fgets(str, 100, stdin);

        if (str[0] == 'q'){
            break;
        }

        printf( "\nsend msg len: %d", strlen(str));
        // mtp_send();
    }

    mtp_release(&server.mtp);

    close(fd);

#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    return 0;
Reset:
    return -1;
}