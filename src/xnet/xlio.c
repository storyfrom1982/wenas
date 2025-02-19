#include "xlio.h"

#include "xpipe.h"

#define MSGBUF_RANGE    2
#define MSGBUF_SIZE     1280 * 1024 //1.25MB
// #define MSGBUF_SIZE     1280 * 16

typedef struct xmsgbuf {
    uint8_t range, spos, rpos, wpos;
    xframe_t *buf[MSGBUF_RANGE];
}*xmsgbuf_ptr;

#define __serialbuf_wpos(b)         ((b)->wpos & ((b)->range - 1))
#define __serialbuf_rpos(b)         ((b)->rpos & ((b)->range - 1))
#define __serialbuf_spos(b)         ((b)->spos & ((b)->range - 1))

#define __serialbuf_recvable(b)     ((uint8_t)((b)->spos - (b)->rpos))
#define __serialbuf_sendable(b)     ((uint8_t)((b)->wpos - (b)->spos))

#define __serialbuf_readable(b)     ((uint8_t)((b)->wpos - (b)->rpos))
#define __serialbuf_writable(b)     ((uint8_t)((b)->range - (b)->wpos + (b)->rpos))

typedef struct xlio_stream {
    int flag;
    int64_t api;
    __xfile_t fd;
    // int is_dir;
    // int is_resend;
    xline_t *obj;
    xframe_t *upload_list, *recv_frame;
    xframe_t upload_parser;
    uint64_t file_pos, file_size;
    uint64_t list_pos, list_size;
    char uri[2048];
    int uri_len;
    int src_name_len;
    int src_name_pos;
    struct xlio *io;
    xchannel_ptr channel;
    __xfs_item_ptr item;
    __xfs_scanner_ptr scanner;
    struct xlio_stream *prev, *next;
    struct xmsgbuf buf;
}xlio_stream_t;


typedef struct xlio {
    __atom_bool running;
    xmsger_ptr msger;
    xpipe_ptr pipe;
    __xthread_ptr tid;
    xlio_stream_t streamlist;
}xlio_t;

#define XLIO_STREAM_REQ_LIST        0
#define XLIO_STREAM_RES_LIST        1
#define XLIO_STREAM_DOWNLOAD_LIST   2
#define XLIO_STREAM_UPLOAD_LIST     3


static inline xframe_t* xlio_take_frame(xlio_stream_t *stream)
{
    xframe_t *frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
    frame->id = __serialbuf_rpos(&stream->buf);
    stream->buf.buf[frame->id] = NULL;
    stream->buf.rpos++;
    return frame;
}

static inline void xlio_resave_frame(xlio_stream_t *stream, xframe_t *frame)
{
    xl_clear(frame);
    stream->buf.buf[frame->id] = frame;
    stream->buf.wpos++;
}

