#include "xltp.h"
#include "xpeer.h"

#include "xapi/xapi.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xbuf.h>
#include <xlib/avlmini.h>

typedef struct xltp_io {
    __xfile_t fd;
    uint64_t pos, size;
    struct xltp *xltp;
    xchannel_ptr channel;
    struct xltp_io *prev, *next;
}xltp_io_t;

typedef struct xltp {
    uint16_t port;
    char ip[46];
    __atom_bool runnig;
    __atom_size rid;
    xline_t parser;
    xmsger_ptr msger;

    xtree api;
    xpipe_ptr msgpipe, iopipe;
    __xthread_ptr msg_tid, io_tid;
    struct xmsgercb listener;
    
    xline_t msglist;
    struct avl_tree msgid_table;

    xltp_io_t ctxlist;

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
    __xlogd("on_message_to_peer enter\n");
    msg->flag = XMSG_FLAG_BACK;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    __xlogd("on_message_to_peer exit\n");
    return 0;
XClean:
    __xlogd("on_message_to_peer failed\n");
    xl_free(&msg);
    return -1;
}

static int on_message_from_peer(xmsgercb_ptr cb, xchannel_ptr channel, xline_t *msg)
{
    msg->flag = XMSG_FLAG_RECV;
    __xmsg_set_channel(msg, channel);
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
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
    __xcheck(xpipe_write(((xltp_t*)(cb->ctx))->msgpipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
     if (msg != NULL){
        xl_free(&msg);
     }
    return -1;
}

static inline xltp_io_t* xltp_make_ctx(xltp_t *xltp, char *path)
{
    xltp_io_t* ctx = (xltp_io_t*)malloc(sizeof(xltp_io_t));
    __xcheck(ctx == NULL);
    ctx->xltp = xltp;
    ctx->channel = NULL;
    ctx->next = &xltp->ctxlist;
    ctx->prev = xltp->ctxlist.prev;
    ctx->next->prev = ctx;
    ctx->prev->next = ctx;
    ctx->fd = __xapi->fs_open(path, 0, 0644);
    __xcheck(ctx->fd < 0);
    ctx->pos = 0;
    ctx->size = __xapi->fs_tell(ctx->fd);
    __xlogd("file size = %lu\n", ctx->size);
    ctx->size = __xapi->fs_size(path);
    __xlogd("file size == %lu\n", ctx->size);
    return ctx;
XClean:
    if (ctx){
        free(ctx);
    }
    return NULL;
}

static inline void xltp_del_ctx(xmsgctx_ptr xltp)
{
    xltp_io_t *ctx = (xltp_io_t*)xltp;
    if (ctx){
        ctx->prev->next = ctx->next;
        ctx->next->prev = ctx->prev;
        if (ctx->fd > 0){
            __xapi->fs_close(ctx->fd);
        }
        free(ctx);
    }
}

static inline xline_t* xltp_make_req(xltp_t *xltp, xline_t *msg, const char *api, xmsgcb_ptr cb)
{
    // xmsg_ctx_t *ctx = xltp_make_ctx(xltp, cb, NULL);
    // __xcheck(ctx == NULL);
    // __xmsg_set_ctx(msg, ctx);

    __xmsg_set_cb(msg, cb);
    if (++xltp->rid == XEEND){
        xltp->rid++;
    }
    msg->id = xltp->rid;
    __xcheck(xl_add_word(&msg, "api", api) == XEEND);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XEEND);
    return msg;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return NULL;
}

int xltp_make_api(xltp_t *xltp, const char *api, xmsgcb_ptr cb)
{
    __xcheck(xtree_add(xltp->api, (void*)api, slength(api), cb) == NULL);
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
    xl_free(&msg);
}


static inline int xltp_send_req(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_REQ;
    xltp_add_req(xltp, msg);
    xmsger_connect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_send_res(xltp_t *xltp, xline_t *msg)
{
    msg->type = XPACK_TYPE_RES;
    xmsger_send(xltp->msger, __xmsg_get_channel(msg), msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_send_bye(xltp_t *xltp, xline_t *msg)
{
    __xcheck(xltp == NULL || msg == NULL);
    msg->type = XPACK_TYPE_BYE;
    xmsger_disconnect(xltp->msger, msg);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv_req(xltp_t *xltp, xline_t *msg)
{
    static char *api;
    static xmsgcb_ptr cb;
    xltp->parser = xl_parser(&msg->data);
    api = xl_find_word(&xltp->parser, "api");
    __xcheck(api == NULL);
    msg->id = xl_find_uint(&xltp->parser, "rid");
    __xcheck(msg->id == XEEND);
    cb = xtree_find(xltp->api, api, slength(api));
    __xcheck(cb == NULL);
    cb(msg, xltp);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv_res(xltp_t *xltp, xline_t *msg)
{
    static uint64_t rid;
    static xline_t *req;
    static xmsgcb_ptr cb;
    static xmsgctx_ptr ctx;
    xltp->parser = xl_parser(&msg->data);
    rid = xl_find_uint(&xltp->parser, "rid");
    __xcheck(rid == XEEND);
    req = xltp_find_req(xltp, rid);
    __xcheck(req == NULL);
    cb = __xmsg_get_cb(req);
    __xcheck(cb == NULL);
    // ctx = xchannel_get_ctx(__xmsg_get_channel(msg));
    // if(ctx != NULL){
    //     cb(msg, ctx);
    //     xltp_del_ctx(ctx);
    // }else {
    //     cb(msg, xltp);
    // }
    cb(msg, xltp);
    xltp_del_req(xltp, req);
    return 0;
XClean:
    return -1;
}

static inline int xltp_recv(xltp_t *xltp, xline_t *msg)
{
    if (msg->type == XPACK_TYPE_REQ){
        xltp_recv_req(xltp, msg);
    }else if (msg->type == XPACK_TYPE_BYE){
        if (msg->wpos > 0){
            xltp_recv_res(xltp, msg);
        }
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
        xl_free(&msg);
    }else if (msg->type == XPACK_TYPE_RES){
        xltp_recv_res(xltp, msg);
    }else if (msg->type == XPACK_TYPE_MSG){
        __xlogd("recv msg -----\n");
        xl_free(&msg);
    }
    return 0;
}

static inline int xltp_back(xltp_t *xltp, xline_t *msg)
{
    __xlogd("xltp_back enter\n");
    if (msg->type == XPACK_TYPE_REQ){
        
    }else if (msg->type == XPACK_TYPE_BYE){
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }else if (msg->type == XPACK_TYPE_MSG){
        xltp_io_t *io = xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(io == NULL);
        io->pos += msg->wpos;
        __xlogd("put file pos=%lu size=%lu\n", io->pos, io->size);
        if (io->pos == io->size){
            xl_clear(msg);
            xltp_send_bye(xltp, msg);
            xltp_del_ctx(io);
        }else {
            xl_hold(msg);
            msg->flag = XMSG_FLAG_STREAM;
            __xcheck(xpipe_write(xltp->iopipe, &msg, __sizeof_ptr) != __sizeof_ptr);
        }
    }
    xl_free(&msg);
    __xlogd("xltp_back exit\n");
    return 0;
XClean:
    __xlogd("xltp_back failed\n");
    return -1;
}

static inline int xltp_timedout(xltp_t *xltp, xline_t *msg)
{
    xmsgctx_ptr ctx = xchannel_get_ctx(__xmsg_get_channel(msg));
    xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    if (msg->id != XEEND){
        xline_t *req = xltp_find_req(xltp, msg->id);
        __xcheck(req == NULL);
        xltp_del_req(xltp, req);
    }
    if (ctx != NULL){
        xltp_del_ctx(ctx);
    }
    xl_free(&msg);
    return 0;
XClean:
    return -1;
}

static void xltp_loop(void *ptr)
{
    __xlogd("xltp_loop enter\n");

    xline_t *msg;
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
            __xcheck(cb(msg, xltp) != 0);

        }else if(msg->flag == XMSG_FLAG_TIMEDOUT){

            xltp_timedout(xltp, msg);
        }
    }

    __xlogd("xltp_loop >>>>---------------> exit\n");

XClean:

    return;
}

static void xltp_io_loop(void *ptr)
{
    __xlogd("xltp_io_loop enter\n");

    int rret = 0;
    uint64_t *flen = 1024 * 8;
    xline_t *msg;
    xline_t *frame;
    xmsgcb_ptr cb;
    xltp_io_t *io;
    xltp_t *xltp = (xltp_t*)ptr;

    while (xltp->runnig)
    {
        __xcheck(xpipe_read(xltp->iopipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        io = xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(io == NULL);

        if (msg->flag == XMSG_FLAG_READY){

            

        }else if(msg->flag == XMSG_FLAG_STREAM){

            if (msg->size == flen){
                frame = msg;
            }else {
                xl_free(&msg);
                frame = xl_creator(flen);
                __xcheck(frame == NULL);
            }

            rret = __xapi->fs_read(io->fd, frame->ptr, flen);
            if (rret > 0){
                frame->wpos = rret;
                frame->type = XPACK_TYPE_MSG;
                xmsger_send(io->xltp->msger, __xmsg_get_channel(msg), frame);
            }else {
                // __xapi->fs_close(io->fd);
                // xltp_send_bye(io->xltp, frame);
            }
        }
    }

    __xlogd("xltp_io_loop >>>>---------------> exit\n");

XClean:

    return;
}

////////////////////////////////////////////////////////////////////
//// 
////////////////////////////////////////////////////////////////////

static int api_echo(xline_t *msg, xltp_t *xltp)
{
    // __xcheck(msg == NULL);
    // __xcheck(ctx == NULL);
    xl_printf(&msg->data);
    xl_clear(msg);
    xl_add_uint(&msg, "rid", msg->id);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XEEND);
    __xcheck(xl_add_uint(&msg, "port", port) == XEEND);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);
    xl_add_uint(&msg, "code", 200);
    xltp_send_bye(xltp, msg);
    return 0;
XClean:
    return -1;
}

static int res_echo(xline_t *res, xltp_t *xltp)
{
    xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
XClean:
    return -1;
}

static int req_echo(xline_t *msg, xltp_t *xltp)
{
    msg = xltp_make_req(xltp, msg, "echo", res_echo);
    __xcheck(msg == NULL);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);    
    __xcheck(xltp_send_req(xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

int xltp_echo(xltp_t *xltp, const char *ip, uint16_t port)
{
    xline_t *msg = xl_maker();
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

static int api_boot(xline_t *msg, xltp_t *xltp)
{
    // __xcheck(msg == NULL);
    // __xcheck(ctx == NULL);
    xl_printf(&msg->data);
    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XEEND);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XEEND);
    __xcheck(xl_add_uint(&msg, "port", port) == XEEND);
    uint8_t uuid[32];
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XEEND);
    __xcheck(xl_add_uint(&msg, "code", 200) == XEEND);
    __xcheck(xltp_send_bye(xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int res_boot(xline_t *res, xltp_t *xltp)
{
    xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
XClean:
    return -1;
}

static int req_boot(xline_t *msg, xltp_t *xltp)
{
    msg = xltp_make_req(xltp, msg, "boot", res_boot);
    __xcheck(msg == NULL);
    __xipaddr_ptr addr = __xapi->udp_host_to_addr("xltp.net", 9256);
    // __xipaddr_ptr addr = __xapi->udp_host_to_addr("192.168.1.7", 9256);
    __xmsg_set_ipaddr(msg, addr);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->data);
    xl_free(&test);
    __xcheck(xltp_send_req(xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

static inline int xltp_bootstrap(xltp_t *xltp)
{
    xline_t *msg = xl_maker();
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

static int recv_put(xline_t *msg, xltp_io_t *put)
{
    return 0;
}

static int send_put(xline_t *msg, xltp_io_t *put)
{
    put->xltp->parser = xl_parser(&msg->data);
    xl_printf(&msg->data);
    return 0;
}

static int api_put(xline_t *msg, xltp_t *xltp)
{
    // __xcheck(msg == NULL);
    // __xcheck(ctx == NULL);
    xl_printf(&msg->data);
    // xltp_put_t *put = xltp_make_ctx(xltp, __xmsg_get_channel(msg), recv_put, msg->id);
    // __xcheck(put == NULL);
    // xchannel_set_ctx(__xmsg_get_channel(msg), put);
    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XEEND);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XEEND);
    __xcheck(xl_add_uint(&msg, "port", port) == XEEND);
    uint8_t uuid[32];
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XEEND);
    __xcheck(xl_add_uint(&msg, "code", 200) == XEEND);
    xltp_send_res(xltp, msg);
    // xmsger_send(xltp->msger, __xmsg_get_channel(msg), msg);
    // __xcheck(xltp_respose(xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int res_put(xline_t *res, xltp_t *xltp)
{
    xltp_io_t *ctx = (xltp_io_t *)xchannel_get_ctx(__xmsg_get_channel(res));
    xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    // xl_clear(res);
    // xltp_send_bye(xltp, res);
    // xltp_del_ctx(ctx);
    res->flag = XMSG_FLAG_STREAM;
    __xcheck(xpipe_write(xltp->iopipe, &res, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

static inline void path_clear(const char *path, uint8_t path_len, char *name, uint8_t name_len)
{
    int len = 0;
    while (len < path_len && path[path_len-len-1] != '/'){
        len++;
    }
    for (size_t i = 0; i < name_len && i < len; i++){
        name[i] = path[path_len-(len-i)];
    }
}

static int req_put(xline_t *msg, xltp_t *xltp)
{
    xltp->parser = xl_parser(&msg->data);
    const char *file = xl_find_word(&xltp->parser, "file");
    __xcheck(file == NULL);
    xltp_io_t *ctx = xltp_make_ctx(xltp, file);
    __xcheck(ctx == NULL);
    __xmsg_set_ctx(msg, ctx);
    __xcheck(!__xapi->fs_isfile(file));
    int64_t len = __xapi->fs_size(file);
    uint64_t filelen = __xl_sizeof_body(xltp->parser.val) - 1;
    const char *path = xl_find_word(&xltp->parser, "path");
    __xcheck(path == NULL);
    uint64_t pathlen = __xl_sizeof_body(xltp->parser.val) - 1;
    char name[64] = {0};
    path_clear(file, filelen, name, 64);
    char spath[256] = {0};
    mcopy(spath, path, pathlen);
    xl_clear(msg);
    msg = xltp_make_req(xltp, msg, "put", res_put);
    __xcheck(msg == NULL);
    xl_add_word(&msg, "name", name);
    xl_add_word(&msg, "path", spath);
    xl_add_int(&msg, "len", len);
    __xcheck(xltp_send_req(xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

int xltp_put(xltp_t *xltp, const char *file, const char *path, const char *ip, uint16_t port)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
    __xmsg_set_ipaddr(msg, addr);
    __xmsg_set_cb(msg, req_put);
    xl_add_word(&msg, "file", file);
    xl_add_word(&msg, "path", path);
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

    xltp->msglist.prev = &xltp->msglist;
    xltp->msglist.next = &xltp->msglist;

    xltp->ctxlist.prev = &xltp->ctxlist;
    xltp->ctxlist.next = &xltp->ctxlist;

    avl_tree_init(&xltp->msgid_table, msgid_comp, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));

    xltp->msgpipe = xpipe_create(sizeof(void*) * 1024, "MSG PIPE");
    __xcheck(xltp->msgpipe == NULL);

    xltp->msg_tid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->msg_tid == NULL);
    
    xltp->iopipe = xpipe_create(sizeof(void*) * 1024, "IO PIPE");
    __xcheck(xltp->iopipe == NULL);

    xltp->io_tid = __xapi->thread_create(xltp_io_loop, xltp);
    __xcheck(xltp->io_tid == NULL);

    xltp->msger = xmsger_create(&xltp->listener, 9256);
    __xcheck(xltp->msger == NULL);

    xltp->api = xtree_create();
    __xcheck(xltp->api == NULL);
    __xcheck(xltp_make_api(xltp, "echo", api_echo) != 0);
    __xcheck(xltp_make_api(xltp, "boot", api_boot) != 0);
    __xcheck(xltp_make_api(xltp, "put", api_put) != 0);

    if (boot){
        xltp_bootstrap(xltp);
    }

    return xltp;
XClean:
    xltp_free(&xltp);
    return NULL;
}

// static void xapi_clear(void *xapi)
// {
//     if (xapi){
//         free(xapi);
//     }
// }

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

        if (xltp->iopipe){
            xpipe_break(xltp->iopipe);
        }

        if (xltp->msg_tid){
            __xapi->thread_join(xltp->msg_tid);
        }

        if (xltp->io_tid){
            __xapi->thread_join(xltp->io_tid);
        }

        if (xltp->msgpipe){
            __xlogi("pipe readable = %u\n", xpipe_readable(xltp->msgpipe));
            xline_t *msg;
            while (__xpipe_read(xltp->msgpipe, &msg, __sizeof_ptr) == __sizeof_ptr){
                __xlogi("free msg\n");
                xl_free(&msg);
            }
            xpipe_free(&xltp->msgpipe);
        }

        if (xltp->iopipe){
            __xlogi("pipe readable = %u\n", xpipe_readable(xltp->iopipe));
            xline_t *msg;
            while (__xpipe_read(xltp->iopipe, &msg, __sizeof_ptr) == __sizeof_ptr){
                __xlogi("free msg\n");
                xl_free(&msg);
            }
            xpipe_free(&xltp->iopipe);
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
