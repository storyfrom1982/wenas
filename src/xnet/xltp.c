#include "xltp.h"

#include "xapi/xapi.h"

#include "xlio.h"

#include <xnet/xtree.h>
#include <xnet/xmsger.h>
#include <xnet/xpipe.h>
#include <xlib/avlmini.h>

// typedef struct xltp_io {
//     __xfile_t fd;
//     uint64_t pos, size;
//     struct xltp *xltp;
//     xchannel_ptr channel;
//     struct xltp_io *prev, *next;
// }xltp_io_t;

typedef struct xltp {
    uint16_t port;
    char ip[46];
    __atom_bool runnig;
    __atom_size rid;
    xline_t parser;
    xmsger_ptr msger;
    xlio_t *io;

    xtree api;
    xpipe_ptr msgpipe;
    __xthread_ptr msg_tid;
    struct xmsgercb listener;
    
    xline_t msglist;
    struct avl_tree msgid_table;

    // xltp_io_t ctxlist;

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

// static inline xltp_io_t* xltp_make_ctx(xltp_t *xltp, char *path)
// {
//     xltp_io_t* ctx = (xltp_io_t*)malloc(sizeof(xltp_io_t));
//     __xcheck(ctx == NULL);
//     ctx->xltp = xltp;
//     ctx->channel = NULL;
//     ctx->next = &xltp->ctxlist;
//     ctx->prev = xltp->ctxlist.prev;
//     ctx->next->prev = ctx;
//     ctx->prev->next = ctx;
//     ctx->fd = __xapi->fs_open(path, 0, 0644);
//     __xcheck(ctx->fd < 0);
//     ctx->pos = 0;
//     ctx->size = __xapi->fs_tell(ctx->fd);
//     __xlogd("file size = %lu\n", ctx->size);
//     ctx->size = __xapi->fs_size(path);
//     __xlogd("file size == %lu\n", ctx->size);
//     return ctx;
// XClean:
//     if (ctx){
//         free(ctx);
//     }
//     return NULL;
// }

// static inline void xltp_del_ctx(xmsgctx_ptr xltp)
// {
//     xltp_io_t *ctx = (xltp_io_t*)xltp;
//     if (ctx){
//         ctx->prev->next = ctx->next;
//         ctx->next->prev = ctx->prev;
//         if (ctx->fd > 0){
//             __xapi->fs_close(ctx->fd);
//         }
//         free(ctx);
//     }
// }

static inline xline_t* xltp_make_req(xltp_t *xltp, xline_t *msg, const char *api, xmsgcb_ptr cb)
{
    // xmsg_ctx_t *ctx = xltp_make_ctx(xltp, cb, NULL);
    // __xcheck(ctx == NULL);
    // __xmsg_set_ctx(msg, ctx);

    __xmsg_set_cb(msg, cb);
    if (++xltp->rid == XNONE){
        xltp->rid++;
    }
    msg->id = xltp->rid;
    __xcheck(xl_add_word(&msg, "api", api) == XNONE);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XNONE);
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
    __xcheck(msg->id == XNONE);
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
    xltp->parser = xl_parser(&msg->data);
    rid = xl_find_uint(&xltp->parser, "rid");
    __xcheck(rid == XNONE);
    req = xltp_find_req(xltp, rid);
    __xcheck(req == NULL);
    cb = __xmsg_get_cb(req);
    __xcheck(cb == NULL);
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
        }else {
            xl_free(&msg);
        }
        xlio_stream_t *ios = xchannel_get_ctx(__xmsg_get_channel(msg));
        if (ios != NULL){
            xlio_stream_free(ios);
        }        
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }else if (msg->type == XPACK_TYPE_RES){
        xltp_recv_res(xltp, msg);
        xl_free(&msg);
    }else if (msg->type == XPACK_TYPE_MSG){
        __xlogd("recv msg -----\n");
        xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(ios == NULL);
        __xcheck(xlio_stream_write(ios, msg) != 0);
    }
    return 0;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return -1;
}

static inline int xltp_back(xltp_t *xltp, xline_t *msg)
{
    __xlogd("xltp_back enter\n");
    if (msg->type == XPACK_TYPE_REQ){
        
    }else if (msg->type == XPACK_TYPE_BYE){
        xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    }else if (msg->type == XPACK_TYPE_MSG){
        xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(ios == NULL);
        // ios->pos += msg->wpos;
        // __xlogd("put file pos=%lu size=%lu\n", ios->pos, ios->size);
        if (xlio_stream_update(ios, msg->wpos) == 0){
            xl_clear(msg);
            xltp_send_bye(xltp, msg);
            xlio_stream_free(ios);
            // xltp_del_ctx(ios);
        }else {
            xl_hold(msg);
            xlio_stream_read(ios, msg);
            // msg->flag = XMSG_FLAG_STREAM;
            // __xcheck(xpipe_write(xltp->iopipe, &msg, __sizeof_ptr) != __sizeof_ptr);
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
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(msg));
    xmsger_flush(xltp->msger, __xmsg_get_channel(msg));
    if (msg->id != XNONE){
        xline_t *req = xltp_find_req(xltp, msg->id);
        __xcheck(req == NULL);
        xltp_del_req(xltp, req);
    }
    if (ios != NULL){
        xlio_stream_free(ios);
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
    __xcheck(xl_add_word(&msg, "ip", ip) == XNONE);
    __xcheck(xl_add_uint(&msg, "port", port) == XNONE);
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
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XNONE);
    const char *ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    uint16_t port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XNONE);
    __xcheck(xl_add_uint(&msg, "port", port) == XNONE);
    uint8_t uuid[32];
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XNONE);
    __xcheck(xl_add_uint(&msg, "code", 200) == XNONE);
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

