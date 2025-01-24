#include "xlio.h"

#include "xpipe.h"

#define MSGBUF_RANGE    2
// #define MSGBUF_SIZE     1280 * 1024 //1.25MB
#define MSGBUF_SIZE     1280 * 16

typedef struct xmsgbuf {
    uint8_t range, spos, rpos, wpos;
    xline_t *buf[MSGBUF_RANGE];
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
    __xfile_t fd;
    int is_dir;
    int is_resend;
    xline_t *dir;
    uint16_t list_frame_count;
    xline_t list_frame;
    xline_t parser;
    xline_t *current_frame;
    xline_t list_parser;
    xbyte_t *obj;
    xbyte_t *dlist;
    uint64_t list_pos, list_size;
    // const char *path;
    // char fpath[2048];
    char uri[2048];
    int uri_len;
    int src_name_len;
    int src_name_pos;
    uint64_t pos, size;
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

#define XLIO_STREAM_SYN_LIST        0
#define XLIO_STREAM_GET_LIST        1
#define XLIO_STREAM_PUT_LIST        2

static inline int xlio_send_file(xlio_stream_t *stream, xline_t *frame)
{
    if (stream->fd > 0){
        __xlogd("xlio_send_file >>>>---------------> 1\n");
        uint64_t len = stream->size - stream->pos;
        if (stream->pos < stream->size){
            int ret;
            if (len > frame->size){
                ret = __xapi->fs_file_read(stream->fd, frame->ptr, frame->size);
            }else {
                ret = __xapi->fs_file_read(stream->fd, frame->ptr, len);
            }
            __xcheck(ret < 0);
            frame->wpos = ret;
            frame->type = XPACK_TYPE_BIN;
            __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
            stream->pos += frame->wpos;
            stream->list_pos += frame->wpos;
            __xlogd("-------------list size= %lu list pos = %lu\n", stream->list_size, stream->list_pos);
            if (stream->pos == stream->size){
                __xapi->fs_file_close(stream->fd);
                stream->fd = -1;
                stream->pos = 0;
                stream->size = 0;
            }
        }
    }else {

        xl_clear(frame);
        xl_add_int(&frame, "type", XLIO_STREAM_PUT_LIST);
        uint64_t list_begin_pos = xl_add_list_begin(&frame, "list");

        do {

            if ((stream->obj != NULL)){

                xl_printf(stream->obj);
                
                if (frame->size - frame->wpos < __xl_sizeof_line(stream->obj) + 256){
                    __xlogd("xlio_send_file frame size =%lu wpos =%lu obj size =%lu\n", frame->size, frame->wpos, __xl_sizeof_line(stream->obj));
                    break;
                }
                
                xline_t parser = xl_parser(stream->obj);
                const char *name = xl_find_word(&parser, "path");
                int64_t isfile = xl_find_int(&parser, "type");
                stream->size = xl_find_uint(&parser, "size");
                

                uint64_t obj_begin_pos = xl_add_obj_begin(&frame, NULL);

                xl_add_word(&frame, "path", name);
                xl_add_int(&frame, "type", isfile);
                xl_add_uint(&frame, "size", stream->size);

                if (isfile){
                    int full_path_len = stream->uri_len + slength(name) + 2;
                    char full_path[full_path_len];                
                    __xapi->snprintf(full_path, full_path_len, "%s/%s", stream->uri, name + (stream->src_name_len+1));
                    __xlogd("send file ================= %s ---- %s\n", full_path, name);
                    stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_READ, 0644);
                    __xcheck(stream->fd < 0);
                    if (stream->size == 0){
                        __xapi->fs_file_close(stream->fd);
                        stream->fd = -1;
                    }else {
                        uint64_t len = frame->size - frame->wpos - 6/* key size */ - 9 /* value head size */;
                        if (stream->size < len){
                            len = stream->size;
                        }
                        uint64_t pos = xl_add_bin(&frame, "data", NULL, len);
                        int ret = __xapi->fs_file_read(stream->fd, frame->ptr + (pos - len), len);
                        __xcheck(ret < 0);
                        stream->pos += len;
                        stream->list_pos += len;
                        __xlogd("-------------size= %lu pos = %lu\n", stream->size, stream->pos);
                        if (stream->pos == stream->size){
                            __xapi->fs_file_close(stream->fd);
                            stream->fd = -1;
                            stream->pos = 0;
                            stream->size = 0;
                        }
                    }
                }

                xl_add_obj_end(&frame, obj_begin_pos);
            }

            // __xlogd("xlio_send_file >>>>---------------> list count %lu\n", stream->list_frame_count);
            if (stream->list_parser.rpos == stream->list_parser.wpos && stream->list_frame.next != &stream->list_frame){
                xl_free(&stream->current_frame);
                stream->current_frame = stream->list_frame.next;
                stream->current_frame->prev->next = stream->current_frame->next;
                stream->current_frame->next->prev = stream->current_frame->prev;
                stream->list_frame_count--;
                xl_printf(&stream->current_frame->line);
                __xlogd("xlio_send_file >>>>---------------> stream->list_frame_count=%lu\n", stream->list_frame_count);
                stream->parser = xl_parser(&stream->current_frame->line);
                xbyte_t *listobj= xl_find(&stream->parser, "list");
                stream->list_parser = xl_parser(listobj);                
            }else {
                xl_free(&stream->current_frame);
                stream->current_frame = NULL;
            }
            
        }while ((stream->obj = xl_list_next(&stream->list_parser)) != NULL);

        xl_add_list_end(&frame, list_begin_pos);

        xl_printf(&frame->line);

        frame->type = XPACK_TYPE_MSG;
        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
    }

    __xlogd("xlio_send_file >>>>---------------> exit\n");
        
    return 0;
XClean:
    return -1;
}