static inline int xlio_send_file(xlio_stream_t *stream, xframe_t **frame)
{
    char *name;
    int64_t isfile;
    xline_t *name_val;
    xframe_t parser;

    __xcheck(xl_add_int(frame, "api", XLIO_STREAM_UPLOAD_LIST) == XEOF);
    __xcheck(xl_add_int(frame, "id", (*frame)->id) == XEOF);
    uint64_t list_begin_pos = xl_list_begin(frame, "list");
    __xcheck(list_begin_pos == XEOF);

    do {

        if ((stream->obj != NULL)){

            xl_printf(stream->obj);

            if (xl_usable(*frame, "pos") <= __xl_sizeof_line(stream->obj)){
                break;
            }
            
            parser = xl_parser(stream->obj);
            __xcheck((name_val = xl_find_word(&parser, "path", &name)) == NULL);
            __xcheck(xl_find_int(&parser, "type", &isfile) == NULL);
            __xcheck(xl_find_uint(&parser, "size", &stream->file_size) == NULL);

            if (isfile){

                uint64_t obj_begin_pos = xl_obj_begin(frame, NULL);
                __xcheck(obj_begin_pos == XEOF);

                __xcheck(xl_add_word(frame, "path", name) == XEOF);
                __xcheck(xl_add_int(frame, "type", isfile) == XEOF);
                __xcheck(xl_add_uint(frame, "size", stream->file_size) == XEOF);
                __xcheck(xl_add_uint(frame, "pos", stream->file_pos) == XEOF);

                if (stream->fd == -1){
                    if (__xl_sizeof_body(name_val) == stream->src_name_len + 1){
                        // 是单个文件
                        stream->fd = __xapi->fs_file_open(stream->uri, XAPI_FS_FLAG_READ, 0644);
                        __xcheck(stream->fd < 0);
                    }else {
                        // 是整个目录
                        int full_path_len = stream->uri_len + 1 + __xl_sizeof_body(name_val);
                        char full_path[full_path_len];                        
                        __xapi->snprintf(full_path, full_path_len, "%s/%s", stream->uri, name + (stream->src_name_len + 1));
                        __xlogd("upload file %s\n", full_path);
                        stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_READ, 0644);
                        __xcheck(stream->fd < 0);
                    }
                }

                if (stream->file_size > 0){
                    uint64_t len = xl_usable(*frame, "data");
                    if (stream->file_size - stream->file_pos < len){
                        len = stream->file_size - stream->file_pos;
                    }
                    uint64_t pos = xl_add_bin(frame, "data", NULL, len);
                    int ret = __xapi->fs_file_read(stream->fd, (*frame)->ptr + (pos - len), len);
                    __xcheck(ret < 0);
                    stream->file_pos += len;
                    stream->list_pos += len;
                    __xlogd("-------------list size= %lu list pos = %lu\n", stream->list_size, stream->list_pos);
                    __xlogd("-------------file size= %lu file pos = %lu\n", stream->file_size, stream->file_pos);
                    if (stream->file_pos < stream->file_size){
                        xl_obj_end(frame, obj_begin_pos);
                        break;
                    }
                }

                if (stream->file_pos == stream->file_size){
                    __xapi->fs_file_close(stream->fd);
                    stream->fd = -1;
                    stream->file_pos = 0;
                    stream->file_size = 0;
                    stream->obj = NULL;
                }

                xl_obj_end(frame, obj_begin_pos);
            }
        }
        
    }while ((stream->obj = xl_list_next(&stream->upload_parser)) != NULL);

    xl_list_end(frame, list_begin_pos);
    
    (*frame)->type = XPACK_TYPE_MSG;
    __xcheck(xmsger_send(stream->io->msger, stream->channel, *frame) != 0);

    __xlogd("xlio_send_file >>>>---------------> exit\n");
        
    return 0;
XClean:
    return -1;
}

static inline int xlio_recv_file(xlio_stream_t *stream, xframe_t *msg)
{
    char *name;
    int64_t isfile;
    uint64_t pos;
    xline_t *obj_val, *name_val;
    xframe_t list_parser, obj_parser;
    xframe_t *frame = NULL;

    __xlogd("upload frame size=%lu\n", msg->wpos);
    __xlogd("ip=%s:%u\n", __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg)), __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg)));

    list_parser = xl_parser(&msg->line);
    xline_t *objlist = xl_find(&list_parser, "list");
    __xcheck(objlist == NULL);

    if (__xl_sizeof_body(objlist) == 0){

        __xcheck(stream->list_pos != stream->list_size);
        frame = xlio_take_frame(stream);
        __xcheck(xl_add_int(&frame, "api", XLIO_STREAM_REQ_LIST) == XEOF);
        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
        stream->list_pos = stream->list_size = 0;

    }else {

        list_parser = xl_parser(objlist);

        while ((obj_val = xl_list_next(&list_parser)) != NULL){

            xl_printf(obj_val);
            obj_parser = xl_parser(obj_val);
            
            __xcheck((name_val = xl_find_word(&obj_parser, "path", &name)) == NULL);
            __xcheck(xl_find_int(&obj_parser, "type", &isfile) == NULL);
            __xcheck(xl_find_uint(&obj_parser, "size", &stream->file_size) == NULL);
            __xcheck(xl_find_uint(&obj_parser, "pos", &pos) == NULL);

            int full_path_len = stream->uri_len + 1 + __xl_sizeof_body(name_val);
            char full_path[full_path_len];                
            __xapi->snprintf(full_path, full_path_len, "%s/%s", stream->uri, name);
            __xlogd("download file = %s\n", full_path);

            if (stream->fd == -1){
                if (pos == 0){
                    stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_CREATE, 0644);
                    __xcheck(stream->fd < 0);
                }else {
                    stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_WRITE, 0644);
                    __xcheck(stream->fd < 0);
                    __xcheck(pos != __xapi->fs_file_tell(stream->fd));
                }
            }else {
                __xcheck(pos != __xapi->fs_file_tell(stream->fd));
            }

            xline_t *bin = xl_find(&obj_parser, "data");
            if (bin != NULL){
                uint64_t data_len = __xl_sizeof_body(bin);
                if (data_len > 0){
                    __xcheck(__xapi->fs_file_write(stream->fd, __xl_b2o(bin), data_len) != data_len);
                    stream->file_pos += data_len;
                    stream->list_pos += data_len;
                    __xlogd("list size = %lu pos = %lu\n", stream->list_size, stream->list_pos);
                }
            }

            if (stream->file_pos == stream->file_size){
                __xapi->fs_file_close(stream->fd);
                stream->fd = -1;
                stream->file_pos = 0;
                stream->file_size = 0;
            }
        }
    }

    return 0;
