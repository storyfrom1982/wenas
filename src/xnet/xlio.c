#include "xlio.h"

#include "xpipe.h"

typedef struct xlio_stream {
    int flag;
    __xfile_t fd;
    xline_t *dir;
    xline_t parser;
    xbyte_t *dlist;
    uint64_t list_pos, list_size;
    const char *path;
    uint64_t pos, size;
    struct xlio *io;
    xchannel_ptr channel;
    struct xlio_stream *prev, *next;
}xlio_stream_t;


typedef struct xlio {
    __atom_bool running;
    xmsger_ptr msger;
    xpipe_ptr pipe;
    __xthread_ptr tid;
    xlio_stream_t streamlist;
}xlio_t;

static void xlio_loop(void *ptr)
{
    __xlogd("xlio_loop >>>>---------------> enter\n");

    int ret = 0;
    xline_t parser;
    xline_t *msg;
    xline_t *frame;
    uint64_t framelen = 1280 * 128; //160KB
    xmsgcb_ptr cb;
    xlio_stream_t *stream;
    xlio_t *io = (xlio_t*)ptr;

    while (io->running)
    {
        __xcheck(xpipe_read(io->pipe, &msg, __sizeof_ptr) != __sizeof_ptr);

        stream = xchannel_get_ctx(__xmsg_get_channel(msg));
        __xcheck(stream == NULL);

        if (stream->flag == XAPI_FS_FLAG_READ){

            if (msg->flag == XMSG_FLAG_READY){

                xl_free(&msg);

                for (size_t i = 0; i < 3; i++){

                    if (stream->fd > -1){
                        frame = xl_creator(framelen);
                        __xcheck(frame == NULL);                        
                        ret = __xapi->fs_file_read(stream->fd, frame->ptr, framelen);
                        if (ret > 0){
                            frame->wpos = ret;
                            frame->type = XPACK_TYPE_BIN;
                            __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);
                            stream->list_pos += frame->wpos;
                        }else {
                            __xapi->fs_file_close(stream->fd);
                            stream->fd = -1;
                            stream->pos = 0;
                            stream->size = 0;
                            continue;
                        }

                    }else {

                        frame = xl_creator(framelen);
                        __xcheck(frame == NULL);
                        xbyte_t *obj = xl_list_next(&stream->parser);
                        __xcheck(obj == NULL);
                        parser = xl_parser(obj);
                        const char *name = xl_find_word(&parser, "path");
                        int64_t isfile = xl_find_int(&parser, "type");
                        stream->size = xl_find_uint(&parser, "size");
                        xl_add_word(&frame, "path", name);
                        xl_add_int(&frame, "type", isfile);
                        xl_add_uint(&frame, "size", stream->size);
                        if (isfile){
                            int full_path_len = slength(stream->path) + slength(name) + 2;
                            char full_path[full_path_len];                
                            __xapi->snprintf(full_path, full_path_len, "%s/%s\0", stream->path, name);
                            stream->fd = __xapi->fs_file_open(full_path, stream->flag, 0644);
                            __xcheck(stream->fd < 0);
                        }
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);
                    }
                }


            }else if(msg->flag == XMSG_FLAG_STREAM){

                frame = msg;
                __xcheck(frame->size != framelen);
                if (stream->fd > 0){
                    ret = __xapi->fs_file_read(stream->fd, frame->ptr, framelen);
                    if (ret > 0){
                        frame->wpos = ret;
                        frame->type = XPACK_TYPE_BIN;
                        __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);
                        stream->list_pos += frame->wpos;
                    }else {
                        __xapi->fs_file_close(stream->fd);
                        stream->fd = -1;
                        stream->pos = 0;
                        stream->size = 0;
                    }
                }
                xbyte_t *obj = xl_list_next(&stream->parser);
                __xcheck(obj == NULL);
                parser = xl_parser(obj);
                const char *name = xl_find_word(&parser, "path");
                int64_t isfile = xl_find_int(&parser, "type");
                stream->size = xl_find_uint(&parser, "size");
                xl_add_word(&frame, "path", name);
                xl_add_int(&frame, "type", isfile);
                xl_add_uint(&frame, "size", stream->size);
                if (isfile){
                    int full_path_len = slength(stream->path) + slength(name) + 2;
                    char full_path[full_path_len];                
                    __xapi->snprintf(full_path, full_path_len, "%s/%s\0", stream->path, name);
                    stream->fd = __xapi->fs_file_open(full_path, stream->flag, 0644);
                    __xcheck(stream->fd < 0);
                }
                frame->type = XPACK_TYPE_MSG;
                __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);                
            }

        }else if (stream->flag == XAPI_FS_FLAG_WRITE || stream->flag == XAPI_FS_FLAG_CREATE){

            if(msg->flag == XMSG_FLAG_STREAM){
                __xcheck(__xapi->fs_file_write(stream->fd, msg->ptr, msg->wpos) != msg->wpos);
                stream->pos += msg->wpos;
                stream->list_pos += msg->wpos;
                if (stream->pos == stream->size){
                    __xapi->fs_file_close(stream->fd);
                    stream->fd = -1;
                    stream->pos = 0;
                    stream->size = 0;
                }
            }else if(msg->flag == XMSG_FLAG_READY){
                __xcheck(stream->fd != -1);
                xbyte_t *obj = xl_list_next(&stream->parser);
                __xcheck(obj == NULL);
                // __xcheck(__xl_sizeof_body(obj) != __xl_sizeof_body(&msg->line));
                parser = xl_parser(obj);
                const char *name = xl_find_word(&parser, "path");
                int64_t isfile = xl_find_int(&parser, "type");
                stream->size = xl_find_uint(&parser, "size");
                int full_path_len = slength(stream->path) + slength(name) + 2;
                char full_path[full_path_len];                
                __xapi->snprintf(full_path, full_path_len, "%s/%s\0", stream->path, name);
                if (isfile){
                    stream->fd = __xapi->fs_file_open(full_path, stream->flag, 0644);
                    __xcheck(stream->fd < 0);
                }else {
                    __xapi->fs_path_maker(full_path);
                }
            }
            xl_free(&msg);
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
    if (__xapi->fs_dir_exist(path)){
        __xcheck(__xapi->fs_path_scanner(path, path_len - name_pos, scandir_cb, (void**)frame) != 0);
    }else if (__xapi->fs_file_exist(path)){
        __xcheck(scandir_cb(path + (path_len - name_pos), 1, __xapi->fs_file_size(path), (void**)frame) != 0);
    }
    xl_add_list_end(frame, pos);
    __xcheck(xl_add_int(frame, "len", (*frame)->range) == XNONE);
    return 0;
XClean:
    return -1;
}