static inline int xlio_check_list(xlio_stream_t *ios, xline_t **in, xline_t **out)
{
    ios->parser = xl_parser(&(*in)->line);
    xbyte_t *objlist = xl_find(&ios->parser, "list");
    ios->parser = xl_parser(objlist);

    uint64_t pos = xl_add_list_begin(out, "list");
    while ((ios->obj = xl_list_next(&ios->parser)) != NULL)
    {
        // xl_printf(ios->obj);
        // __xcheck(__xl_sizeof_body(obj) != __xl_sizeof_body(&msg->line));
        xline_t parser = xl_parser(ios->obj);
        int64_t isfile = xl_find_int(&parser, "type");
        if (isfile){
            xl_add_list_obj(out, ios->obj);
            // uint64_t pos = xl_add_obj_begin(out, "llll");
            // uint64_t pos = xl_add_obj_begin(out, NULL);
            // __xcheck(pos == XNONE);
            // __xcheck(xl_add_int(out, "type", 1) == XNONE);
            // xl_add_obj_end(out, pos);
        }else {
            const char *name = xl_find_word(&parser, "path");
            // ios->size = xl_find_uint(&parser, "size");
            int full_path_len = slength(ios->uri) + slength(name) + 2;
            char full_path[full_path_len];
            __xapi->snprintf(full_path, full_path_len, "%s/%s\0", ios->uri, name);
            __xlogd("mkpath === %s\n", full_path);
            __xapi->fs_path_maker(full_path);
        }
    }
    xl_add_list_end(out, pos);
    // xl_fixed(*out);
    __xlogd("========================= checklist\n");
    // xl_printf(&(*out)->line);
    return 0;
XClean:
    return -1;
}