XClean:
    if (frame != NULL){
        xlio_resave_frame(stream, frame);
    }
    return -1;
}

static inline int xlio_upload_list(xlio_stream_t *stream)
{
    xframe_t *frame = NULL;
    if (stream->upload_list != NULL){
        if (stream->list_pos < stream->list_size){
            frame = xlio_take_frame(stream);
            __xcheck(xlio_send_file(stream, &frame) != 0);
        }else {
            __xcheck(stream->list_pos != stream->list_size);
            frame = xlio_take_frame(stream);
            __xcheck(xl_add_int(&frame, "api", XLIO_STREAM_UPLOAD_LIST) == XEOF);
            __xcheck(xl_list_end(&frame, xl_list_begin(&frame, "list")) == XEOF);
            frame->type = XPACK_TYPE_MSG;
            __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
            xl_free(&stream->upload_list);
            stream->list_pos = stream->list_size = 0;
        }
    }
    return 0;
XClean:
    if (frame != NULL){
        xlio_resave_frame(stream, frame);
    }
    return -1;
}

static inline int xlio_check_list(xlio_stream_t *ios, xline_t *inlist, xframe_t **outframe)
{
    char *name;
    int64_t isfile;
    uint64_t size;
    char md5[64] = {0};
    xline_t *obj, *name_val, *md5_val;
    xframe_t obj_parser, list_parser = xl_parser(inlist);

    uint64_t pos = xl_list_begin(outframe, "list");
    __xcheck(pos == XEOF);

    while ((obj = xl_list_next(&list_parser)) != NULL){
        obj_parser = xl_parser(obj);
        __xcheck(xl_find_int(&obj_parser, "type", &isfile) == NULL);
        __xcheck((name_val = xl_find_word(&obj_parser, "path", &name)) == NULL);
        __xcheck(xl_find_uint(&obj_parser, "size", &size) == NULL);
        __xcheck((md5_val = xl_find(&obj_parser, "md5")) == NULL);
        int path_len = ios->uri_len + 1 + __xl_sizeof_body(name_val);
        char path[path_len];
        __xapi->snprintf(path, path_len, "%s/%s", ios->uri, name);
        if (isfile){
            if (!__xapi->fs_file_exist(path)){
                __xcheck(xl_list_append(outframe, obj) == XEOF);
                ios->list_size += size;
            }else {
                // TODO 对比 md5
                if(mcompare(md5, __xl_b2o(md5_val), 64) != 0){
                    __xcheck(xl_list_append(outframe, obj) == XEOF);
                    ios->list_size += size;
                    __xlogd("xlio_check_list >>>>---------------> mcompare md5\n");
                }
            }
        }else {
            if (!__xapi->fs_dir_exist(path)){
                __xcheck(__xapi->fs_path_maker(path) != 0);
            }
        }
    }

    xl_list_end(outframe, pos);

    return 0;
XClean:
    return -1;
}

