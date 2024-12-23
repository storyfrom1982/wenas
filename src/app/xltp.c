#include "xltp.h"
#include "xpeer.h"

#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xlib/avlmini.h>

// typedef struct xapi_ctx {
//     struct xltp *xltp;
//     xchannel_ptr channel;
// }xmsg_ctx_t;

typedef struct xltp {
    uint16_t port;
    char ip[46];
    __atom_bool runnig;
    __atom_size rid;
    xapi_ctx_ptr ctx;
    xline_t parser;
    // __xipaddr_ptr addr;
    xmsger_ptr msger;

    xtree api;
    xpipe_ptr mpipe;
    __xthread_ptr task_pid;
    struct xmsgercb listener;
    
    xline_t msglist;
    struct avl_tree msgid_table;

}xltp_t;

static inline int msgid_comp(const void *a, const void *b)
{
	return ((xline_t*)a)->id - ((xline_t*)b)->id;
}

static inline int msgid_find(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xline_t*)b)->id;
}

// xmsg_ctx_t* xltp_make_req_ctx(xltp_t *xltp)
// {
//     xmsg_ctx_t* ctx = (xmsg_ctx_t*)malloc(sizeof(xmsg_ctx_t));
//     __xcheck(ctx == NULL);
//     ctx->xltp = xltp;
//     return ctx;
// XClean:
//     if (ctx){
//         free(ctx);
//     }
//     return NULL;
// }

xline_t* xltp_make_req(xltp_t *xltp, const char *api, xapi_cb_ptr cb)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    __xmsg_set_cb(msg, cb);
    msg->id = __atom_add(xltp->rid, 1);
    __xcheck(xl_add_word(&msg, "api", api) == EENDED);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == EENDED);
    return msg;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return NULL;
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
    xl_free(&msg);
}

static int on_message_timeout(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
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

static inline int recv_request(xltp_t *xltp, xline_t *msg)
{
    static char *api;
    static xapi_cb_ptr cb;
    xltp->parser = xl_parser(&msg->data);
    api = xl_find_word(&xltp->parser, "api");
    __xcheck(api == NULL);
    cb = xtree_find(xltp->api, api, slength(api));
    __xcheck(cb == NULL);
    cb(msg, xltp->ctx);
    return 0;
XClean:
    return -1;
}

static inline int recv_respnos(xltp_t *xltp, xline_t *msg)
{
    static uint64_t rid;
    static xline_t *req;
    static xapi_cb_ptr cb;
    xltp->parser = xl_parser(&msg->data);
    rid = xl_find_uint(&xltp->parser, "rid");
    __xcheck(rid == EENDED);
    req = xltp_find_req(xltp, rid);
    __xcheck(req == NULL);
    cb = __xmsg_get_cb(req);
    __xcheck(cb == NULL);
    cb(msg, xltp->ctx);
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

static inline int xltp_send(xltp_t *xltp, xline_t *msg)
{
    if (__xmsg_get_cb(msg) != NULL){
        xltp_add_req(xltp, msg);
    }
    if (msg->type == XPACK_TYPE_REQ){
        xmsger_connect(xltp->msger, msg, msg);
    }else if (msg->type == XPACK_TYPE_RES){
        xmsger_disconnect(xltp->msger, msg);
    }
    return 0;
}

static void xltp_loop(void *ptr)
{
    __xlogd("xltp_loop enter\n");

    xline_t *msg;
    xltp_t *xltp = (xltp_t*)ptr;

    while (xltp->runnig)
    {
        __xcheck(xpipe_read(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        if (msg->flag == XMSG_FLAG_RECV){

            xltp_recv(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_BACK){

            xltp_back(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_SEND){

            xltp_send(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_TIMEDOUT){

            xltp_timedout(xltp, msg);

        }else if(msg->flag == XMSG_FLAG_BOOT){

            if (__xmsg_get_cb(msg) != NULL){
                xltp_add_req(xltp, msg);
            }
            __xipaddr_ptr addr = __xapi->udp_host_to_addr("xltp.net", 9256);
            // __xapi->udp_addr_to_host(addr, xltp->ip, &xltp->port);
            // __xlogd("bootstrap %s:%u\n", xltp->ip, xltp->port);
            __xmsg_set_ipaddr(msg, addr);
            xmsger_connect(xltp->msger, msg, msg);
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
    msg->flag = XMSG_FLAG_SEND;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_respose(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_RES;
    msg->flag = XMSG_FLAG_SEND;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_bootstrap(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_REQ;
    msg->flag = XMSG_FLAG_BOOT;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_register(xltp_t *xltp, const char *api, xapi_cb_ptr cb)
{
    __xcheck(xltp == NULL || api == NULL || cb == NULL);
    __xcheck(xtree_add(xltp->api, (void*)api, slength(api), cb) == NULL);
    return 0;
XClean:
    return -1;
}

xltp_t* xltp_create(xapi_ctx_ptr ctx)
{
    xltp_t *xltp = (xltp_t*)calloc(1, sizeof(struct xltp));

    __set_true(xltp->runnig);
    xltp->ctx = ctx;
    xltp->api = xtree_create();
    __xcheck(xltp->api == NULL);

    xmsgercb_ptr listener = &xltp->listener;

    listener->ctx = xltp;
    listener->on_message_to_peer = on_message_to_peer;
    listener->on_message_from_peer = on_message_from_peer;
    listener->on_message_timeout = on_message_timeout;

    xltp->msglist.prev = &xltp->msglist;
    xltp->msglist.next = &xltp->msglist;

    avl_tree_init(&xltp->msgid_table, msgid_comp, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));

    xltp->mpipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xcheck(xltp->mpipe == NULL);

    xltp->task_pid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->task_pid == NULL);
    
    xltp->msger = xmsger_create(&xltp->listener, 9256);
    __xcheck(xltp->msger == NULL);

    return xltp;
XClean:
    xltp_free(&xltp);
    return NULL;
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
            xtree_clear(xltp->api, NULL);
            xtree_free(&xltp->api);
        }

        // if (xltp->addr){
        //     free(xltp->addr);
        // }

        free(xltp);
    }
}
