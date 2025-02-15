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
    int64_t status;
    __xfile_t fd;
    int is_dir;
    int is_resend;
    xframe_t parser;
    xframe_t *current_frame;
    xframe_t list_parser;
    xline_t *obj;
    xline_t *dlist;
    uint64_t file_pos, file_size;
    uint64_t list_pos, list_size;
    // const char *path;
    // char fpath[2048];
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

static inline int xlio_send_file(xlio_stream_t *stream, xframe_t *frame)
{
    if (stream->fd > 0){
        int64_t isfile;
        char *name;
        __xcheck(stream->obj == NULL);
        xl_add_int(&frame, "api", XLIO_STREAM_UPLOAD_LIST);
        xl_add_int(&frame, "id", frame->id);
        uint64_t list_begin_pos = xl_list_begin(&frame, "list");

        xframe_t parser = xl_parser(stream->obj);
        xl_find_word(&parser, "path", &name);
        xl_find_int(&parser, "type", &isfile);
        xl_find_uint(&parser, "size", &stream->file_size);
        

        uint64_t obj_begin_pos = xl_obj_begin(&frame, NULL);

        xl_add_word(&frame, "path", name);
        xl_add_int(&frame, "type", isfile);
        xl_add_uint(&frame, "size", stream->file_size);
        xl_add_uint(&frame, "pos", stream->file_pos);

        uint64_t len = xl_usable(frame, "data");
        if (stream->file_size - stream->file_pos < len){
            len = stream->file_size - stream->file_pos;
        }
        uint64_t pos = xl_add_bin(&frame, "data", NULL, len);
        int ret = __xapi->fs_file_read(stream->fd, frame->ptr + (pos - len), len);
        __xcheck(ret < 0);

        stream->file_pos += len;
        stream->list_pos += len;
        __xlogd("-------------list size= %lu list pos = %lu\n", stream->list_size, stream->list_pos);
        __xlogd("-------------file size= %lu file pos = %lu\n", stream->file_size, stream->file_pos);
        if (stream->file_pos == stream->file_size){
            __xapi->fs_file_close(stream->fd);
            stream->fd = -1;
            stream->file_pos = 0;
            stream->file_size = 0;
            stream->obj = NULL;
        }

        xl_obj_end(&frame, obj_begin_pos);
        xl_list_end(&frame, list_begin_pos);

        // xl_printf(&frame->line);

        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);


    }else {

        xl_add_int(&frame, "api", XLIO_STREAM_UPLOAD_LIST);
        xl_add_int(&frame, "id", frame->id);
        uint64_t list_begin_pos = xl_list_begin(&frame, "list");

        do {

            if ((stream->obj != NULL)){
                int64_t isfile;
                char *name;
                // xl_printf(stream->obj);
                
                if (frame->size - frame->wpos < __xl_sizeof_line(stream->obj) + 256){
                    __xlogd("xlio_send_file frame size =%lu wpos =%lu obj size =%lu\n", frame->size, frame->wpos, __xl_sizeof_line(stream->obj));
                    break;
                }
                
                xframe_t parser = xl_parser(stream->obj);
                xl_find_word(&parser, "path", &name);
                xl_find_int(&parser, "type", &isfile);
                xl_find_uint(&parser, "size", &stream->file_size);
                

                uint64_t obj_begin_pos = xl_obj_begin(&frame, NULL);

                xl_add_word(&frame, "path", name);
                xl_add_int(&frame, "type", isfile);
                xl_add_uint(&frame, "size", stream->file_size);
                xl_add_uint(&frame, "pos", stream->file_pos);

                if (isfile){
                    int full_path_len = stream->uri_len + xlen(name) + 2;
                    char full_path[full_path_len];                
                    __xapi->snprintf(full_path, full_path_len, "%s/%s", stream->uri, name + (stream->src_name_len+1));
                    __xlogd("upload file %s\n", full_path);
                    stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_READ, 0644);
                    __xcheck(stream->fd < 0);
                    if (stream->file_size == 0){
                        __xapi->fs_file_close(stream->fd);
                        stream->fd = -1;
                    }else {
                        uint64_t len = xl_usable(frame, "data");
                        if (stream->file_size < len){
                            len = stream->file_size;
                        }
                        uint64_t pos = xl_add_bin(&frame, "data", NULL, len);
                        int ret = __xapi->fs_file_read(stream->fd, frame->ptr + (pos - len), len);
                        __xcheck(ret < 0);
                        stream->file_pos += len;
                        stream->list_pos += len;
                        __xlogd("-------------list size= %lu list pos = %lu\n", stream->list_size, stream->list_pos);
                        __xlogd("-------------file size= %lu file pos = %lu\n", stream->file_size, stream->file_pos);
                        if (stream->file_pos == stream->file_size){
                            __xapi->fs_file_close(stream->fd);
                            stream->fd = -1;
                            stream->file_pos = 0;
                            stream->file_size = 0;
                        }
                        if (len < stream->file_size){
                            xl_obj_end(&frame, obj_begin_pos);
                            break;
                        }
                    }
                }

                xl_obj_end(&frame, obj_begin_pos);
            }
            
        }while ((stream->obj = xl_list_next(&stream->list_parser)) != NULL);

        xl_list_end(&frame, list_begin_pos);

        // xl_printf(&frame->line);
        __xlogd("send frame size = %lu\n", frame->wpos);
        xframe_t parser = xl_parser(&frame->line);
        __xlogd("send frame rpos=%lu wpos=%lu\n", parser.rpos, parser.wpos);
        xline_t *olist = xl_find(&parser, "list");
        parser = xl_parser(olist);
        __xlogd("send list rpos=%lu wpos=%lu\n", parser.rpos, parser.wpos);
        

        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
    }

    __xlogd("xlio_send_file >>>>---------------> exit\n");
        
    return 0;