static inline int xlio_gen_list(xlio_stream_t *ios, xframe_t **frame)
{
    uint64_t file_size;
    char md5[64] = {0};
    ios->list_size = ios->file_size;
    uint64_t list_pos = xl_list_begin(frame, "list");
    __xcheck(list_pos == XEOF);

    uint64_t obj_pos = xl_obj_begin(frame, NULL);
    __xcheck(obj_pos == XEOF);
    __xlogd("file name = %s::%s\n", ios->uri, ios->uri + ios->src_name_pos);
    __xcheck(xl_add_word(frame, "path", ios->uri + ios->src_name_pos) == XEOF);
    __xcheck(xl_add_int(frame, "type", 1) == XEOF);
    __xcheck(xl_add_uint(frame, "size", ios->file_size) == XEOF);
    __xcheck(xl_add_bin(frame, "md5", md5, 64) == XEOF);

    xl_obj_end(frame, obj_pos);
    xl_list_end(frame, list_pos);

    xl_printf(&(*frame)->line);

    return 0;
XClean:
    return -1;
}

static inline int xlio_scan_dir(xlio_stream_t *ios, xframe_t **frame)
{
    char md5[64] = {0};
    xframe_t *obj = xl_maker();
    __xcheck(obj == NULL);
    uint64_t pos = xl_list_begin(frame, "list");
    __xcheck(pos == XEOF);

    do {

        if (ios->item != NULL){

            __xcheck(xl_add_int(&obj, "type", ios->item->type) == XEOF);
            __xcheck(xl_add_word(&obj, "path", ios->item->path + ios->src_name_pos) == XEOF);
            __xcheck(xl_add_uint(&obj, "size", ios->item->size) == XEOF);
            __xcheck(xl_add_bin(&obj, "md5", md5, 64) == XEOF);

            if ((*frame)->size - (*frame)->wpos < __xl_sizeof_line(&obj->line)){
                xl_list_end(frame, pos);
                xl_free(&obj);
                return 0;
            }

            __xcheck(xl_list_append(frame, &obj->line) == XEOF);
            ios->list_size += ios->item->size;
            xl_clear(obj);
        }

        ios->item = __xapi->fs_scanner_read(ios->scanner);

    }while (ios->item != NULL);

    xl_list_end(frame, pos);
    xl_free(&obj);

    __xapi->fs_scanner_close(ios->scanner);
    ios->scanner = NULL;

    return 0;
XClean:
    return -1;
}

static inline int xlio_recv_frame(xltp_t *xltp, xframe_t *msg, void *ctx)
{
    __xlogd("xlio_recv_frame >>>>---------------> enter\n");

    
    char *name;
    xframe_t *frame = NULL;
    xlio_stream_t *stream = (xlio_stream_t*)ctx;
    xframe_t parser = xl_parser(&msg->line);
    __xcheck(xl_find_int(&parser, "api", &stream->api) == NULL);

    if (stream->api == XLIO_STREAM_REQ_LIST){

        __xlogd("xlio_recv_frame >>>>---------------> XLIO_STREAM_REQ_LIST\n");

        frame = xlio_take_frame(stream);
        __xcheck(xl_add_int(&frame, "api", XLIO_STREAM_RES_LIST) == XEOF);
        if (stream->scanner != NULL){
            __xcheck(xl_add_uint(&frame, "size", 0) == XEOF);
            __xcheck(xlio_scan_dir(stream, &frame) != 0);
            parser = xl_parser(&frame->line);
            __xcheck(xl_set_value(&parser, "size", stream->list_size) == NULL);
        }else if (stream->file_size > 0){
            __xcheck(xl_add_uint(&frame, "size", stream->file_size) == XEOF);
            __xcheck(xlio_gen_list(stream, &frame) != 0);
        }else {
            __xcheck(xl_list_end(&frame, xl_list_begin(&frame, "list")) == XEOF);
        }
        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);

    }else if (stream->api == XLIO_STREAM_RES_LIST){

        __xlogd("xlio_recv_frame >>>>---------------> XLIO_STREAM_RES_LIST\n");

        xline_t *list = xl_find(&parser, "list");
        __xcheck(list == NULL);
        frame = xlio_take_frame(stream);

        if (__xl_sizeof_body(list) > 0){
            __xcheck(xl_add_int(&frame, "api", XLIO_STREAM_DOWNLOAD_LIST) == XEOF);
            __xcheck(xl_add_uint(&frame, "size", 0) == XEOF);
            __xcheck(xlio_check_list(stream, list, &frame) != 0);
            if (stream->list_size > 0){
                parser = xl_parser(&frame->line);
                __xcheck(xl_set_value(&parser, "size", stream->list_size) == NULL);
            }else {
                parser = xl_parser(&frame->line);
                __xcheck(xl_set_value(&parser, "api", XLIO_STREAM_REQ_LIST) == NULL);
            }
            frame->type = XPACK_TYPE_MSG;
            __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
        }else {
            frame->type = XPACK_TYPE_RES;
            __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
        }

    }else if (stream->api == XLIO_STREAM_DOWNLOAD_LIST){

        __xlogd("xlio_recv_frame >>>>---------------> XLIO_STREAM_DOWNLOAD_LIST\n");

        __xcheck(stream->upload_list != NULL);
        stream->upload_list = msg;
        __xcheck(xl_find_uint(&parser, "size", &stream->list_size) == NULL);
        xline_t *list = xl_find(&parser, "list");
        __xcheck(list == NULL);
        if (__xl_sizeof_body(list) > 0){
            xl_hold(msg);
            stream->upload_parser = xl_parser(list);
            while (stream->list_pos < stream->list_size && __serialbuf_readable(&stream->buf)){
                frame = xlio_take_frame(stream);
                __xcheck(xlio_send_file(stream, &frame) != 0);
            }
        }

    }else if (stream->api == XLIO_STREAM_UPLOAD_LIST){

        __xlogd("xlio_recv_frame >>>>---------------> XLIO_STREAM_UPLOAD_LIST\n");
        __xcheck(xlio_recv_file(stream, msg) != 0);
    }

    __xlogd("xlio_recv_frame >>>>---------------> exit\n");

    return 0;
