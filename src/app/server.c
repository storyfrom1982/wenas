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

static void listening(xtask_enter_ptr task_ctx)
{
    __xlogd("listening enter\n");
    server_t *server = (server_t *)task_ctx->ctx;
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
    struct xtask_enter enter;
    enter.func = listening;
    enter.ctx = server;
    xtask_push(server->task, enter);
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

static void disconnect_task(xtask_enter_ptr task)
{
    xchannel_ptr channel = task->ctx;
}

static void on_disconnection(xmsglistener_ptr listener, xchannel_ptr channel)
{
    __xlogd("on_disconnection enter\n");
    server_t *server = (server_t*)listener->ctx;
    struct xtask_enter enter;
    enter.func = disconnect_task;
    enter.ctx = server;
    xtask_push(server->task, enter);

    __xlogd("on_disconnection exit\n");
}

static void process_message(xtask_enter_ptr task_ctx)
{
    __xlogd("process_message enter\n");
    xmsg_ptr msg = (xmsg_ptr)task_ctx->ext;
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
    struct xtask_enter enter;
    enter.func = process_message;
    enter.ctx = server;
    enter.param = channel;
    enter.ext = msg;
    xtask_push(server->task, enter);    
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