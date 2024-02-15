#include <ex/ex.h>
#include <sys/struct/xmsger.h>

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

#include <ex/xatom.h>


typedef struct server {
    int rsock;
    __atom_bool listening;
    socklen_t addlen;
    struct sockaddr_in addr;
    struct xmsgaddr xmsgaddr;
    struct xmsglistener listener;
    xkey_ptr func;
    xtask_ptr task;
    xmsger_ptr messenger;
}server_t;


static void malloc_debug_cb(const char *debug)
{
    __xlogd("%s\n", debug);
}

static void listening(xmaker_ptr task_ctx)
{
    __xlogd("listening enter\n");
    __xlogd("listening ctx = %p ctx->addr = %p\n", task_ctx, task_ctx->addr);
    server_t *server = (server_t *)xline_find_ptr(task_ctx, "ctx");
    if (__set_true(server->listening)){
        __xlogd("listening server = %p\n", server);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server->rsock, &fds);
        struct timeval timeout;
        // timeout.tv_sec  = 10;
        // timeout.tv_usec = 0;
        // select(server->rsock + 1, &fds, NULL, NULL, &timeout);
        select(server->rsock + 1, &fds, NULL, NULL, NULL);
        xmsger_wake(server->messenger);
        __set_false(server->listening);
    }
    __xlogd("listening exit\n");
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *rsock, xmsgaddr_ptr addr, void *data, size_t size)
{
    // // __logi("send_msg enter");
    // send_number++;
    // uint64_t randtime = __ex_clock() / 1000000ULL;
    // if ((send_number & 0x0f) == (randtime & 0x0f)){
    //     __xlogi("send_msg lost number &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& %llu\n", ++lost_number);
    //     return size;
    // }
    server_t *server = (server_t*)rsock->ctx;
    struct sockaddr_in *fromaddr = (struct sockaddr_in *)addr->addr;
    addr->addrlen = sizeof(struct sockaddr_in);
    // __logi("send_msg sendto enter");
    ssize_t result = sendto(server->rsock, data, size, 0, (struct sockaddr*)fromaddr, (socklen_t)addr->addrlen);
    // __logi("send_msg sendto exit");
    // __logi("send_msg exit");
    return result;
}

static size_t recv_msg(struct xmsgsocket *rsock, xmsgaddr_ptr addr, void *buf, size_t size)
{
    server_t *server = (server_t*)rsock->ctx;
    if (addr->addr == NULL){
        addr->addr = malloc(sizeof(struct sockaddr_in));
    }
    addr->addrlen = sizeof(struct sockaddr_in);
    ssize_t result = recvfrom(server->rsock, buf, size, 0, (struct sockaddr*)addr->addr, (socklen_t*)&addr->addrlen);
    if (result > 0){
        // struct sockaddr_in *addr_in = (struct sockaddr_in*)addr->addr;
        addr->ip = ((struct sockaddr_in*)addr->addr)->sin_addr.s_addr;
        addr->port = ((struct sockaddr_in*)addr->addr)->sin_port;
        addr->keylen = 6;
    }
    // __logi("error: %s", strerror(errno));
    return result;
}

static void on_idle(xmsglistener_ptr listener, xchannel_ptr channel)
{
    server_t *server = (server_t*)listener->ctx;
    __xlogd("on_idle server = %p\n", server);
    xmaker_ptr ctx = xtask_hold_pusher(server->task);
    __xlogd("1 on_idle ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    __xlogd("1 maker len = %lu wpos = %lu\n", ctx->len, ctx->wpos);
    xline_add_ptr(ctx, "func", listening);
    __xlogd("2 maker len = %lu wpos = %lu\n", ctx->len, ctx->wpos);
    xline_add_ptr(ctx, "ctx", server);
    __xlogd("2 on_idle ctx = %p ctx->addr = %p\n", ctx, ctx->addr);
    xtask_update_pusher(server->task);
}

static void on_connection_from_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_from_peer enter\n");
    __xlogd("on_connection_from_peer exit\n");
}

static void on_connection_to_peer(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_connection_to_peer enter\n");
    __xlogd("on_connection_to_peer exit\n");
}

static void on_sendable(xmsglistener_ptr listener, xchannel_ptr channel)
{

}

static void disconnect_task(xmaker_ptr task)
{
    xchannel_ptr channel = (xchannel_ptr)xline_find_ptr(task, "ctx");
}

static void on_disconnection(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnection enter\n");
    server_t *server = (server_t*)listener->ctx;
    xmaker_ptr ctx = xtask_hold_pusher(server->task);
    xline_add_ptr(ctx, "func", disconnect_task);
    xline_add_ptr(ctx, "ctx", channel);
    xtask_update_pusher(server->task);
    __xlogd("on_disconnection exit\n");
}

static void process_message(xmaker_ptr task_ctx)
{
    __xlogd("process_message enter\n");
    xmsg_ptr msg = (xmsg_ptr)xline_find_ptr(task_ctx, "msg");
    struct xmaker parser = xline_parse((xline_ptr)msg->data);
    xline_ptr line = xline_find(&parser, "msg");
    if (line){
        char *cmsg = strndup((char*)__xline_to_data(line), (size_t)__xline_sizeof_data(line));
        __xlogd("process_message >>>>--------------> %s\n", cmsg);
        free(cmsg);
    }
    __xlogd("process_message exit\n");
}

static void on_receive_message(xmsglistener_ptr listener, xchannel_ptr channel, xmsg_ptr msg)
{
    server_t *server = (server_t*)listener->ctx;
    xmaker_ptr ctx = xtask_hold_pusher(server->task);
    xline_add_ptr(ctx, "func", process_message);
    xline_add_ptr(ctx, "ctx", channel);
    xline_add_ptr(ctx, "msg", msg);
    xtask_update_pusher(server->task);
}

int main(int argc, char *argv[])
{
    __ex_backtrace_setup();
    __xlog_open("./tmp/server/log", NULL);
    __xlogi("start server\n");

    const char *host = "127.0.0.1";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 9256;
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
        __xloge("rsock error");
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0){
        __xloge("setsockopt error");
    }
	if(setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0){
        __xloge("setsockopt error");
    }
    if (bind(fd, (const struct sockaddr *)&server.addr, sizeof(server.addr)) == -1){
        __xloge("bind error");
    }
	int flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
        __xloge("set no block failed");
    }

    server.rsock = fd;
    msgsock->ctx = &server;
    // msgsock->listening = listening;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    server.task = xtask_create();

    listener->ctx = &server;
    server.messenger = xmsger_create(msgsock, &server.listener);
    xmsger_run(server.messenger);

    // xmsger_connect(server.messenger, addr);
    
    // struct xmaker task_ctx;
    // xline_maker_setup(&task_ctx, NULL, 256);
    // xline_add_ptr(&task_ctx, "func", listening);
    // xline_add_ptr(&task_ctx, "ctx", &server);
    // __ex_task_post(server.task, task_ctx.xline);

    while (true)
    {
        xmsger_wait(server.messenger);
    }

    __set_false(server.messenger->running);
    int data = 0;
    ssize_t result = sendto(server.rsock, &data, sizeof(data), 0, (struct sockaddr*)&server.addr, (socklen_t)sizeof(server.addr));
    __xlogi("xmsger_disconnect");
    xmsger_free(&server.messenger);
    __xlogi("xmsger_free\n");

    xtask_free(&server.task);

    __xlogi("close rsock\n");
    free(msgsock);

    close(fd);

    __xlog_close();

    __xlogi("exit\n");

    return 0;
}