XClean:
    if (frame != NULL){
        xlio_resave_frame(stream ,frame);
    }
    return -1;
}

static void xlio_loop(void *ptr)
{
    __xlogd("xlio_loop >>>>---------------> enter\n");

    xframe_t *msg;
    xlio_stream_t *stream;
    xlio_t *io = (xlio_t*)ptr;

    while (io->running)
    {
        if (xpipe_read(io->pipe, &msg, __sizeof_ptr) != __sizeof_ptr){
            break;
        }

        stream = xchannel_get_ctx(__xmsg_get_channel(msg));
        if (stream == NULL){
            continue;
        }

        if (msg->flag == XMSG_FLAG_RECV){

            if (msg->type == XPACK_TYPE_MSG){

                if (__serialbuf_readable(&stream->buf) == 0){
                    __xlogd("xliokabuf le\n");
                    xl_hold(msg);
                    stream->recv_frame = msg;
                    __xmsg_set_cb(stream->recv_frame, xlio_recv_frame);
                }else {
                    if (xlio_recv_frame(NULL, msg, stream) != 0){
                        xlio_stream_free(stream);
                        stream = NULL;
                    }
                }
            }

            xl_free(&msg);

        }else if (msg->flag == XMSG_FLAG_BACK){

            xlio_resave_frame(stream, msg);

            if (stream->recv_frame != NULL){

                if ((__xmsg_get_cb(stream->recv_frame))(NULL, stream->recv_frame, stream) != 0){
                    xlio_stream_free(stream);
                    stream = NULL;
                }
                xl_free(&stream->recv_frame);

            }else if (stream->flag == IOSTREAM_TYPE_UPLOAD){
                
                if (xlio_upload_list(stream) != 0){
                    xlio_stream_free(stream);
                    stream = NULL;
                }
            }

        }else if (msg->flag == XMSG_FLAG_POST){

            (__xmsg_get_cb(msg))(NULL, msg, __xmsg_get_ctx(msg));

        }

    }

    __xlogd("xlio_loop >>>>---------------> exit\n");
}

xlio_t* xlio_create(xmsger_ptr msger)
{
    xlio_t *io = (xlio_t*)malloc(sizeof(xlio_t));
    __xcheck(io == NULL);

    io->running = true;
    io->msger = msger;

    io->pipe = xpipe_create(sizeof(void*) * 1024, "IO PIPE");
    __xcheck(io->pipe == NULL);

    io->tid = __xapi->thread_create(xlio_loop, io);
    __xcheck(io->tid == NULL);

    io->streamlist.prev = &io->streamlist;
    io->streamlist.next = &io->streamlist;

    return io;
XClean:
    if (io != NULL){
        xlio_free(&io);
    }
    return NULL;
}

