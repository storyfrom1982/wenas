#include <ex/ex.h>
#include <sys/struct/xmessenger.h>

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
    ___atom_bool listening;
    socklen_t addlen;
    struct sockaddr_in addr;
    struct xmsgaddr xmsgaddr;
    struct xmsglistener listener;
    xkey_ptr func;
    __ex_task_ptr task;
    xmessenger_ptr messenger;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __ex_logd("%s\n", debug);
}

static void listening(xmaker_ptr task_ctx)
{
    __ex_logd("listening enter\n");
    __ex_logd("listening ctx = %p ctx->addr = %p\n", task_ctx, task_ctx->addr);
    server_t *server = (server_t *)xline_find_ptr(task_ctx, "ctx");
    if (___set_true(&server->listening)){
        __ex_logd("listening server = %p\n", server);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server->socket, &fds);
        struct timeval timeout;
        // timeout.tv_sec  = 10;
        // timeout.tv_usec = 0;
        // select(server->socket + 1, &fds, NULL, NULL, &timeout);
        select(server->socket + 1, &fds, NULL, NULL, NULL);
        xmessenger_wake(server->messenger);
        ___set_false(&server->listening);
    }
    __ex_logd("listening exit\n");
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *socket, xmsgaddr_ptr addr, void *data, size_t size)
{
    // __logi("send_msg enter");
    send_number++;
    uint64_t randtime = __ex_clock() / 1000000ULL;
    if ((send_number & 0x0f) == (randtime & 0x0f)){
        __ex_logi("send_msg lost number &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& %llu\n", ++lost_number);
        return size;
    }
    server_t *server = (server_t*)socket->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    // __logi("send_msg sendto enter");
    ssize_t result = sendto(server->socket, data, size, 0, (struct sockaddr*)fromaddr, (socklen_t)addr->addrlen);
    // __logi("send_msg sendto exit");
    // __logi("send_msg exit");
    return result;
}

static size_t recv_msg(struct xmsgsocket *socket, xmsgaddr_ptr addr, void *buf, size_t size)
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

static void on_idle(xmsglistener_ptr listener, xmsgchannel_ptr channel)
{
    server_t *server = (server_t*)listener->ctx;
    __ex_logd("on_idle server = %p\n", server);
    xmaker_ptr ctx = __ex_task_hold_pusher(server->task);
    __ex_logd("1 on_idle ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    __ex_logd("1 maker len = %lu wpos = %lu\n", ctx->len, ctx->wpos);
    xline_add_ptr(ctx, "func", listening);
    __ex_logd("2 maker len = %lu wpos = %lu\n", ctx->len, ctx->wpos);
    xline_add_ptr(ctx, "ctx", server);
    __ex_logd("2 on_idle ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    __ex_task_update_pusher(server->task);
}

static void on_connection_from_peer(xmsglistener_ptr listener, xmsgchannel_ptr channel)
{

}

static void on_connection_to_peer(xmsglistener_ptr listener, xmsgchannel_ptr channel)
{

}

static void on_sendable(xmsglistener_ptr listener, xmsgchannel_ptr channel)
{

}

static void disconnect_task(xmaker_ptr task)
{
    xmsgchannel_ptr channel = (xmsgchannel_ptr)xline_find_ptr(task, "ctx");
}

static void on_disconnection(xmsglistener_ptr listener, xmsgchannel_ptr channel)
{
    server_t *server = (server_t*)listener->ctx;
    xmaker_ptr ctx = __ex_task_hold_pusher(server->task);
    xline_add_ptr(ctx, "func", disconnect_task);
    xline_add_ptr(ctx, "ctx", channel);
    __ex_task_update_pusher(server->task);
}

static void process_message(xmaker_ptr task_ctx)
{

}

static void on_receive_message(xmsglistener_ptr listener, xmsgchannel_ptr channel, xmsg_ptr msg)
{
    server_t *server = (server_t*)listener->ctx;
    xmaker_ptr ctx = __ex_task_hold_pusher(server->task);
    xline_add_ptr(ctx, "func", process_message);
    xline_add_ptr(ctx, "ctx", channel);
    xline_add_ptr(ctx, "msg", msg);
    __ex_task_update_pusher(server->task);
}

int main(int argc, char *argv[])
{
    __ex_backtrace_setup();
    __ex_log_file_open("./tmp/server/log", NULL);
    __ex_logi("start server\n");

    const char *host = "127.0.0.1";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 3824;
    server_t server;
    xmsgsocket_ptr msgsock = (xmsgsocket_ptr)malloc(sizeof(struct xmsgsocket));
    xmsgaddr_ptr addr = &server.xmsgaddr;
    xmsglistener_ptr listener = &server.listener;

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

    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onReceiveMessage = on_receive_message;
    listener->onSendable = on_sendable;
    listener->onIdle = on_idle;

    int fd;
    int enable = 1;
    if((fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
        __ex_loge("socket error");
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0){
        __ex_loge("setsockopt error");
    }
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0){
        __ex_loge("setsockopt error");
    }
    if (bind(fd, (const struct sockaddr *)&server.addr, sizeof(server.addr)) == -1){
        __ex_loge("bind error");
    }
	int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        __ex_loge("set no block failed");
    }

    server.socket = fd;
    msgsock->ctx = &server;
    // msgsock->listening = listening;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    server.task = __ex_task_create();

    listener->ctx = &server;
    server.messenger = xmessenger_create(msgsock, &server.listener);
    xmessenger_run(server.messenger);

    xmessenger_connect(server.messenger, addr);
    
    // struct xmaker task_ctx;
    // xline_maker_setup(&task_ctx, NULL, 256);
    // xline_add_ptr(&task_ctx, "func", listening);
    // xline_add_ptr(&task_ctx, "ctx", &server);
    // __ex_task_post(server.task, task_ctx.xline);

    while (true)
    {
        xmessenger_wait(server.messenger);
    }

    ___set_false(&server.messenger->running);
    int data = 0;
    ssize_t result = sendto(server.socket, &data, sizeof(data), 0, (struct sockaddr*)&server.addr, (socklen_t)sizeof(server.addr));
    __ex_logi("xmessenger_disconnect");
    xmessenger_free(&server.messenger);
    __ex_logi("xmessenger_free\n");

    __ex_task_free(&server.task);

    __ex_logi("close socket\n");
    free(msgsock);

    close(fd);

    __ex_log_file_close();

    __ex_logi("exit\n");

    return 0;
}