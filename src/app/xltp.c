#include "xltp.h"

#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xlib/avlmini.h>


typedef struct xltp {
    uint16_t port;
    char ip[46];
    __atom_bool runnig;
    xpeer_t *peer;
    xline_t parser;
    __xipaddr_ptr addr;
    xmsger_ptr msger;

    xtree api;
    xpipe_ptr mpipe;
    __xthread_ptr task_pid;
    struct xmsgercb listener;
    
    xline_t msglist;
    struct avl_tree msgid_table;

}xltp_t;

static inline int msgid_compare(const void *a, const void *b)
{
	return ((xline_t*)a)->id - ((xline_t*)b)->id;
}

static inline int msgid_find(const void *a, const void *b)
{
	return (*(uint64_t*)a) - ((xline_t*)b)->id;
}

inline static void xpeer_add_msg_cb(xltp_t *xltp, xline_t *msg)
{
    xl_hold(msg);
    msg->next = xltp->msglist.next;
    msg->prev = &xltp->msglist;
    msg->prev->next = msg;
    msg->next->prev = msg;
    avl_tree_add(&xltp->msgid_table, msg);
}

inline static void xpeer_del_msg_cb(xltp_t *xltp, xline_t *msg)
{
    avl_tree_remove(&xltp->msgid_table, msg);
    msg->prev->next = msg->next;
    msg->next->prev = msg->prev;
    xl_free(&msg);
}

static int on_message_timeout(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    xltp_t *ctx = (xltp_t *)xchannel_get_ctx(channel);
    if (ctx){
        if (msg != NULL){
            msg->flag = XL_MSG_FLAG_TIMEOUT;
            __xmsg_set_channel(msg, channel);
        }
        __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    }
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_to_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XL_MSG_FLAG_BACK;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xl_free(&msg);
    return -1;
}

static int on_message_from_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XL_MSG_FLAG_RECV;
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
    static api_cb_t cb;
    xltp->parser = xl_parser(&msg->data);
    xl_printf(&msg->data);
    api = xl_find_word(&xltp->parser, "api");
    __xcheck(api == NULL);
    cb = xtree_find(xltp->api, api, slength(api));
    if (cb){
        cb(msg);
    }
    return 0;
XClean:
    return -1;
}

static inline int recv_respnos(xltp_t *xltp, xline_t *msg)
{
    xl_printf(&msg->data);
    xltp->parser = xl_parser(&msg->data);
    uint64_t tid = xl_find_uint(&xltp->parser, "mid");
    xline_t *req = (xline_t*)avl_tree_find(&xltp->msgid_table, &tid);
    if (req && __xmsg_get_cb(req)){
        ((msg_cb_t)(__xmsg_get_cb(req)))(msg);
        __xmsg_set_channel(req, __xmsg_get_channel(msg));
        xpeer_del_msg_cb(xltp, req);
    }
    return 0;
XClean:
    return -1;
}

static void xltp_loop(void *ptr)
{
    __xlogd("xltp_loop enter\n");

    xline_t *msg;
    xltp_t *xltp = (xltp_t*)ptr;

    while (xltp->runnig)
    {
        __xcheck(xpipe_read(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        if (msg->flag == XL_MSG_FLAG_RECV){

            __xmsg_set_peer(msg, xltp->peer);
            __xmsg_set_xltp(msg, xltp);

            if (msg->type == XPACK_TYPE_REQ){
                recv_request(xltp, msg);
            }else if (msg->type == XPACK_TYPE_RES){
                recv_respnos(xltp, msg);
                xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
            }
            xl_free(&msg);

        }else if(msg->flag == XL_MSG_FLAG_BACK){

            if (msg->type == XPACK_TYPE_REQ){
                xchannel_set_ctx(__xmsg_get_channel(msg), __xmsg_get_xltp(msg));
            }else if (msg->type == XPACK_TYPE_RES){
                xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
            }
            xl_free(&msg);

        }else if(msg->flag == XL_MSG_FLAG_TIMEOUT){

        }else if(msg->flag == XL_MSG_FLAG_SEND){

            if (__xmsg_get_cb(msg) != NULL){
                xpeer_add_msg_cb(xltp, msg);
                // 不允许在 xpeer_add_msg_cb 之后，再追加任何字段
                // xl_add_uint(&msg, "mid", msg->id);
            }
            if (msg->type == XPACK_TYPE_REQ){
                xmsger_connect(xltp->msger, msg);
            }else if (msg->type == XPACK_TYPE_RES){
                xmsger_disconnect(xltp->msger, msg);
            }

        }else if(msg->flag == XL_MSG_FLAG_BOOT){

            xltp->addr = __xapi->udp_host_to_addr("xltp.net", 9256);
            __xapi->udp_addr_to_host(xltp->addr, xltp->ip, &xltp->port);
            __xlogd("bootstrap %s:%u\n", xltp->ip, xltp->port);
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
    msg->flag = XL_MSG_FLAG_SEND;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_respose(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_RES;
    msg->flag = XL_MSG_FLAG_SEND;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_bootstrap(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_REQ;
    msg->flag = XL_MSG_FLAG_BOOT;
    __xcheck(xpipe_write(xltp->mpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xltp_register(xltp_t *xltp, const char *api, api_cb_t cb)
{
    __xcheck(xltp == NULL || api == NULL || cb == NULL);
    __xcheck(xtree_add(xltp->api, (void*)api, slength(api), cb) == NULL);
    return 0;
XClean:
    return -1;
}

xltp_t* xltp_create(xpeer_t *peer)
{
    xltp_t *xltp = (xltp_t*)calloc(1, sizeof(struct xltp));

    __set_true(xltp->runnig);
    xltp->peer = peer;
    xltp->api = xtree_create();
    __xcheck(xltp->api == NULL);

    xmsgercb_ptr listener = &xltp->listener;

    listener->ctx = xltp;
    listener->on_message_to_peer = on_message_to_peer;
    listener->on_message_from_peer = on_message_from_peer;
    listener->on_message_timeout = on_message_timeout;

    xltp->msglist.prev = &xltp->msglist;
    xltp->msglist.next = &xltp->msglist;

    avl_tree_init(&xltp->msgid_table, msgid_compare, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));

    xltp->mpipe = xpipe_create(sizeof(void*) * 1024, "RECV PIPE");
    __xcheck(xltp->mpipe == NULL);

    xltp->task_pid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->task_pid == NULL);
    
    xltp->msger = xmsger_create(&xltp->listener, 1, 9256);
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

        avl_tree_clear(&xltp->msgid_table, NULL);

        if (xltp->api){
            xtree_clear(xltp->api, NULL);
            xtree_free(&xltp->api);
        }

        free(xltp);
    }
}