void xlio_free(xlio_t **pptr)
{
    if (pptr && *pptr){
        xlio_t *io = *pptr;
        *pptr = NULL;

        __set_false(io->running);

        if(io->pipe){
            if (io->pipe){
                xpipe_break(io->pipe);
            }
            if (io->tid){
                __xapi->thread_join(io->tid);
            }
            xframe_t *msg;
            while (__xpipe_read(io->pipe, &msg, __sizeof_ptr) == __sizeof_ptr){
                __xlogi("free msg\n");
                xl_free(&msg);
            }
            xpipe_free(&io->pipe);
        }

        xlio_stream_t *next, *stream = io->streamlist.next;
        while (stream != &io->streamlist){
            next = stream->next;
            stream->prev->next = stream->next;
            stream->next->prev = stream->prev;
            xlio_stream_free(stream);
            stream = next;
        }

        free(io);
    }
}

static int xlio_post_close(xltp_t *tp, xframe_t *msg, void *ctx)
{
    __xlogd("xlio_post_close enter\n");
    xlio_stream_t *ios = (xlio_stream_t *)ctx;
    xmsger_flush(ios->io->msger, ios->channel);
    __xlogd("xlio_post_close 1\n");
    xlio_stream_free(ios);
    __xlogd("xlio_post_close 2\n");
    xl_free(&msg);
    __xlogd("xlio_post_close exit\n");
    return 0;
XClean:
    return -1;
}

int xlio_stream_close(xlio_stream_t *ios)
{
    __xlogd("xlio_stream_close enter\n");
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(msg, xlio_post_close);
    __xmsg_set_ctx(msg, ios);
    __xmsg_set_channel(msg, ios->channel);
    __xcheck(xpipe_write(ios->io->pipe, &msg, __sizeof_ptr) != __sizeof_ptr);
    __xlogd("xlio_stream_close exit\n");
    return 0;
XClean:
    xlio_stream_free(ios);
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;    
}

static int xlio_post_download(xltp_t *tp, xframe_t *msg, void *ctx)
{
    xlio_stream_t *ios = (xlio_stream_t *)ctx;
    __xcheck(__serialbuf_readable(&ios->buf) == 0);
    xframe_t *frame = xlio_take_frame(ios);
    xl_add_int(&frame, "api", XLIO_STREAM_REQ_LIST);
    frame->type = XPACK_TYPE_MSG;
    __xcheck(xmsger_send(ios->io->msger, ios->channel, frame) != 0);
    xl_free(&msg);
    return 0;
XClean:
    return -1;
}

int xlio_start_downloader(xlio_t *io, xframe_t *req, int response)
{
    char *uri;
    xframe_t parser = xl_parser(&req->line);
    xl_find_word(&parser, "path", &uri);
    __xcheck(uri == NULL);

    xlio_stream_t *ios = xlio_stream_maker(io, uri, IOSTREAM_TYPE_DOWNLOAD);
    __xcheck(ios == NULL);

    ios->channel = __xmsg_get_channel(req);
    xchannel_set_ctx(ios->channel, ios);
    __xmsg_set_ctx(req, ios);

    if (response){
        xframe_t *frame = xlio_take_frame(ios);
        xl_add_int(&frame, "status", 200);
        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(ios->io->msger, ios->channel, frame) != 0);
    }

    req->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(req, xlio_post_download);
    __xcheck(xpipe_write(ios->io->pipe, &req, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    xlio_stream_free(ios);
    return -1;
}

int xlio_start_uploader(xlio_t *io, xframe_t *req, int response)
{
    char *uri;
    xframe_t parser = xl_parser(&req->line);
    xl_find_word(&parser, "uri", &uri);
    __xcheck(uri == NULL);

    xlio_stream_t *ios = xlio_stream_maker(io, uri, IOSTREAM_TYPE_UPLOAD);
    __xcheck(ios == NULL);

    ios->channel = __xmsg_get_channel(req);
    xchannel_set_ctx(ios->channel, ios);

    if (response){
        xframe_t *frame = xlio_take_frame(ios);
        xl_add_int(&frame, "status", 200);
        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(ios->io->msger, ios->channel, frame) != 0);
    }

    xl_free(&req);
    return 0;
XClean:
    xlio_stream_free(ios);
    if (req){
        xl_free(&req);
    }    
    return -1;
}