static int api_put(xline_t *msg, xltp_t *xltp)
{
    int n;
    const char *ip;
    uint16_t port;
    uint8_t uuid[32];
    const char *file;
    const char *dir;
    char file_path[2048];
    xlio_stream_t *ios;

    xl_printf(&msg->data);
    xltp->parser = xl_parser(&msg->data);
    file = xl_find_word(&xltp->parser, "file");
    __xcheck(file == NULL);
    dir = xl_find_word(&xltp->parser, "dir");
    __xcheck(dir == NULL);
    if (!__xapi->fs_isdir(dir)){
        __xcheck(__xapi->fs_mkpath(dir) != 0);
    }
    
    n = __xapi->snprintf(file_path, 2048, "%s/%s", dir, file);
    file_path[n] = '\0';
    ios = xlio_stream_maker(xltp->io, file_path, XAPI_FS_FLAG_CREATE);
    __xcheck(ios == NULL);
    xchannel_set_ctx(__xmsg_get_channel(msg), ios);

    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XNONE);
    ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XNONE);
    __xcheck(xl_add_uint(&msg, "port", port) == XNONE);
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XNONE);
    __xcheck(xl_add_uint(&msg, "code", 200) == XNONE);
    xltp_send_res(xltp, msg);
    return 0;

XClean:
    if (ios != NULL){
        xlio_stream_free(ios);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

static int res_put(xline_t *res, xltp_t *xltp)
{
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(res));
    __xcheck(ios == NULL);
    xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    xlio_stream_ready(ios, res);
    return 0;
XClean:
    return -1;
}

// static inline void path_clear(const char *file_path, uint8_t path_len, char *file_name, uint16_t name_len)
// {
//     int len = 0;
//     while (len < path_len && file_path[path_len-len-1] != '/'){
//         len++;
//     }
//     for (size_t i = 0; i < name_len && i < len; i++){
//         file_name[i] = file_path[path_len-(len-i)];
//     }
//     file_name[len] = '\0';
// }

static int req_put(xline_t *msg, xltp_t *xltp)
{
    const char *remote_dir;
    const char *file_path;
    const char *file_name;
    uint64_t file_path_len;
    xlio_stream_t *ios;
    xline_t *req = xl_maker();
    __xcheck(req == NULL);

    xltp->parser = xl_parser(&msg->data);
    file_path = xl_find_word(&xltp->parser, "file");
    __xcheck(file_path == NULL);
    file_path_len = __xl_sizeof_body(xltp->parser.val) - 1;
    remote_dir = xl_find_word(&xltp->parser, "dir");
    __xcheck(remote_dir == NULL);

    ios = xlio_stream_maker(xltp->io, file_path, XAPI_FS_FLAG_READ);
    __xcheck(ios == NULL);

    file_name = file_path + file_path_len;
    while (file_name > file_path && *(file_name-1) != '/'){
        file_name--;
    }

    req = xltp_make_req(xltp, req, "put", res_put);
    __xcheck(req == NULL);
    __xmsg_set_ctx(req, ios);
    __xmsg_set_ipaddr(req, __xmsg_get_ipaddr(msg));

    xl_add_word(&req, "file", file_name);
    xl_add_word(&req, "dir", remote_dir);
    xl_add_int(&req, "len", xlio_stream_length(ios));
    __xcheck(xltp_send_req(xltp, req) != 0);
    xl_free(&msg);
    return 0;

XClean:
    if (ios != NULL){
        xlio_stream_free(ios);
    }
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xltp_put(xltp_t *xltp, const char *local_file_path, const char *remote_directory, const char *ip, uint16_t port)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
    __xmsg_set_ipaddr(msg, addr);
    __xmsg_set_cb(msg, req_put);
    xl_add_word(&msg, "file", local_file_path);
    xl_add_word(&msg, "dir", remote_directory);
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

static int api_get(xline_t *msg, xltp_t *xltp)
{
    int n;
    const char *ip;
    uint16_t port;
    uint8_t uuid[32];
    const char *file_path;
    xlio_stream_t *ios;
    xline_t *ready = xl_maker();
    __xcheck(ready == NULL);

    for (size_t i = 0; i < 4; i++)
    {
        ready->args[i] = msg->args[i];
    }

    xl_printf(&msg->data);
    xltp->parser = xl_parser(&msg->data);
    file_path = xl_find_word(&xltp->parser, "file");
    __xcheck(file_path == NULL);
    __xcheck(!__xapi->fs_isfile(file_path));
    
    ios = xlio_stream_maker(xltp->io, file_path, XAPI_FS_FLAG_READ);
    __xcheck(ios == NULL);
    xchannel_set_ctx(__xmsg_get_channel(msg), ios);

    xl_clear(msg);
    __xmsg_set_ctx(msg, NULL);
    __xcheck(xl_add_uint(&msg, "rid", msg->id) == XNONE);
    ip = __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg));
    port = __xapi->udp_addr_port(__xmsg_get_ipaddr(msg));
    __xcheck(xl_add_word(&msg, "ip", ip) == XNONE);
    __xcheck(xl_add_uint(&msg, "port", port) == XNONE);
    __xcheck(xl_add_bin(&msg, "uuid", uuid, 32) == XNONE);
    __xcheck(xl_add_uint(&msg, "code", 200) == XNONE);
    xltp_send_res(xltp, msg);

    xlio_stream_ready(ios, ready);
    return 0;