XClean:
    return -1;
}

static inline int xlio_check_list(xlio_stream_t *ios, xframe_t **in, xframe_t **out)
{
    // ios->parser = xl_parser(&(*in)->line);
    // xline_t *objlist = xl_find(&ios->parser, "list");
    // ios->parser = xl_parser(objlist);
    int64_t isfile;
    char *name;
    uint64_t size;
    uint64_t pos = xl_list_begin(out, "list");
    while ((ios->obj = xl_list_next(&ios->parser)) != NULL)
    {
        // xl_printf(ios->obj);
        // __xcheck(__xl_sizeof_body(obj) != __xl_sizeof_body(&msg->line));
        xframe_t parser = xl_parser(ios->obj);
        xl_find_int(&parser, "type", &isfile);
        if (isfile){
            xl_list_append(out, ios->obj);
            xl_find_uint(&parser, "size", &size);
            ios->list_size += size;
            // uint64_t pos = xl_obj_begin(out, "llll");
            // uint64_t pos = xl_obj_begin(out, NULL);
            // __xcheck(pos == XNONE);
            // __xcheck(xl_add_int(out, "type", 1) == XNONE);
            // xl_obj_end(out, pos);
        }else {
            xl_find_word(&parser, "path", &name);
            // ios->size = xl_find_uint(&parser, "size");
            int full_path_len = xlen(ios->uri) + xlen(name) + 2;
            char full_path[full_path_len];
            __xapi->snprintf(full_path, full_path_len, "%s/%s\0", ios->uri, name);
            __xlogd("mkpath === %s\n", full_path);
            __xapi->fs_path_maker(full_path);
        }
    }
    xl_list_end(out, pos);
    // xl_fixed(*out);
    __xlogd("========================= checklist\n");
    // xl_printf(&(*out)->line);
    return 0;
XClean:
    return -1;
}

static inline int xlio_scan_dir(xlio_stream_t *ios, xframe_t **frame)
{
    // xl_clear(stream->list_frame);
    // __xcheck(xl_add_int(frame, "type", XLIO_STREAM_SYN_LIST) == XNONE);
    uint64_t pos = xl_list_begin(frame, "list");
    __xcheck(pos == XEOF);

    do {
        if (ios->item != NULL){
            if ((*frame)->size - (*frame)->wpos < ios->item->path_len + 256){
                // __xlogd("frame size = %lu wpos = %lu path len = %lu\n", (*frame)->size, (*frame)->wpos, ios->item->path_len + 256);
                xl_list_end(frame, pos);
                // (*frame)->type = XPACK_TYPE_MSG;
                // __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                return 0;
            }
            // __xlogd("scanner --- type(%d) size:%lu %s\n", ios->item->type, ios->item->size, ios->item->path + ios->dir_name_pos);
            uint64_t pos = xl_obj_begin(frame, NULL);
            __xcheck(pos == XEOF);
            __xcheck(xl_add_int(frame, "type", ios->item->type) == XEOF);
            __xcheck(xl_add_word(frame, "path", ios->item->path + ios->src_name_pos) == XEOF);
            __xcheck(xl_add_uint(frame, "size", ios->item->size) == XEOF);
            xl_obj_end(frame, pos);
            ios->list_size += ios->item->size;
            // stream->item = NULL;
        }
        ios->item = __xapi->fs_scanner_read(ios->scanner);
    }while (ios->item != NULL);

    xl_list_end(frame, pos);
    // (*frame)->type = XPACK_TYPE_MSG;
    // __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);

    __xapi->fs_scanner_close(ios->scanner);
    ios->scanner = NULL;
    return 0;
XClean:
    return -1;
}