xlio_stream_t* xlio_stream_maker(xlio_t *io, const char *uri, int stream_type)
{
    xlio_stream_t* ios = (xlio_stream_t*)calloc(1, sizeof(xlio_stream_t));
    __xcheck(ios == NULL);
    mclear(ios, sizeof(xlio_stream_t));
    ios->flag = stream_type;

    // if (__xapi->fs_file_exist(uri)){
    //     ios->is_dir = 0;
    //     if (ios->flag == IOSTREAM_TYPE_DOWNLOAD){
    //         ios->is_resend = 1;
    //     }
    // }else if (__xapi->fs_dir_exist(uri)){
    //     ios->is_dir = 1;
    //     if (ios->flag == IOSTREAM_TYPE_DOWNLOAD){
    //         ios->is_resend = 1;
    //     }
    // }else {
    //     __xcheck((ios->flag == IOSTREAM_TYPE_UPLOAD));
    // }

    ios->uri_len = slength(uri);
    while (uri[ios->uri_len-1] == '/'){
        ios->uri_len--;
    }
    mcopy(ios->uri, uri, ios->uri_len);
    ios->src_name_len = 0;
    while (ios->src_name_len < ios->uri_len 
            && ios->uri[ios->uri_len - ios->src_name_len - 1] != '/'){
        ios->src_name_len++;
    }
    ios->src_name_pos = ios->uri_len - ios->src_name_len;

    if (ios->flag == IOSTREAM_TYPE_UPLOAD){
        if (__xapi->fs_dir_exist(uri)){
            ios->scanner = __xapi->fs_scanner_open(ios->uri);
            __xcheck(ios->scanner == NULL);
        }else {
            ios->file_size = __xapi->fs_file_size(ios->uri);
        }
    }

    ios->buf.range = MSGBUF_RANGE;
    ios->buf.wpos = ios->buf.range;
    ios->buf.rpos = ios->buf.spos = 0;
    for (size_t i = 0; i < MSGBUF_RANGE; i++){
        ios->buf.buf[i] = xl_creator(MSGBUF_SIZE);
        __xcheck(ios->buf.buf[i] == NULL);
        __xmsg_set_ctx(ios->buf.buf[i], ios);
    }

    ios->fd = -1;
    ios->io = io;
    ios->next = &io->streamlist;
    ios->prev = io->streamlist.prev;
    ios->next->prev = ios;
    ios->prev->next = ios;
    
    return ios;
XClean:
    xlio_stream_free(ios);
    return NULL;
}

void xlio_stream_free(xlio_stream_t *ios)
{
    __xlogd("xlio_stream_free enter\n");
    if (ios){
        if (ios->fd > 0){
            __xlogd("xlio_stream_free 1\n");
            __xapi->fs_file_close(ios->fd);
        }
        if (ios->scanner != NULL){
            __xlogd("xlio_stream_free 2\n");
            __xapi->fs_scanner_close(ios->scanner);
            ios->scanner = NULL;
        }
        if (ios->upload_list != NULL){
            __xlogd("xlio_stream_free 3\n");
            xl_free(&ios->upload_list);
        }
        if (ios->prev != NULL && ios->next != NULL){
            ios->prev->next = ios->next;
            ios->next->prev = ios->prev;
        }
        for (size_t i = 0; i < MSGBUF_RANGE; i++){
            if (ios->buf.buf[i] != NULL){
                __xlogd("xlio_stream_free buf %lu\n", ios->buf.buf[i]->ref);
                xl_free(&ios->buf.buf[i]);
            }
        }
        __xlogd("xlio_stream_free 4\n");
        free(ios);
    }
    __xlogd("xlio_stream_free exit\n");
}

int xlio_stream_post(xlio_stream_t *ios, xframe_t *frame)
{
    frame->flag = XMSG_FLAG_POST;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xlio_stream_read(xlio_stream_t *ios, xframe_t *frame)
{
    __xlogd("xlio_stream_read enter\n");
    frame->flag = XMSG_FLAG_BACK;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    __xlogd("xlio_stream_read exit\n");
    return 0;
XClean:
    return -1;
}

int xlio_stream_write(xlio_stream_t *ios, xframe_t *frame)
{
    frame->flag = XMSG_FLAG_RECV;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}