XClean:
    if (ios != NULL){
        xlio_stream_free(ios);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    if (ready != NULL){
        xl_free(&ready);
    }
    return -1;
}

static int res_get(xline_t *res, xltp_t *xltp)
{
    xlio_stream_t *ios = (xlio_stream_t*)xchannel_get_ctx(__xmsg_get_channel(res));
    __xcheck(ios == NULL);
    xltp->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    xl_free(&res);
    return 0;
XClean:
    return -1;
}

static int req_get(xline_t *msg, xltp_t *xltp)
{
    int n;
    const char *local_dir;
    const char *remote_path;
    const char *file_name;
    uint64_t file_path_len;
    xlio_stream_t *ios;
    char file_path[2048];
    xline_t *req = xl_maker();
    __xcheck(req == NULL);

    xltp->parser = xl_parser(&msg->data);
    remote_path = xl_find_word(&xltp->parser, "file");
    __xcheck(remote_path == NULL);
    file_path_len = __xl_sizeof_body(xltp->parser.val) - 1;

    local_dir = xl_find_word(&xltp->parser, "dir");
    __xcheck(local_dir == NULL);
    if (!__xapi->fs_isdir(local_dir)){
        __xcheck(__xapi->fs_mkpath(local_dir) != 0);
    }

    file_name = remote_path + file_path_len;
    while (file_name > remote_path && *(file_name-1) != '/'){
        file_name--;
    }

    n = __xapi->snprintf(file_path, 2048, "%s/%s", local_dir, file_name);
    file_path[n] = '\0';
    ios = xlio_stream_maker(xltp->io, file_path, XAPI_FS_FLAG_CREATE);
    __xcheck(ios == NULL);

    req = xltp_make_req(xltp, req, "get", res_get);
    __xcheck(req == NULL);
    __xmsg_set_ctx(req, ios);
    __xmsg_set_ipaddr(req, __xmsg_get_ipaddr(msg));

    xl_add_word(&req, "file", remote_path);
    xl_add_int(&req, "len", xlio_stream_length(ios));
    __xcheck(xltp_send_req(xltp, req) != 0);
    xl_free(&msg);
    return 0;

XClean:
    if (ios != NULL){
        xlio_stream_free(ios);
    }
    if (req != NULL){
        xl_free(&req);
    }
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xltp_get(xltp_t *xltp, const char *remote_file_path, const char *local_directory, const char *ip, uint16_t port)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
    __xmsg_set_ipaddr(msg, addr);
    __xmsg_set_cb(msg, req_get);
    xl_add_word(&msg, "file", remote_file_path);
    xl_add_word(&msg, "dir", local_directory);
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

    avl_tree_init(&xltp->msgid_table, msgid_comp, msgid_find, sizeof(xline_t), AVL_OFFSET(xline_t, node));

    xltp->msgpipe = xpipe_create(sizeof(void*) * 1024, "MSG PIPE");
    __xcheck(xltp->msgpipe == NULL);

    xltp->msg_tid = __xapi->thread_create(xltp_loop, xltp);
    __xcheck(xltp->msg_tid == NULL);
    
    // xltp->iopipe = xpipe_create(sizeof(void*) * 1024, "IO PIPE");
    // __xcheck(xltp->iopipe == NULL);

    // xltp->io_tid = __xapi->thread_create(xlio_loop, xltp);
    // __xcheck(xltp->io_tid == NULL);

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

        if (xltp->msg_tid){
            __xapi->thread_join(xltp->msg_tid);
        }

        if(xltp->io){
            xlio_free(&xltp->io);
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

        free(xltp);
    }
}
