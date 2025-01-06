#include "xlio.h"

#include "xpipe.h"

typedef struct xlio_stream {
    int flag;
    __xfile_t fd;
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
                    frame = xl_creator(framelen);
                    __xcheck(frame == NULL);
                    ret = __xapi->fs_read(stream->fd, frame->ptr, framelen);
                    if (ret > 0){
                        frame->wpos = ret;
                        frame->type = XPACK_TYPE_MSG;
                        __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);
                    }
                }


            }else if(msg->flag == XMSG_FLAG_STREAM){

                frame = msg;
                __xcheck(frame->size != framelen);
                ret = __xapi->fs_read(stream->fd, frame->ptr, framelen);
                if (ret > 0){
                    frame->wpos = ret;
                    frame->type = XPACK_TYPE_MSG;
                    __xcheck(xmsger_send(io->msger, __xmsg_get_channel(msg), frame) != 0);
                }else {
                    xl_free(&frame);
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

xlio_stream_t* xlio_stream_maker(xlio_t *io, const char *file_path, int stream_type)
{
    xlio_stream_t* stream = (xlio_stream_t*)malloc(sizeof(xlio_stream_t));
    __xcheck(stream == NULL);
    __xcheck(!__xapi->fs_isfile(file_path));
    stream->fd = __xapi->fs_open(file_path, XAPI_FS_FLAG_READ, 0644);
    __xcheck(stream->fd < 0);
    stream->flag = stream_type;
    stream->pos = 0;
    stream->size = __xapi->fs_size(file_path);
    __xlogd("file size == %lu\n", stream->size);
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
            __xapi->fs_close(ios->fd);
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