static void xlio_loop(void *ptr)
{
    __xlogd("xlio_loop >>>>---------------> enter\n");

    int ret = 0;
    uint64_t pos;
    int64_t isfile;
    char *name;
    xframe_t parser;
    xframe_t *msg;
    xframe_t *frame;
    uint64_t framelen = MSGBUF_SIZE;
    xmsgcb_ptr cb;
    xlio_stream_t *stream;
    xlio_t *io = (xlio_t*)ptr;

    while (io->running)
    {
        __xcheck(xpipe_read(io->pipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        stream = xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(stream == NULL);
        // stream->channel = __xmsg_get_channel(msg);

        if (msg->flag == XMSG_FLAG_RECV){

            if (msg->type == XPACK_TYPE_MSG){

                parser = xl_parser(&msg->line);
                // xl_printf(&msg->line);
                xl_find_int(&parser, "api", &stream->status);

                if (stream->status == XLIO_STREAM_REQ_LIST){

                    __xcheck(__serialbuf_readable(&stream->buf) == 0);
                    frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                    xl_clear(frame);
                    xl_add_int(&frame, "api", XLIO_STREAM_RES_LIST);
                    xl_add_uint(&frame, "size", 0);
                    if (stream->scanner != NULL){
                        __xcheck(xlio_scan_dir(stream, &frame) != 0);
                        parser = xl_parser(&frame->line);
                        xline_t *size = xl_find(&parser, "size");
                        *size = __xl_u2b(stream->list_size);
                    }else {
                        uint64_t pos = xl_list_begin(&frame, "list");
                        xl_list_end(&frame, pos);
                    }
                    // xl_printf(&frame->line);
                    frame->type = XPACK_TYPE_MSG;
                    __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                    stream->buf.rpos++;

                }else if (stream->status == XLIO_STREAM_RES_LIST){

                    xline_t *list = xl_find(&parser, "list");
                    __xcheck(__serialbuf_readable(&stream->buf) == 0);
                    if (__xl_sizeof_body(list) > 0){
                        stream->parser = xl_parser(list);
                        frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                        xl_clear(frame);
                        xl_add_int(&frame, "api", XLIO_STREAM_DOWNLOAD_LIST);
                        xl_add_uint(&frame, "size", 0);
                        xlio_check_list(stream, &msg, &frame);
                        if (stream->list_size > 0){
                            parser = xl_parser(&frame->line);
                            xline_t *size = xl_find(&parser, "size");
                            *size = __xl_u2b(stream->list_size);
                        }else {
                            xl_clear(frame);
                            xl_add_int(&frame, "api", XLIO_STREAM_REQ_LIST);
                        }
                        // xl_printf(&frame->line);
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                        stream->buf.rpos++;
                    }else {
                        frame->type = XPACK_TYPE_RES;
                        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);                        
                        stream->buf.rpos++;
                    }

                }else if (stream->status == XLIO_STREAM_DOWNLOAD_LIST){

                    // xl_printf(&msg->line);
                    __xcheck(stream->current_frame != NULL);
                    stream->current_frame = msg;
                    xl_find_uint(&parser, "size", &stream->list_size);
                    xline_t *list = xl_find(&parser, "list");
                    if (__xl_sizeof_body(list) > 0){
                        xl_hold(msg);
                        stream->list_parser = xl_parser(list);
                        while (stream->list_pos < stream->list_size && __serialbuf_readable(&stream->buf)){
                            __xlogd("buf rpos = %u wpos = %u readable = %u\n", __serialbuf_wpos(&stream->buf), __serialbuf_rpos(&stream->buf), __serialbuf_readable(&stream->buf));
                            frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                            frame->id = stream->buf.rpos;
                            xl_clear(frame);
                            xlio_send_file(stream, frame);
                            stream->buf.rpos++;
                        }
                    }

                }else if (stream->status == XLIO_STREAM_UPLOAD_LIST){

                    // xl_printf(&msg->line);
                    __xlogd("upload frame size=%lu\n", msg->wpos);
                    __xlogd("ip=%s:%u\n", __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg)), __xapi->udp_addr_ip(__xmsg_get_ipaddr(msg)));
                    // __xcheck(stream->fd != -1);
                    stream->parser = xl_parser(&msg->line);
                    __xlogd("write data frame rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
                    xline_t *objlist = xl_find(&stream->parser, "list");
                    if (__xl_sizeof_body(objlist) == 0){
                        __xcheck(stream->list_pos != stream->list_size);
                        __xcheck(__serialbuf_readable(&stream->buf) == 0);
                        xframe_t *frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                        xl_clear(frame);
                        xl_add_int(&frame, "api", XLIO_STREAM_REQ_LIST);
                        stream->buf.rpos++;
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                        stream->list_pos = stream->list_size = 0;

                    }else {

                        stream->parser = xl_parser(objlist);
                        __xlogd("write data 0 rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
                        while ((stream->obj = xl_list_next(&stream->parser)) != NULL)
                        {
                            __xlogd("write data 1 rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
                            xl_printf(stream->obj);
                            // __xcheck(__xl_sizeof_body(obj) != __xl_sizeof_body(&msg->line));
                            parser = xl_parser(stream->obj);
                            __xlogd("write data 2 rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
                            xl_find_word(&parser, "path", &name);
                            xl_find_int(&parser, "type", &isfile);
                            __xcheck(isfile != 1);
                            xl_find_uint(&parser, "size", &stream->file_size);
                            xl_find_uint(&parser, "pos", &pos);
                            int full_path_len = xlen(stream->uri) + xlen(name) + 2;
                            char full_path[full_path_len];                
                            __xlogd("write data 3 rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
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

                            if (stream->file_size == 0){
                                __xapi->fs_file_close(stream->fd);
                                stream->fd = -1;
                            }else {
                                xline_t *bin = xl_find(&parser, "data");
                                if (bin != NULL){
                                    uint64_t data_len = __xl_sizeof_body(bin);
                                    if (data_len > 0){
                                        __xcheck(__xapi->fs_file_write(stream->fd, __xl_b2o(bin), data_len) != data_len);
                                        stream->file_pos += data_len;
                                        stream->list_pos += data_len;
                                        __xlogd("list size = %lu pos = %lu\n", stream->list_size, stream->list_pos);
                                        if (stream->file_pos == stream->file_size){
                                            __xlogd("upload 1\n");
                                            __xapi->fs_file_close(stream->fd);
                                            stream->fd = -1;
                                            stream->file_pos = 0;
                                            stream->file_size = 0;
                                        }
                                    }
                                }
                            }
                            __xlogd("write data 4 rpos=%lu wpos=%lu\n", stream->parser.rpos, stream->parser.wpos);
                        }
                    
                    }

                }
            }

            xl_free(&msg);

        }else if (msg->flag == XMSG_FLAG_BACK){

            stream->buf.wpos++;
            xl_clear(msg);
            __xlogd("----- buf rpos = %u wpos = %u readable = %u\n", __serialbuf_wpos(&stream->buf), __serialbuf_rpos(&stream->buf), __serialbuf_readable(&stream->buf));

            if (stream->flag == IOSTREAM_TYPE_UPLOAD){
                if (stream->current_frame != NULL){
                    //TODO list size 需要从对方发过来的下载列表中获取
                    if (stream->list_pos < stream->list_size){
                        __xlogd("list size = %lu pos = %lu\n", stream->list_size, stream->list_pos);
                        if (__serialbuf_readable(&stream->buf)){
                            frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                            xlio_send_file(stream, frame);
                            stream->buf.rpos++;
                        }
                    }else {
                        __xlogd("list size = %lu pos = %lu\n", stream->list_size, stream->list_pos);
                        __xcheck(stream->list_pos != stream->list_size);
                        frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                        xl_add_int(&frame, "api", XLIO_STREAM_UPLOAD_LIST);
                        uint64_t pos = xl_list_begin(&frame, "list");
                        xl_list_end(&frame, pos);                    
                        stream->buf.rpos++;
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                        xl_free(&stream->current_frame);
                        // stream->current_frame = NULL;
                        stream->list_pos = stream->list_size = 0;
                    }
                }
            }

        }else if (msg->flag == XMSG_FLAG_POST){

            (__xmsg_get_cb(msg))(NULL, msg, __xmsg_get_ctx(msg));

        }

    }

    __xlogd("xlio_loop >>>>---------------> exit\n");

XClean:
    if (frame){
        xl_free(&frame);
    }
    return;
}

xlio_t* xlio_create(xmsger_ptr msger)
{
    xlio_t *io = (xlio_t*)xalloc(sizeof(xlio_t));
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

        xfree(io);
    }
}

static int xlio_post_close(xltp_t *tp, xframe_t *msg, void *ctx)
{
    xlio_stream_t *ios = (xlio_stream_t *)ctx;
    xmsger_flush(ios->io->msger, ios->channel);
    xlio_stream_free(ios);
    xl_free(&msg);
    return 0;
XClean:
    return -1;
}

int xlio_stream_close(xlio_stream_t *ios)
{
    xframe_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(msg, xlio_post_close);
    __xmsg_set_ctx(msg, ios);
    __xmsg_set_channel(msg, ios->channel);
    __xcheck(xpipe_write(ios->io->pipe, &msg, __sizeof_ptr) != __sizeof_ptr);    
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
    xframe_t *frame = ios->buf.buf[__serialbuf_rpos(&ios->buf)];
    xl_clear(frame);
    xl_add_int(&frame, "api", XLIO_STREAM_REQ_LIST);
    ios->buf.rpos++;
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
        xframe_t *res = ios->buf.buf[__serialbuf_rpos(&ios->buf)];
        xl_clear(res);
        xl_add_int(&res, "status", 200);
        ios->buf.rpos++;
        res->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(ios->io->msger, ios->channel, res) != 0);
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

    ios->scanner = __xapi->fs_scanner_open(ios->uri);
    __xcheck(ios->scanner == NULL);

    ios->channel = __xmsg_get_channel(req);
    xchannel_set_ctx(ios->channel, ios);

    if (response){
        xframe_t *res = ios->buf.buf[__serialbuf_rpos(&ios->buf)];
        xl_clear(res);
        xl_add_int(&res, "status", 200);
        ios->buf.rpos++;
        res->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(ios->io->msger, ios->channel, res) != 0);
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
    xlio_stream_t* ios = (xlio_stream_t*)xalloc(sizeof(xlio_stream_t));
    __xcheck(ios == NULL);
    xclear(ios, sizeof(xlio_stream_t));
    ios->flag = stream_type;

    if (__xapi->fs_file_exist(uri)){
        ios->is_dir = 0;
        if (ios->flag == IOSTREAM_TYPE_DOWNLOAD){
            ios->is_resend = 1;
        }
    }else if (__xapi->fs_dir_exist(uri)){
        ios->is_dir = 1;
        if (ios->flag == IOSTREAM_TYPE_DOWNLOAD){
            ios->is_resend = 1;
        }
    }else {
        __xcheck((ios->flag == IOSTREAM_TYPE_UPLOAD));
    }

    ios->uri_len = xlen(uri);
    while (uri[ios->uri_len-1] == '/'){
        ios->uri_len--;
    }
    xcopy(ios->uri, uri, ios->uri_len);
    ios->src_name_len = 0;
    while (ios->src_name_len < ios->uri_len 
            && ios->uri[ios->uri_len - ios->src_name_len - 1] != '/'){
        ios->src_name_len++;
    }
    ios->src_name_pos = ios->uri_len - ios->src_name_len;

    ios->buf.range = MSGBUF_RANGE;
    ios->buf.wpos = ios->buf.range;
    ios->buf.rpos = ios->buf.spos = 0;
    for (size_t i = 0; i < MSGBUF_RANGE; i++){
        ios->buf.buf[i] = xl_creator(MSGBUF_SIZE);
        __xcheck(ios->buf.buf[i] == NULL);
        __xmsg_set_ctx(ios->buf.buf[i], ios);
    }

    ios->list_pos = 0;
    ios->fd = -1;
    ios->file_pos = 0;
    ios->file_size = 0;
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
    if (ios){
        if (ios->fd > 0){
            __xapi->fs_file_close(ios->fd);
        }
        if (ios->scanner != NULL){
            __xapi->fs_scanner_close(ios->scanner);
            ios->scanner = NULL;
        }
        if (ios->current_frame != NULL){
            xl_free(&ios->current_frame);
        }
        if (ios->prev != NULL && ios->next != NULL){
            ios->prev->next = ios->next;
            ios->next->prev = ios->prev;
        }
        for (size_t i = 0; i < MSGBUF_RANGE; i++){
            if (ios->buf.buf[i] != NULL){
                xl_free(&ios->buf.buf[i]);
            }
        }
        xfree(ios);
    }
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

uint64_t xlio_stream_length(xlio_stream_t *ios)
{
    if (ios){
        return ios->file_size;
    }
    return XEOF;
}

uint64_t xlio_stream_update(xlio_stream_t *ios, uint64_t size)
{
    return ios->list_size - ios->list_pos;
}