xlio_stream_t* xlio_stream_maker(xlio_t *io, xline_t *frame, int stream_type)
{
    xlio_stream_t* stream = (xlio_stream_t*)malloc(sizeof(xlio_stream_t));
    __xcheck(stream == NULL);
    stream->flag = stream_type;
    stream->dir = frame;
    stream->parser = xl_parser(&frame->line);
    if (stream_type == XAPI_FS_FLAG_READ){
        stream->path = xl_find_word(&stream->parser, "full");
    }else {
        stream->path = xl_find_word(&stream->parser, "path");
        if (!__xapi->fs_dir_exist(stream->path)){
            __xapi->fs_path_maker(stream->path);
        }
    }
    stream->dlist = xl_find(&stream->parser, "list");
    stream->list_size = xl_find_uint(&stream->parser, "len");
    stream->parser = xl_parser(stream->dlist);
    
    stream->list_pos = 0;
    stream->fd = -1;
    stream->pos = 0;
    stream->size = 0;
    stream->io = io;
    stream->channel = NULL;
    stream->next = &io->streamlist;
    stream->prev = io->streamlist.prev;
    stream->next->prev = stream;
    stream->prev->next = stream;
    return stream;
XClean:
    if (stream){
        free(stream);
    }
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
        free(ios);
    }
}

int xlio_stream_ready(xlio_stream_t *ios, xline_t *frame)
{
    frame->flag = XMSG_FLAG_READY;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xlio_stream_read(xlio_stream_t *ios, xline_t *frame)
{
    frame->flag = XMSG_FLAG_STREAM;
    __xcheck(xpipe_write(ios->io->pipe, &frame, __sizeof_ptr) != __sizeof_ptr);
    return 0;
XClean:
    return -1;
}

int xlio_stream_write(xlio_stream_t *ios, xline_t *frame)
{
    frame->flag = XMSG_FLAG_STREAM;
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
    ios->pos += size;
    return ios->size - ios->pos;
}