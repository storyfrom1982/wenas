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
#include <arpa/inet.h>
// #include <netdb.h>
// #include <errno.h>

// #include <string.h>
// #include <fcntl.h>
// #include <poll.h>

// #include <assert.h>
// #include <stdint.h>
#include <stdio.h>



// typedef struct udpnode {
//     size_t size;
//     void *data;
//     struct udpnode *prev, *next;
// }*udpnode_ptr;


// typedef struct udpqueue {
//     ___mutex_ptr mtx;
//     ___atom_size len;
//     struct udpnode head, end;
// }*udpqueue_ptr;


typedef struct server {
    int socket;
    socklen_t addlen;
    struct sockaddr_in addr;
    struct msgaddr msgaddr;
    struct msglistener listener;
    msgtransmitter_ptr mtp;
    // udpqueue_ptr udpbuf;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __logw("%s\n", debug);
}


static size_t send_msg(struct physics_socket *socket, msgaddr_ptr addr, void *data, size_t size)
{
    __logi("send_msg ip: %u port: %u", addr->ip, addr->port);
    server_t *server = (server_t*)socket->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = sendto(server->socket, data, size, 0, (struct sockaddr*)fromaddr, (socklen_t)addr->addrlen);
    __logi("send_msg result %d", result);
    return result;
    // // __logi("send_msg size %lu", size);
    
    // udpnode_ptr node = malloc(sizeof(struct udpnode));
    // node->size = size;
    // node->data = malloc(size);
    // memcpy(node->data, data, node->size);

    // ___lock lk = ___mutex_lock(server->udpbuf->mtx);
    // node->prev = server->udpbuf->end.prev;
    // server->udpbuf->end.prev = node;
    // node->next = &server->udpbuf->end;
    // node->prev->next = node;
    // server->udpbuf->len++;
    // ___mutex_notify(server->udpbuf->mtx);
    // ___mutex_unlock(server->udpbuf->mtx, lk);

    // __logi("send_msg exit");

    // return size;
}

static size_t recv_msg(struct physics_socket *socket, msgaddr_ptr addr, void *buf, size_t size)
{
    server_t *server = (server_t*)socket->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = recvfrom(server->socket, buf, size, 0, (struct sockaddr*)fromaddr, (socklen_t*)&addr->addrlen);
    addr->ip = fromaddr->sin_addr.s_addr;
    addr->port = fromaddr->sin_port;
    addr->keylen = 6;
    __logi("recv_msg ip: %u port: %u", addr->ip, addr->port);
    __logi("recv_msg result %d", result);
    return result;

    // // *addr = server->msgaddr
    
    // ___lock lk = ___mutex_lock(server->udpbuf->mtx);
    // // __logi("recv_msg 1");
    // if (server->udpbuf->len == 0){
    //     // __logi("recv_msg 2");
    //     ___mutex_wait(server->udpbuf->mtx, lk);
    // }
    // // __logi("recv_msg 3");
    // udpnode_ptr node = server->udpbuf->head.next;
    // // __logi("recv_msg 4");
    // node->prev->next = node->next;
    // node->next->prev = node->prev;
    // // __logi("recv_msg 5");
    // server->udpbuf->len--;
    // ___mutex_unlock(server->udpbuf->mtx, lk);
    // // __logi("recv_msg size %lu", node->size);
    // // __logi("recv_msg 6");
    // memcpy(buf, node->data, node->size);
    // size = node->size;
    // free(node->data);
    // free(node);

    // __logi("recv_msg exit");

    // return size;
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
}

static void update_status(msglistener_ptr listener, msgchannel_ptr channel)
{

}

int main(int argc, char *argv[])
{
    env_backtrace_setup();
    env_logger_start("./tmp/server/log", NULL);
    __logi("start server");

    const char *host = "127.0.0.1";
    uint16_t port = atoi(argv[1]);
    // uint16_t port = 3721;
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
    if (bind(fd, (const struct sockaddr *)&server.addr, sizeof(server.addr)) == -1){
        __loge("bind error");
    }

    server.socket = fd;
    device->ctx = &server;
    device->sendto = send_msg;
    device->recvfrom = recv_msg;
    
    listener->ctx = &server;
    server.mtp = msgtransmitter_create(device, &server.listener);

    // msgchannel_ptr channel = msgtransmitter_connect(server.mtp, addr);

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
        // msgtransmitter_send(server.mtp, channel, str, strlen(str));
    }

    close(fd);
    // msgtransmitter_disconnect(server.mtp, channel);
    __logi("msgtransmitter_disconnect");
    msgtransmitter_release(&server.mtp);
    __logi("msgtransmitter_release");

    __logi("free device");
    free(device);

    __logi("env_logger_stop");
    env_logger_stop();

    __logi("env_malloc_debug");
#if defined(ENV_MALLOC_BACKTRACE)
    env_malloc_debug(malloc_debug_cb);
#endif

    __logi("exit");
    return 0;
}