#include <sys/struct/xmsger.h>

typedef struct server {
    int rsock;
    __atom_bool listening;
    struct __xipaddr xmsgaddr;
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
        __xapi->udp_listen(server->rsock);
        xmsger_wake(server->messenger);
        __set_false(server->listening);
    }
    __xlogd("listening exit\n");
}

static uint64_t send_number = 0, lost_number = 0;
static size_t send_msg(struct xmsgsocket *rsock, __xipaddr_ptr addr, void *data, size_t size)
{
    // // __logi("send_msg enter");
    // send_number++;
    // uint64_t randtime = __ex_clock() / 1000000ULL;
    // if ((send_number & 0x0f) == (randtime & 0x0f)){
    //     __xlogi("send_msg lost number &&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&& %llu\n", ++lost_number);
    //     return size;
    // }
    server_t *server = (server_t*)rsock->ctx;
    long result = __xapi->udp_sendto(server->rsock, addr, data, size);
    return result;
}

static size_t recv_msg(struct xmsgsocket *rsock, __xipaddr_ptr addr, void *buf, size_t size)
{
    server_t *server = (server_t*)rsock->ctx;
    long result = __xapi->udp_recvfrom(server->rsock, addr, buf, size);
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
    __xlog_open("./tmp/server/log", NULL);
    __xlogi("start server\n");

    const char *host = "127.0.0.1";
    // uint16_t port = atoi(argv[1]);
    uint16_t port = 9256;
    server_t server;
    xmsgsocket_ptr msgsock = (xmsgsocket_ptr)malloc(sizeof(struct xmsgsocket));
    __xipaddr_ptr addr = &server.xmsgaddr;
    xmsglistener_ptr listener = &server.listener;

    server.xmsgaddr = __xapi->udp_make_ipaddr(NULL, port);

    listener->onConnectionToPeer = on_connection_to_peer;
    listener->onConnectionFromPeer = on_connection_from_peer;
    listener->onDisconnection = on_disconnection;
    listener->onReceiveMessage = on_receive_message;
    listener->onSendable = on_sendable;
    listener->onIdle = on_idle;

    server.rsock = __xapi->udp_open();
    __xapi->udp_bind(server.rsock, &server.xmsgaddr);

    msgsock->ctx = &server;
    // msgsock->listening = listening;
    msgsock->sendto = send_msg;
    msgsock->recvfrom = recv_msg;
    
    server.task = xtask_create();

    listener->ctx = &server;
    server.messenger = xmsger_create(msgsock, &server.listener);
    xmsger_run(server.messenger);

    while (true)
    {
        xmsger_wait(server.messenger);
    }

    __set_false(server.messenger->running);
    __xlogi("xmsger_disconnect");
    xmsger_free(&server.messenger);
    __xlogi("xmsger_free\n");

    xtask_free(&server.task);

    __xlogi("close rsock\n");
    free(msgsock);

    __xapi->udp_close(server.rsock);
    __xapi->udp_clear_ipaddr(server.xmsgaddr);

    __xlog_close();

    __xlogi("exit\n");

    return 0;
}