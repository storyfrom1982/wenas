#include "xltp.h"

#include "xapi/xapi.h"

#include "xlio.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xpipe.h>
#include <xlib/avlmini.h>


struct xltp {
    __atom_bool runnig;
    xtree api;
    xlio_t *io;
    xframe_t parser;
    xmsger_ptr msger;
    xpipe_ptr msgpipe;
    __xthread_ptr msg_tid;
    struct xmsgercb listener;
};


static int on_message_to_peer(xmsgercb_ptr cb, xchannel_ptr channel, xframe_t *msg)
{
    msg->flag = XMSG_FLAG_BACK;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_from_peer(xmsgercb_ptr cb, xchannel_ptr channel, xframe_t *msg)
{
    msg->flag = XMSG_FLAG_RECV;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_timedout(xmsgercb_ptr cb, xchannel_ptr channel, xframe_t *msg)
{
    __xcheck(channel == NULL);
    msg->flag = XMSG_FLAG_TIMEDOUT;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
     if (msg != NULL){
        xl_free(&msg);
     }
    return -1;
}

int xltp_make_api(xltp_t *xltp, const char *api, xmsgcb_ptr cb)
{
    __xcheck(xtree_add(xltp->api, (void*)api, slength(api), cb) == NULL);
    return 0;
XClean:
    return -1;
}

static inline int xltp_send_req(xltp_t *xltp, xframe_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_REQ;
    xmsger_connect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_send_msg(xltp_t *xltp, xframe_t *msg)
{
    msg->type = XPACK_TYPE_MSG;
    xmsger_send(xltp->msger, __xmsg_get_channel(msg), msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_send_res(xltp_t *xltp, xframe_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_RES;
    xmsger_disconnect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv_req(xltp_t *xltp, xframe_t *msg)
{
    static char *api;
    static xmsgcb_ptr cb;
    xltp->parser = xl_parser(&msg->line);
    __xcheck((xl_find_word(&xltp->parser, "api", &api)) == NULL);
    __xcheck((cb = (xmsgcb_ptr)xtree_find(xltp->api, api, slength(api))) == NULL);
    __xcheck(cb(xltp, msg, NULL) != 0);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv_res(xltp_t *xltp, xframe_t *msg)
{
    static xframe_t *req;
    static xmsgcb_ptr cb;
    __xcheck((req = xchannel_get_req(__xmsg_get_channel(msg))) == NULL);
    __xcheck((cb = __xmsg_get_cb(req)) == NULL);
    __xcheck(cb(xltp, msg, NULL) != 0);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv_msg(xltp_t *xltp, xframe_t *msg)
{
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
    if (ios != NULL){
        __xcheck(xlio_stream_write(ios, msg) != 0);
    }else {
        xframe_t *req = xchannel_get_req(__xmsg_get_channel(msg));
        __xcheck(req == NULL);
        xmsgcb_ptr cb = __xmsg_get_cb(req);
        __xcheck(cb == NULL);
        __xcheck(cb(xltp, msg, NULL) != 0);
    }
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv(xltp_t *xltp, xframe_t *msg)
{
    if (msg->type == XPACK_TYPE_REQ){
        __xcheck(xltp_recv_req(xltp, msg) != 0);
    }else if (msg->type == XPACK_TYPE_MSG){
        __xcheck(xltp_recv_msg(xltp, msg) != 0);
    }else if (msg->type == XPACK_TYPE_RES){
        xlio_stream_t *ios = xchannel_get_ctx(__xmsg_get_channel(msg));
        if (ios != NULL){
            __xcheck(xlio_stream_write(ios, msg) != 0);
        }else {
            xchannel_ptr channel = __xmsg_get_channel(msg);
            if (msg->wpos > 0){
                if (xltp_recv_res(xltp, msg) != 0){
                    xl_free(&msg);
                }else {
                    msg = NULL;
                }
            }else {
                xl_free(&msg);
            }
            __xcheck(xmsger_flush(xltp->msger, channel) != 0);
        }
    }
    return 0;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return -1;
}

static inline int xltp_back(xltp_t *xltp, xframe_t *msg)
{
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
    if (ios != NULL){
        __xcheck(xlio_stream_resave(ios, msg) != 0);
    }else {
        if (msg->type == XPACK_TYPE_RES){
            __xcheck(xmsger_flush(xltp->msger, __xmsg_get_channel(msg)) != 0);
        }
        xl_free(&msg);
    }
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static inline int xltp_timedout(xltp_t *xltp, xframe_t *msg)
{
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
    if (ios != NULL){
        xlio_stream_close(ios);
    }else {
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }
    // msg 会在释放 channel 时被释放
    return 0;
XClean:
    return -1;
}

static void xltp_loop(void *ptr)
{
    __xlogd("xltp_loop enter\n");

    xframe_t *msg;
    xmsgcb_ptr cb;
    xltp_t *xltp = (xltp_t*)ptr;

    while (xltp->runnig)
    {
        __xcheck(xpipe_read(xltp->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        if (msg->flag == XMSG_FLAG_RECV){

            xltp_recv(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_BACK){

            xltp_back(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_POST){

            cb = __xmsg_get_cb(msg);
            __xcheck(cb == NULL);
            __xcheck(cb(xltp, msg, NULL) != 0);

        }else if(msg->flag == XMSG_FLAG_TIMEDOUT){

            xltp_timedout(xltp, msg);
        }
    }

    __xlogd("xltp_loop >>>>---------------> exit\n");

XClean:

    return;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_echo(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    // __xcheck(msg == NULL);
    // __xcheck(ctx == NULL);
    xl_printf(&msg->line);
    xl_clear(msg);
    // xl_add_uint(&msg, "rid", msg->id);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XEOF);
    __xcheck(xl_add_uint(&msg, "port", port) == XEOF);
    xframe_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->line);
    xl_free(&test);
    xl_add_uint(&msg, "code", 200);
    xltp_send_res(xltp, msg);
    return 0;
XClean:
    return -1;
}

static int res_echo(xltp_t *xltp, xframe_t *res, void *ctx)
{
    xltp->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    xl_free(&res);
    return 0;
XClean:
    return -1;
}

static int req_echo(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    // msg = xltp_make_req(xltp, msg, "echo", res_echo);
    // __xcheck(msg == NULL);
    xl_clear(msg);
    xl_add_word(&msg, "api", "echo");
    __xmsg_set_cb(msg, res_echo);
    xframe_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->line);
    xl_free(&test);    
    __xcheck(xltp_send_req(xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

int xltp_echo(xltp_t *xltp, const char *ip, uint16_t port)
{
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
    __xmsg_set_ipaddr(msg, addr);
    __xmsg_set_cb(msg, req_echo);
    __xcheck(xpipe_write(xltp->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_boot(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    // __xcheck(msg == NULL);
    // __xcheck(ctx == NULL);
    xl_printf(&msg->line);
    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    // __xcheck(xl_add_uint(&msg, "rid", msg->id) == XEOF);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XEOF);
    __xcheck(xl_add_uint(&msg, "port", port) == XEOF);
    uint8_t uuid[32];
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XEOF);
    __xcheck(xl_add_uint(&msg, "code", 200) == XEOF);
    __xcheck(xltp_send_res(xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int res_boot(xltp_t *xltp, xframe_t *res, void *ctx)
{
    xltp->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    xl_free(&res);
    return 0;
XClean:
    return -1;
}

static int req_boot(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    // msg = xltp_make_req(xltp, msg, "boot", res_boot);
    // __xcheck(msg == NULL);
    xl_clear(msg);
    xl_add_word(&msg, "api", "boot");
    __xmsg_set_cb(msg, res_boot);
    __xipaddr_ptr addr = __xapi->udp_host_to_addr("xltp.net", 9256);
    // __xipaddr_ptr addr = __xapi->udp_host_to_addr("192.168.1.7", 9256);
    __xmsg_set_ipaddr(msg, addr);
    xframe_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->line);
    xl_free(&test);
    __xcheck(xltp_send_req(xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

static inline int xltp_bootstrap(xltp_t *xltp)
{
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(msg, req_boot);
    __xcheck(xpipe_write(xltp->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_put(xltp_t *xltp, xframe_t *req, void *ctx)
{
    __xcheck(xlio_start_downloader(xltp->io, req, 1) != 0);
    return 0;
XClean:
    xl_clear(req);
    xl_add_uint(&req, "code", 400);
    xltp_send_res(xltp, req);
    return -1;
}

static int res_put(xltp_t *xltp, xframe_t *res, void *ctx)
{
    xframe_t *req = NULL;
    xframe_t *msg = NULL;
    // xlio_stream_t *ios = NULL;
    xltp->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    req = xchannel_get_req( __xmsg_get_channel(res));
    __xcheck(req == NULL);
    msg = __xmsg_get_ctx(req);
    __xcheck(msg == NULL);
    __xmsg_set_channel(msg, __xmsg_get_channel(res));
    __xcheck(xlio_start_uploader(xltp->io, msg, 0) != 0);
    xl_free(&res);
    return 0;
XClean:
    if (res != NULL){
        xl_free(&res);
    }
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int req_put(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    char *path;
    xframe_t *req = NULL;
    xltp->parser = xl_parser(&msg->line);
    __xcheck(xl_find_word(&xltp->parser, "path", &path) == NULL);
    __xcheck((req = xl_maker()) == NULL);
    __xmsg_set_cb(req, res_put);
    __xmsg_set_ctx(req, msg);
    __xmsg_set_ipaddr(req, __xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&req, "api", "put") == XEOF);
    __xcheck(xl_add_word(&req, "path", path) == XEOF);
    __xcheck(xltp_send_req(xltp, req) != 0);
    return 0;
XClean:
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xltp_put(xltp_t *xltp, const char *local_uri, const char *remote_path, __xipaddr_ptr ipaddr, void *ctx)
{
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xmsg_set_ipaddr(msg, ipaddr);
    __xmsg_set_cb(msg, req_put);
    xl_add_word(&msg, "uri", local_uri);
    xl_add_word(&msg, "path", remote_path);
    __xcheck(xpipe_write(xltp->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    // xlio_upload(xltp->io, path, url, ipaddr);
    return 0;
XClean:
    // if (msg != NULL){
    //     xl_free(&msg);
    // }
    return -1;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_get(xltp_t *xltp, xframe_t *req, void *ctx)
{    
    __xcheck(xlio_start_uploader(xltp->io, req, 1) != 0);
    return 0;
XClean:
    xl_clear(req);
    xl_add_uint(&req, "code", 400);
    xltp_send_res(xltp, req);
    return -1;
}

static int res_get(xltp_t *xltp, xframe_t *res, void *ctx)
{
    xframe_t *req = NULL;
    xframe_t *msg = NULL;
    xl_printf(&res->line);
    req = xchannel_get_req( __xmsg_get_channel(res));
    __xcheck(req == NULL);
    msg = __xmsg_get_ctx(req);
    __xcheck(msg == NULL);
    __xmsg_set_channel(msg, __xmsg_get_channel(res));
    __xcheck(xlio_start_downloader(xltp->io, msg, 0) != 0);
    xl_free(&res);
    return 0;
XClean:
    if (res != NULL){
        xl_free(&res);
    }
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int req_get(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    char *uri;
    xltp->parser = xl_parser(&msg->line);
    xl_find_word(&xltp->parser, "uri", &uri);
    __xcheck(uri == NULL);
    xframe_t *req = xl_maker();
    __xcheck(req == NULL);
    xl_add_word(&req, "api", "get");
    __xmsg_set_cb(req, res_get);    
    __xmsg_set_ctx(req, msg);
    __xmsg_set_ipaddr(req, __xmsg_get_ipaddr(msg));
    xl_add_word(&req, "uri", uri);
    __xcheck(xltp_send_req(xltp, req) != 0);
    return 0;
XClean:
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xltp_get(xltp_t *xltp, const char *local_path, const char *remote_uri, __xipaddr_ptr ipaddr, void *ctx)
{
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(msg, req_get);
    __xmsg_set_ctx(msg, ctx);
    __xmsg_set_ipaddr(msg, ipaddr);
    xl_add_word(&msg, "path", local_path);
    xl_add_word(&msg, "uri", remote_uri);
    __xcheck(xpipe_write(xltp->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

xltp_t* xltp_create(int boot)
{
    xltp_t *xltp = (xltp_t*)calloc(1, sizeof(struct xltp));

    __set_true(xltp->runnig);

    xmsgercb_ptr listener = &xltp->listener;

    listener->ctx = xltp;
    listener->on_message_to_peer = on_message_to_peer;
    listener->on_message_from_peer = on_message_from_peer;
    listener->on_message_timedout = on_message_timedout;

    xltp->msgpipe = xpipe_create(sizeof(void*) * 1024, "MSG PIPE");
    __xcheck(xltp->msgpipe == NULL);

    xltp->msg_tid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->msg_tid == NULL);

    xltp->msger = xmsger_create(&xltp->listener, 9256);
    __xcheck(xltp->msger == NULL);

    xltp->io = xlio_create(xltp->msger);
    __xcheck(xltp->io == NULL);

    xltp->api = xtree_create();
    __xcheck(xltp->api == NULL);
    __xcheck(xltp_make_api(xltp, "echo", api_echo) != 0);
    __xcheck(xltp_make_api(xltp, "boot", api_boot) != 0);
    __xcheck(xltp_make_api(xltp, "put", api_put) != 0);
    __xcheck(xltp_make_api(xltp, "get", api_get) != 0);

    if (boot){
        // xltp_bootstrap(xltp);
    }

    return xltp;
XClean:
    xltp_free(&xltp);
    return NULL;
}

static void xapi_clear(void *xapi)
{
    if (xapi){
        // free(xapi);
    }
}

void xltp_free(xltp_t **pptr)
{
    if (pptr && *pptr){

        xltp_t *xltp = *pptr;
        *pptr = NULL;

        if (xltp->msger){
            xmsger_free(&xltp->msger);
        }

        if (xltp->msgpipe){
            xpipe_break(xltp->msgpipe);
        }

        if (xltp->msg_tid){
            __xapi->thread_join(xltp->msg_tid);
        }

        if(xltp->io){
            xlio_free(&xltp->io);
        }

        if (xltp->msgpipe){
            __xlogi("pipe readable = %u\n", xpipe_readable(xltp->msgpipe));
            xframe_t *msg;
            while (__xpipe_read(xltp->msgpipe, &msg, __sizeof_ptr) == __sizeof_ptr){
                __xlogi("free msg\n");
                xl_free(&msg);
            }
            xpipe_free(&xltp->msgpipe);
        }

        if (xltp->api){
            xtree_clear(xltp->api, xapi_clear);
            xtree_free(&xltp->api);
        }

        free(xltp);
    }
}
