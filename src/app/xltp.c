#include "xltp.h"
#include "xpeer.h"

#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xlib/avlmini.h>

typedef struct xmsg_ctx {
    struct xltp *xltp;
    xchannel_ptr channel;
    int(*cb)(xline_t *msg, struct xmsg_ctx *ctx);
    struct xmsg_ctx *prev, *next;
}xmsg_ctx_t;

// typedef struct xltp_api {
//     xmsg_cb_ptr cb;
//     xmsg_ctx_ptr ctx;
// }xltp_api_t;

typedef struct xltp {
    uint16_t port;
    char ip[46];
    __atom_bool runnig;
    __atom_size rid;
    xline_t parser;
    xmsger_ptr msger;

    xtree api;
    xpipe_ptr mpipe;
    __xthread_ptr task_pid;
    struct xmsgercb listener;
    
    xline_t msglist;
    struct avl_tree msgid_table;

    xmsg_ctx_t ctxlist;

}xltp_t;

static inline int msgid_comp(const void *a, const void *b)
{
	return ((xline_t*)a)->id - ((xline_t*)b)->id;
}

static inline int msgid_find(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xline_t*)b)->id;
}

static int on_message_to_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XMSG_FLAG_BACK;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_from_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XMSG_FLAG_RECV;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_timedout(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    __xcheck(channel == NULL);
    msg->flag = XMSG_FLAG_TIMEDOUT;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
     if (msg != NULL){
        xl_free(&msg);
     }
    return -1;
}

static inline xmsg_ctx_t* xltp_make_ctx(xltp_t *xltp, xmsg_cb_ptr cb, xchannel_ptr channel)
{
    xmsg_ctx_t* ctx = (xmsg_ctx_t*)malloc(sizeof(xmsg_ctx_t));
    __xcheck(ctx == NULL);
    ctx->cb = cb;
    ctx->xltp = xltp;
    ctx->channel = channel;
    ctx->next = &xltp->ctxlist;
    ctx->prev = xltp->ctxlist.prev;
    ctx->next->prev = ctx;
    ctx->prev->next = ctx;
    return ctx;
XClean:
    if (ctx){
        free(ctx);
    }
    return NULL;
}

static inline void xltp_del_ctx(xltp_t *xltp, xmsg_ctx_t *ctx)
{
    if (ctx){
        ctx->prev->next = ctx->next;
        ctx->next->prev = ctx->prev;
        free(ctx);
    }
}

static inline xline_t* xltp_make_req(xltp_t *xltp, xline_t *msg, const char *api, xmsg_cb_ptr cb)
{
    xmsg_ctx_t *ctx = xltp_make_ctx(xltp, cb, NULL);
    __xcheck(ctx == NULL);
    __xmsg_set_ctx(msg, ctx);
    msg->id = __atom_add(xltp->rid, 1);
    __xcheck(xl_add_word(&msg, "api", api) == EENDED);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == EENDED);
    return msg;
XClean:
    if (ctx){
        free(ctx);
    }
    return NULL;
}

int xltp_make_api(xltp_t *xltp, const char *api, xmsg_cb_ptr cb)
{
    __xcheck(xltp == NULL || api == NULL || cb == NULL);
    xmsg_ctx_t *ctx = (xmsg_ctx_t*)malloc(sizeof(xmsg_ctx_t));
    __xcheck(ctx == NULL);
    ctx->cb = cb;
    ctx->xltp = xltp;
    __xcheck(xtree_add(xltp->api, (void*)api, slength(api), ctx) == NULL);
    return 0;
XClean:
    return -1;
}

inline static void xltp_add_req(xltp_t *xltp, xline_t *msg)
{
    xl_hold(msg);
    msg->next = xltp->msglist.next;
    msg->prev = &xltp->msglist;
    msg->prev->next = msg;
    msg->next->prev = msg;
    avl_tree_add(&xltp->msgid_table, msg);
}

inline static xline_t* xltp_find_req(xltp_t *xltp, uint64_t rid)
{
    return avl_tree_find(&xltp->msgid_table, &rid);
}

inline static void xltp_del_req(xltp_t *xltp, xline_t *msg)
{
    avl_tree_remove(&xltp->msgid_table, msg);
    msg->prev->next = msg->next;
    msg->next->prev = msg->prev;
    xltp_del_ctx(xltp, __xmsg_get_ctx(msg));
    xl_free(&msg);
}