static inline int xlio_scan_dir(xlio_stream_t *ios, xline_t **frame)
{
    // xl_clear(stream->list_frame);
    // __xcheck(xl_add_int(frame, "type", XLIO_STREAM_SYN_LIST) == XNONE);
    uint64_t pos = xl_add_list_begin(frame, "list");
    __xcheck(pos == XNONE);

    do {
        if (ios->item != NULL){
            if ((*frame)->size - (*frame)->wpos < ios->item->path_len + 256){
                // __xlogd("frame size = %lu wpos = %lu path len = %lu\n", (*frame)->size, (*frame)->wpos, ios->item->path_len + 256);
                xl_add_list_end(frame, pos);
                // (*frame)->type = XPACK_TYPE_MSG;
                // __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                return 0;
            }
            // __xlogd("scanner --- type(%d) size:%lu %s\n", ios->item->type, ios->item->size, ios->item->path + ios->dir_name_pos);
            uint64_t pos = xl_add_obj_begin(frame, NULL);
            __xcheck(pos == XNONE);
            __xcheck(xl_add_int(frame, "type", ios->item->type) == XNONE);
            __xcheck(xl_add_word(frame, "path", ios->item->path + ios->src_name_len) == XNONE);
            __xcheck(xl_add_uint(frame, "size", ios->item->size) == XNONE);
            xl_add_obj_end(frame, pos);
            ios->list_size += ios->item->size;
            // stream->item = NULL;
        }
        ios->item = __xapi->fs_scanner_read(ios->scanner);
    }while (ios->item != NULL);

    xl_add_list_end(frame, pos);
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
    xline_t parser;
    xline_t *msg;
    xline_t *frame;
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

            if (stream->flag == IOSTREAM_TYPE_UPLOAD){

                __xlogd("xlio_loop >>>>---------------> 1\n");

                if (msg->type == XPACK_TYPE_MSG){

                    parser = xl_parser(&msg->line);
                    xl_printf(&msg->line);
                    int64_t mtype = xl_find_int(&parser, "type");
                    __xlogi("xlio_loop >>>>---------------> msg size = %lu type=%ld\n", msg->wpos, mtype);
                    __xcheck(mtype != XLIO_STREAM_GET_LIST);

                    if (mtype == XLIO_STREAM_GET_LIST){

                        __xlogd("xlio_loop >>>>---------------> 2\n");

                        xl_hold(msg);
                        // xl_printf(&msg->line);
                        __xlogd("xlio_loop >>>>---------------> 3\n");

                        msg->prev = stream->list_frame.prev;
                        msg->next = &stream->list_frame;
                        msg->prev->next = msg;
                        msg->next->prev = msg;
                        stream->list_frame_count++;
                        __xlogd("xlio_loop >>>>------+++---------> stream->list_frame_count=%lu\n", stream->list_frame_count);

                        if (stream->scanner != NULL && __serialbuf_readable(&stream->buf)){
                            frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                            xl_clear(frame);
                            xl_add_int(&frame, "type", XLIO_STREAM_SYN_LIST);
                            __xcheck(xlio_scan_dir(stream, &frame) != 0);
                            stream->buf.rpos++;
                            // xl_printf(&frame->line);
                            frame->type = XPACK_TYPE_MSG;
                            __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                        }

                        while (stream->list_frame.next != &stream->list_frame && __serialbuf_readable(&stream->buf)){
                            frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                            xlio_send_file(stream, frame);
                            stream->buf.rpos++;
                        }
                    }
                }

            }else if (stream->flag == IOSTREAM_TYPE_DOWNLOAD){

                if (msg->flag == XMSG_FLAG_RECV) {

                    if (msg->type == XPACK_TYPE_MSG){

                        parser = xl_parser(&msg->line);
                        int64_t mtype = xl_find_int(&parser, "type");

                        if (mtype == XLIO_STREAM_SYN_LIST){

                            if (__serialbuf_readable(&stream->buf)){
                                frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                                xl_clear(frame);
                                xl_add_int(&frame, "type", XLIO_STREAM_GET_LIST);
                                // xl_printf(&msg->line);
                                xlio_check_list(stream, &msg, &stream->buf.buf[__serialbuf_rpos(&stream->buf)]);
                                xl_printf(&frame->line);
                                __xlogi("xlio_loop >>>>---------------> msg size = %lu\n", frame->wpos);
                                stream->buf.rpos++;
                                frame->type = XPACK_TYPE_MSG;
                                __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                            }else {
                                xl_hold(msg);
                                // xl_printf(&msg->line);
                                // __xlogd("debug frame list %p:%p\n", stream->list_frame.prev, stream->list_frame.next);
                                msg->next = &stream->list_frame;
                                msg->prev = stream->list_frame.prev;
                                msg->prev->next = msg;
                                msg->next->prev = msg;
                                stream->list_frame_count++;

                                __xlogd("xlio_loop >>>>---------------> list count %lu\n", stream->list_frame_count);

                                // xline_t *temp = stream->list_frame.next;
                                // while (temp != &stream->list_frame)
                                // {
                                //     __xlogd("debug frame list ref=%lu ---> %p\n", temp->ref, temp);
                                //     xl_printf(&temp->line);
                                //     temp = temp->next;
                                // }
                                
                            }

                        }else if (mtype == XLIO_STREAM_PUT_LIST){

                            xl_printf(&msg->line);
                            __xcheck(stream->fd != -1);
                            stream->parser = xl_parser(&msg->line);
                            xbyte_t *objlist = xl_find(&stream->parser, "list");
                            stream->parser = xl_parser(objlist);
                            while ((stream->obj = xl_list_next(&stream->parser)) != NULL)
                            {
                                xl_printf(stream->obj);
                                // __xcheck(__xl_sizeof_body(obj) != __xl_sizeof_body(&msg->line));
                                parser = xl_parser(stream->obj);
                                const char *name = xl_find_word(&parser, "path");
                                int64_t isfile = xl_find_int(&parser, "type");
                                stream->size = xl_find_uint(&parser, "size");
                                int full_path_len = slength(stream->uri) + slength(name) + 2;
                                char full_path[full_path_len];                
                                __xapi->snprintf(full_path, full_path_len, "%s/%s", stream->uri, name);
                                __xlogd("download file = %s\n", full_path);
                                if (isfile){
                                    stream->fd = __xapi->fs_file_open(full_path, XAPI_FS_FLAG_CREATE, 0644);
                                    __xcheck(stream->fd < 0);
                                    if (stream->size == 0){
                                        __xapi->fs_file_close(stream->fd);
                                        stream->fd = -1;
                                    }else {
                                        xbyte_t *bin = xl_find(&parser, "data");
                                        __xlogd("find data = %p\n", bin);
                                        if (bin != NULL){
                                            uint64_t data_len = __xl_sizeof_body(bin);
                                            __xlogd("find data len = %lu\n", data_len);
                                            __xcheck(__xapi->fs_file_write(stream->fd, __xl_b2o(bin), data_len) != data_len);
                                            stream->pos += data_len;
                                            stream->list_pos += data_len;
                                            if (stream->pos == stream->size){
                                                __xapi->fs_file_close(stream->fd);
                                                stream->fd = -1;
                                                stream->pos = 0;
                                                stream->size = 0;
                                            }                                
                                        }
                                    }
                                }else {
                                    __xapi->fs_path_maker(full_path);
                                }
                            }
                        
                        }

                    }else if (msg->type == XPACK_TYPE_BIN){
                        __xcheck(stream->fd == -1);
                        __xcheck(__xapi->fs_file_write(stream->fd, msg->ptr, msg->wpos) != msg->wpos);
                        stream->pos += msg->wpos;
                        stream->list_pos += msg->wpos;
                        if (stream->pos == stream->size){
                            __xapi->fs_file_close(stream->fd);
                            stream->fd = -1;
                            stream->pos = 0;
                            stream->size = 0;
                        }
                    }

                    // if (stream->list_pos == stream->list_size){
                    //     xl_clear(msg);
                    //     xl_hold(msg);
                    //     msg->type = XPACK_TYPE_RES;
                    //     __xcheck(xmsger_disconnect(stream->io->msger, msg) != 0);
                    // }
                }
            }

            xl_free(&msg);

        }else if (msg->flag == XMSG_FLAG_POST){

            // send sync list
            // __xcheck(xlio_sync_list(stream) != 0);
            (__xmsg_get_cb(msg))(NULL, msg, __xmsg_get_ctx(msg));
            // cb(NULL, msg, __xmsg_get_ctx(msg));

        }else if (msg->flag == XMSG_FLAG_BACK){

            if (stream->flag == IOSTREAM_TYPE_UPLOAD){
                __xlogd("xlio_loop >>>>---------------> 5\n");

                stream->buf.wpos++;
                xl_clear(msg);

                __xlogd("xlio_loop >>>>---------------> scanner=%p list count = %lu\n", stream->scanner, stream->list_frame_count);

                if (stream->list_frame_count < 2 && stream->scanner != NULL){
                    __xlogd("xlio_loop >>>>---------------> 6\n");
                    if (__serialbuf_readable(&stream->buf)){
                        frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                        xl_clear(frame);
                        xl_add_int(&frame, "type", XLIO_STREAM_SYN_LIST);
                        __xcheck(xlio_scan_dir(stream, &frame) != 0);
                        stream->buf.rpos++;
                        // xl_printf(&frame->line);
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                    }
                }else if (stream->list_frame.next != &stream->list_frame){ 
                    __xlogd("xlio_loop >>>>---------------> 7\n");
                    if (__serialbuf_readable(&stream->buf)){
                        frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                        __xlogd("xlio_loop >>>>---------------> 8\n");
                        xlio_send_file(stream, frame);
                        stream->buf.rpos++;
                    }
                }

            }else if (stream->flag == IOSTREAM_TYPE_DOWNLOAD){

                stream->buf.wpos++;
                xl_clear(msg);

                __xlogd("xlio_loop >>>>---------------> list count %lu\n", stream->list_frame_count);

                if (stream->list_frame.next != &stream->list_frame && __serialbuf_readable(&stream->buf)){ 
                    xline_t *listmsg = stream->list_frame.next;
                    listmsg->prev->next = listmsg->next;
                    listmsg->next->prev = listmsg->prev;
                    stream->list_frame_count--;
                    frame = stream->buf.buf[__serialbuf_rpos(&stream->buf)];
                    xl_clear(frame);
                    xl_add_int(&frame, "type", XLIO_STREAM_GET_LIST);
                    xlio_check_list(stream, &listmsg, &stream->buf.buf[__serialbuf_rpos(&stream->buf)]);
                    xl_printf(&frame->line);
                    __xlogi("xlio_loop >>>>---------------> msg size = %lu\n", frame->wpos);
                    stream->buf.rpos++;
                    frame->type = XPACK_TYPE_MSG;
                    __xcheck(xmsger_send(stream->io->msger, stream->channel, frame) != 0);
                    xl_free(&listmsg);
                }
            }
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
            xline_t *msg;
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

// static int xlio_release_stream(xltp_t*, xline_t *msg, void *ctx)
// {
//     return 0;
// }

static int xlio_post_download(xltp_t *tp, xline_t *msg, void *ctx)
{
    xlio_stream_t *ios = (xlio_stream_t *)ctx;
    ios->parser = xl_parser(&ios->dir->line);
    // ios->uri = xl_find_word(&ios->parser, "path");
    if (ios->list_frame.next == NULL || ios->list_frame.prev == NULL){
        __xloge("list error\n");
    }
    return 0;
}

static int xlio_post_upload(xltp_t *tp, xline_t *msg, void *ctx)
{
    xline_t *frame = NULL;
    xlio_stream_t *ios = (xlio_stream_t *)ctx;
    // xl_free(&msg);

    // ios->parser = xl_parser(&ios->dir->line);
    // msg = xl_find_ptr(&ios->parser, "ctx");
    // xline_t parser = xl_parser(&msg->line);
    // ios->path = xl_find_word(&parser, "lpath");
    // ios->uri = xl_find_word(&parser, "rpath");
    // __xlogd("local uri %s remote uri %s\n", ios->path, ios->uri);
    // xl_free(&msg);

    // int path_len = slength(ios->path);
    // ios->src_name_len = 0;
    // while (ios->path[path_len - ios->src_name_len - 1] != '/'){
    //     ios->src_name_len++;
    // }
    
    // ios->src_name_len = path_len - ios->src_name_len;
    // mcopy(ios->fpath, ios->path, ios->src_name_len - 1);

    if (ios->is_dir){
        __xlogd("item is dir\n");
        ios->scanner = __xapi->fs_scanner_open(ios->uri);
        __xcheck(ios->scanner == NULL);
        while (__serialbuf_readable(&ios->buf) > 0 && ios->scanner != NULL){
            xline_t *frame = ios->buf.buf[__serialbuf_rpos(&ios->buf)];
            xl_clear(frame);
            xl_add_int(&frame, "type", XLIO_STREAM_SYN_LIST);
            __xcheck(xlio_scan_dir(ios, &frame) != 0);
            ios->buf.rpos++;
            // xl_printf(&frame->line);
            frame->type = XPACK_TYPE_MSG;
            __xcheck(xmsger_send(ios->io->msger, ios->channel, frame) != 0);
        }
    }
    return 0;
XClean:
    if (frame != NULL){
        xl_free(&frame);
    }
    return -1;
}

int xlio_stream_upload(xlio_stream_t *ios, xline_t *frame)
{
    frame->flag = XMSG_FLAG_POST;
    __xmsg_set_cb(frame, xlio_post_upload);
    ios->channel = __xmsg_get_channel(frame);
    __xmsg_set_ctx(frame, ios);
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

static int scandir_cb(const char *name, int type, uint64_t size, void **ctx)
{
    xline_t **frame = (xline_t**)ctx;
    uint64_t pos = xl_add_obj_begin(frame, NULL);
    __xcheck(pos == XNONE);
    __xcheck(xl_add_word(frame, "path", name) == XNONE);
    __xcheck(xl_add_int(frame, "type", type) == XNONE);
    __xcheck(xl_add_uint(frame, "size", size) == XNONE);
    xl_add_obj_end(frame, pos);
    (*frame)->range += size;
    return 0;
XClean:
    return -1;
}

int xlio_path_scanner(const char *path, xline_t **frame)
{
    int path_len = slength(path);
    int name_pos = 0;
    while (path[path_len - name_pos - 1] != '/'){
        name_pos++;
    }
    int full_path_len = (path_len - name_pos) + 1;
    char full_path[full_path_len];
    mcopy(full_path, path, full_path_len);
    full_path[full_path_len-1] = '\0';
    xl_add_word(frame, "full", full_path);
    uint64_t pos = xl_add_list_begin(frame, "list");
    __xcheck(pos == XNONE);
#if 0
    if (__xapi->fs_dir_exist(path)){
        __xcheck(__xapi->fs_path_scanner(path, path_len - name_pos, scandir_cb, (void**)frame) != 0);
    }else if (__xapi->fs_file_exist(path)){
        __xcheck(scandir_cb(path + (path_len - name_pos), 1, __xapi->fs_file_size(path), (void**)frame) != 0);
    }
#else

    uint64_t root_pos = xl_add_obj_begin(frame, NULL);
    __xcheck(root_pos == XNONE);
    __xcheck(xl_add_word(frame, "path", path + (path_len - name_pos)) == XNONE);
    __xcheck(xl_add_int(frame, "type", 0) == XNONE);
    __xcheck(xl_add_uint(frame, "size", 0) == XNONE);
    xl_add_obj_end(frame, root_pos);

    __xfs_item_ptr item;
    __xfs_scanner_ptr scanner = __xapi->fs_scanner_open(path);
    while ((item = __xapi->fs_scanner_read(scanner)) != NULL)
    {
        // __xlogd("scanner --- type(%d) size:%lu %s\n", item->type, item->size, item->path + (path_len - name_pos));
        uint64_t pos = xl_add_obj_begin(frame, NULL);
        __xcheck(pos == XNONE);
        __xcheck(xl_add_word(frame, "path", item->path + (path_len - name_pos)) == XNONE);
        __xcheck(xl_add_int(frame, "type", item->type) == XNONE);
        __xcheck(xl_add_uint(frame, "size", item->size) == XNONE);
        xl_add_obj_end(frame, pos);
        (*frame)->range += item->size;
    }
    __xapi->fs_scanner_close(scanner);
#endif
    xl_add_list_end(frame, pos);
    __xcheck(xl_add_int(frame, "len", (*frame)->range) == XNONE);
    return 0;
XClean:
    return -1;
}

xlio_stream_t* xlio_stream_maker(xlio_t *io, const char *uri, int stream_type)
{
    xlio_stream_t* ios = (xlio_stream_t*)malloc(sizeof(xlio_stream_t));
    __xcheck(ios == NULL);
    mclear(ios, sizeof(xlio_stream_t));
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
    ios->pos = 0;
    ios->size = 0;
    ios->io = io;
    ios->next = &io->streamlist;
    ios->prev = io->streamlist.prev;
    ios->next->prev = ios;
    ios->prev->next = ios;

    ios->list_frame.prev = &ios->list_frame;
    ios->list_frame.next = &ios->list_frame;
    return ios;
XClean:
    xlio_stream_free(ios);
    return NULL;
}

void xlio_stream_free(xlio_stream_t *ios)
{
    if (ios){
        ios->prev->next = ios->next;
        ios->next->prev = ios->prev;
        if (ios->fd > 0){
            __xapi->fs_file_close(ios->fd);
        }
        if (ios->dir){
            xl_free(&ios->dir);
        }
        if (ios->scanner != NULL){
            __xapi->fs_scanner_close(ios->scanner);
            ios->scanner = NULL;
        }
        for (size_t i = 0; i < MSGBUF_RANGE; i++){
            if (ios->buf.buf[i] != NULL){
                xl_free(&ios->buf.buf[i]);
            }
        }
        if (ios->list_frame.next != &ios->list_frame){
            xline_t *frame = ios->list_frame.next->next;
            frame->next->prev = frame->prev;
            frame->prev->next = frame->next;
            xl_free(&frame);
        }
        free(ios);
    }
}

int xlio_stream_post(xlio_stream_t *ios, xline_t *frame)
{
    frame->flag = XMSG_FLAG_POST;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xlio_stream_read(xlio_stream_t *ios, xline_t *frame)
{
    __xlogd("xlio_stream_read enter\n");
    frame->flag = XMSG_FLAG_BACK;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    __xlogd("xlio_stream_read exit\n");
    return 0;
XClean:
    return -1;
}

int xlio_stream_write(xlio_stream_t *ios, xline_t *frame)
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
        return ios->size;
    }
    return XNONE;
}

uint64_t xlio_stream_update(xlio_stream_t *ios, uint64_t size)
{
    return ios->list_size - ios->list_pos;
}