#include <env/env.h>
#include <env/task.h>
#include <sys/struct/mtp.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>


typedef struct server {
    int socket;
    socklen_t addlen;
    struct sockaddr_in addr;
    struct msgaddr msgaddr;
    struct msglistener listener;
    linekey_ptr func;
    taskqueue_ptr task;
    msgtransport_ptr mtp;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}

static void listening(struct physics_socket *socket)
{
    server_t *server = (server_t*)socket->ctx;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(server->socket, &fds);
    struct timeval timeout;
    // timeout.tv_sec  = 10;
    // timeout.tv_usec = 0;
    // select(server->socket + 1, &fds, NULL, NULL, &timeout);
    select(server->socket + 1, &fds, NULL, NULL, NULL);
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size)
{
    __logi("send_msg enter");
    send_number++;
    uint64_t randtime = ___sys_clock() / 1000000ULL;
    if ((send_number & 0x0f) == (randtime & 0x0f)){
        __logi("send_msg lost number &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& %llu", ++lost_number);
        return size;
    }
    server_t *server = (server_t*)socket->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    __logi("send_msg sendto enter");
    ssize_t result = sendto(server->socket, data, size, 0, (struct sockaddr*)fromaddr, (socklen_t)addr->addrlen);
    __logi("send_msg sendto exit");
    __logi("send_msg exit");
    return result;
}

static size_t recv_msg(struct physics_socket *socket, msgaddr_ptr addr, void *buf, size_t size)
{
    server_t *server = (server_t*)socket->ctx;
    if (addr->addr == NULL){
        addr->addr = malloc(sizeof(struct sockaddr_in));
    }
    addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = recvfrom(server->socket, buf, size, 0, (struct sockaddr*)addr->addr, (socklen_t*)&addr->addrlen);
    if (result > 0){
        // struct sockaddr_in *addr_in = (struct sockaddr_in*)addr->addr;
        addr->ip = ((struct sockaddr_in*)addr->addr)->sin_addr.s_addr;
        addr->port = ((struct sockaddr_in*)addr->addr)->sin_port;
        addr->keylen = 6;
    }
    // __logi("error: %s", strerror(errno));
    return result;
}

static void channel_connection(msglistener_ptr listener, msgchannel_ptr channel)
{

}

static void disconnect_task(linekv_ptr task)
{
    msgchannel_ptr channel = (msgchannel_ptr)linekv_find_ptr(task, "ctx");
    // __logi(">>>>---------------> disconnect_task channel: 0x%x", channel);
    msgchannel_termination(&channel);
    linekv_release(&task);
}

static void channel_disconnection(msglistener_ptr listener, msgchannel_ptr channel)
{
    // __logi(">>>>---------------> channel_disconnection channel: 0x%x", channel);
    server_t *server = (server_t*)listener->ctx;
    linekv_ptr task = linekv_create(1024);
    linekv_add_ptr(task, "func", (void*)disconnect_task);
    linekv_add_ptr(task, "ctx", channel);
    taskqueue_post(server->task, task);
}

static void recv_task(linekv_ptr task)
{
    msgchannel_ptr channel = (msgchannel_ptr)linekv_find_ptr(task, "ctx");
    transmsg_ptr msg = (transmsg_ptr)linekv_find_ptr(task, "msg");
    struct linekv parser;
    linekv_parser(&parser, msg->data, msg->size);
    __logi(">>>>---------------------------------------------------> recv msg enter: %s", linekv_find_string(&parser, "msg"));
    msgtransport_send(channel->mtp, channel, msg->data, msg->size);
    __logi(">>>>---------------------------------------------------> recv msg exit");
    free(msg);
    linekv_release(&task);
}

static void channel_message(msglistener_ptr listener, msgchannel_ptr channel, transmsg_ptr msg)
{
    __logi(">>>>---------------------------------------------------> recv msg --- enter");
    server_t *server = (server_t*)listener->ctx;
    linekv_ptr task = linekv_create(1024);
    linekv_add_ptr(task, "func", (void*)recv_task);
    linekv_add_ptr(task, "ctx", channel);
    linekv_add_ptr(task, "msg", msg);
    taskqueue_post(server->task, task);
    __logi(">>>>---------------------------------------------------> recv msg --- exit");
}

static void channel_timeout(msglistener_ptr listener, msgchannel_ptr channel)
{
    // __logi(">>>>---------------> channel_timeout channel: 0x%x", channel);
    server_t *server = (server_t*)listener->ctx;
    linekv_ptr task = linekv_create(1024);
    linekv_add_ptr(task, "func", (void*)disconnect_task);
    linekv_add_ptr(task, "ctx", channel);
    taskqueue_post(server->task, task);
}

int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/server/log", NULL);
    __logi("start server");

    const char *host = "127.0.0.1";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 3824;
    server_t server;
    physics_socket_ptr device = (physics_socket_ptr)malloc(sizeof(struct physics_socket));
    msgaddr_ptr addr = &server.msgaddr;
    msglistener_ptr listener = &server.listener;

    // server.udpbuf = (udpqueue_ptr)malloc(sizeof(struct udpqueue));
    // server.udpbuf->mtx = ___mutex_create();
    // server.udpbuf->len = 0;
    // server.udpbuf->head.prev = NULL;
    // server.udpbuf->end.next = NULL;
    // server.udpbuf->head.next = &server.udpbuf->end;
    // server.udpbuf->end.prev = &server.udpbuf->head;

    server.addr.sin_family = AF_INET;
    server.addr.sin_port = htons(port);
    server.addr.sin_addr.s_addr = INADDR_ANY;
    // inet_aton(host, &server.addr.sin_addr);

    addr->keylen = 6;
    addr->ip = server.addr.sin_addr.s_addr;
    addr->port =server.addr.sin_port;
    addr->addr = &server.addr;
    addr->addrlen = sizeof(server.addr);

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
    if (bind(fd, (const struct sockaddr *)&server.addr, sizeof(server.addr)) == -1){
        __loge("bind error");
    }
	int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        __loge("set no block failed");
    }

    server.socket = fd;
    device->ctx = &server;
    device->listening = listening;
    device->sendto = send_msg;
    device->recvfrom = recv_msg;
    
    listener->ctx = &server;
    server.mtp = msgtransport_create(device, &server.listener);

    server.task = taskqueue_create();

    msgtransport_recv_loop(server.mtp);

    ___set_false(&server.mtp->running);
    int data = 0;
    ssize_t result = sendto(server.socket, &data, sizeof(data), 0, (struct sockaddr*)&server.addr, (socklen_t)sizeof(server.addr));
    __logi("msgtransport_disconnect");
    msgtransport_release(&server.mtp);
    __logi("msgtransport_release");

    __logi("free device");
    free(device);

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