static inline int recv_request(xltp_t *xltp, xline_t *msg)
{
    static char *api;
    static xmsg_ctx_t *ctx;
    // static xltp_api_t *xapi;
    xltp->parser = xl_parser(&msg->data);
    api = xl_find_word(&xltp->parser, "api");
    __xcheck(api == NULL);
    msg->id = xl_find_uint(&xltp->parser, "rid");
    __xcheck(msg->id == EENDED);
    ctx = xtree_find(xltp->api, api, slength(api));
    __xcheck(ctx == NULL);
    ctx->cb(msg, ctx);
    return 0;
XClean:
    return -1;
}

static inline int recv_respnos(xltp_t *xltp, xline_t *msg)
{
    static uint64_t rid;
    static xline_t *req;
    // static xmsg_cb_ptr cb;
    static xmsg_ctx_ptr ctx;
    xltp->parser = xl_parser(&msg->data);
    rid = xl_find_uint(&xltp->parser, "rid");
    __xcheck(rid == EENDED);
    req = xltp_find_req(xltp, rid);
    __xcheck(req == NULL);
    // cb = __xmsg_get_cb(req);
    // __xcheck(cb == NULL);
    ctx = __xmsg_get_ctx(req);
    __xcheck(ctx == NULL);
    ctx->cb(msg, ctx);
    xltp_del_req(xltp, req);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv(xltp_t *xltp, xline_t *msg)
{
    if (msg->type == XPACK_TYPE_REQ){
        recv_request(xltp, msg);
    }else if (msg->type == XPACK_TYPE_RES){
        recv_respnos(xltp, msg);
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }
    xl_free(&msg);    
    return 0;
}

static inline int xltp_back(xltp_t *xltp, xline_t *msg)
{
    if (msg->type == XPACK_TYPE_REQ){
        
    }else if (msg->type == XPACK_TYPE_RES){
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }
    xl_free(&msg);
    return 0;
}

static inline int xltp_timedout(xltp_t *xltp, xline_t *msg)
{
    xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    if (msg->type == XPACK_TYPE_REQ){
        xltp_del_req(xltp, msg);
    }
    xl_free(&msg);
    return 0;
}

// static inline int xltp_send(xltp_t *xltp, xline_t *msg)
// {
//     if (__xmsg_get_cb(msg) != NULL){
//         xltp_add_req(xltp, msg);
//     }
//     if (msg->type == XPACK_TYPE_REQ){
//         xmsger_connect(xltp->msger, (xchannel_ctx_ptr)msg, msg);
//     }else if (msg->type == XPACK_TYPE_RES){
//         xmsger_disconnect(xltp->msger, __xmsg_get_channel(msg), msg);
//     }
//     return 0;
// }

static void xltp_loop(void *ptr)
{
    __xlogd("xltp_loop enter\n");

    xline_t *msg;
    int (*post_cb)(xltp_t*, xline_t*);
    xltp_t *xltp = (xltp_t*)ptr;

    while (xltp->runnig)
    {
        __xcheck(xpipe_read(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        if (msg->flag == XMSG_FLAG_RECV){

            xltp_recv(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_BACK){

            xltp_back(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_POST){

            xltp->parser = xl_parser(&msg->data);
            post_cb = xl_find_ptr(&xltp->parser, "cb");
            __xcheck(post_cb == NULL);
            __xcheck(post_cb(xltp, msg) != 0);

        }else if(msg->flag == XMSG_FLAG_TIMEDOUT){

            // xltp_timedout(xltp, msg);
            __xcheck(xmsger_flush(xltp->msger, __xmsg_get_channel(msg)) != 0);
            if (msg->type == XPACK_TYPE_REQ){
                xltp_del_req(xltp, msg);
            }
            xl_free(&msg);

        }
    }

    __xlogd("xltp_loop >>>>---------------> exit\n");

XClean:

    return;
}

int xltp_request(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_REQ;
    xltp_add_req(xltp, msg);
    xmsger_connect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

int xltp_respose(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_RES;
    xmsger_disconnect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_post(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}


////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_echo(xline_t *msg, xmsg_ctx_ptr ctx)
{
    // xltp_t *tp = (xltp_t *)ctx;
    __xcheck(msg == NULL);
    __xcheck(ctx == NULL);
    xl_printf(&msg->data);
    xl_clear(msg);
    xl_add_uint(&msg, "rid", msg->id);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == EENDED);
    __xcheck(xl_add_uint(&msg, "port", port) == EENDED);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);
    xl_add_uint(&msg, "code", 200);
    xltp_respose(ctx->xltp, msg);
    return 0;
XClean:
    return -1;
}

static int res_echo(xline_t *res, xmsg_ctx_ptr ctx)
{
    // xltp_t *tp = (xltp_t *)ctx;
    ctx->xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
XClean:
    return -1;
}

int req_echo(xltp_t *tp, xline_t *msg)
{
    msg = xltp_make_req(tp, msg, "echo", res_echo);
    __xcheck(msg == NULL);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);    
    __xcheck(xltp_request(tp, msg) != 0);
    return 0;
XClean:
    return -1;
}

int xltp_echo(xltp_t *tp, const char *ip, uint16_t port)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
    __xmsg_set_ipaddr(msg, addr);
    xl_add_ptr(&msg, "cb", req_echo);
    xltp_post(tp, msg);
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

static int api_boot(xline_t *msg, xmsg_ctx_ptr ctx)
{
    __xcheck(msg == NULL);
    __xcheck(ctx == NULL);
    xl_printf(&msg->data);
    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == EENDED);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == EENDED);
    __xcheck(xl_add_uint(&msg, "port", port) == EENDED);
    uint8_t uuid[8192];
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 8192) == EENDED);
    __xcheck(xl_add_uint(&msg, "code", 200) == EENDED);
    __xcheck(xltp_respose(ctx->xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int res_boot(xline_t *res, xmsg_ctx_ptr ctx)
{
    // xltp_t *tp = (xltp_t *)ctx;
    ctx->xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
XClean:
    return -1;
}

int req_boot(xltp_t *tp, xline_t *msg)
{
    msg = xltp_make_req(tp, msg, "boot", res_boot);
    __xcheck(msg == NULL);
    __xipaddr_ptr addr = __xapi->udp_host_to_addr("xltp.net", 9256);
    __xmsg_set_ipaddr(msg, addr);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);
    __xcheck(xltp_request(tp, msg) != 0);
    return 0;
XClean:
    return -1;
}

static inline int xltp_bootstrap(xltp_t *tp)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    xl_add_ptr(&msg, "cb", req_boot);
    // xl_add_ptr(&msg, "ctx", tp);
    xltp_post(tp, msg);
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
    xltp->api = xtree_create();
    __xcheck(xltp->api == NULL);

    xmsgercb_ptr listener = &xltp->listener;

    listener->ctx = xltp;
    listener->on_message_to_peer = on_message_to_peer;
    listener->on_message_from_peer = on_message_from_peer;
    listener->on_message_timedout = on_message_timedout;

    xltp->msglist.prev = &xltp->msglist;
    xltp->msglist.next = &xltp->msglist;

    xltp->ctxlist.prev = &xltp->ctxlist;
    xltp->ctxlist.next = &xltp->ctxlist;

    avl_tree_init(&xltp->msgid_table, msgid_comp, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));

    xltp->mpipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xcheck(xltp->mpipe == NULL);

    xltp->task_pid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->task_pid == NULL);
    
    xltp->msger = xmsger_create(&xltp->listener, 9256);
    __xcheck(xltp->msger == NULL);

    __xcheck(xltp_make_api(xltp, "echo", api_echo) != 0);
    __xcheck(xltp_make_api(xltp, "boot", api_boot) != 0);

    if (boot){
        xltp_bootstrap(xltp);
    }

    return xltp;
XClean:
    xltp_free(&xltp);
    return NULL;
}

static void xapi_clear(void *xapi)
{
    if (xapi){
        free(xapi);
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

        if (xltp->mpipe){
            xpipe_break(xltp->mpipe);
        }

        if (xltp->task_pid){
            __xapi->thread_join(xltp->task_pid);
        }

        if (xltp->mpipe){
            __xlogi("pipe readable = %u\n", xpipe_readable(xltp->mpipe));
            xline_t *msg;
            while (__xpipe_read(xltp->mpipe, &msg, __sizeof_ptr) == __sizeof_ptr){
                __xlogi("free msg\n");
                xl_free(&msg);
            }
            xpipe_free(&xltp->mpipe);
        }

        xline_t *next, *msg = xltp->msglist.next;
        while (msg != &xltp->msglist){
            next = msg->next;
            msg->prev->next = msg->next;
            msg->next->prev = msg->prev;
            xl_free(&msg);
            msg = next;
        }

        avl_tree_clear(&xltp->msgid_table, NULL);

        if (xltp->api){
            xtree_clear(xltp->api, xapi_clear);
            xtree_free(&xltp->api);
        }

        // if (xltp->addr){
        //     free(xltp->addr);
        // }

        free(xltp);
    }
}
