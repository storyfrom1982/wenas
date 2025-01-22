#ifndef __XLIO_H__
#define __XLIO_H__

#include "xnet/xmsger.h"

#define IOSTREAM_TYPE_UPLOAD        0
#define IOSTREAM_TYPE_DOWNLOAD      1
#define IOSTREAM_TYPE_VIDEO_CALL    2

// typedef struct ios {
//     xchannel_ptr channel;
//     int (*post_msg)(struct ios, xline_t *msg);
// };

typedef struct xlio xlio_t;
typedef struct xlio_stream xlio_stream_t;

xlio_t* xlio_create(xmsger_ptr msger);
void xlio_free(xlio_t **pptr);
int xlio_api_upload(xlio_t *io, xline_t *req);
int xlio_upload(xlio_t *io, const char *local_uri, const char *remote_uri, __xipaddr_ptr ipaddr);
int xlio_path_scanner(const char *path, xline_t **frame);

// #define XLIO_STREAM_TYPE_SEND       0
// #define XLIO_STREAM_TYPE_RECV       1

xlio_stream_t* xlio_stream_maker(xlio_t *xlio, xline_t *frame, int stream_type);
void xlio_stream_free(xlio_stream_t *ios);
int xlio_stream_post(xlio_stream_t *ios, xline_t *frame);
int xlio_stream_read(xlio_stream_t *ios, xline_t *frame);
int xlio_stream_write(xlio_stream_t *ios, xline_t *frame);
uint64_t xlio_stream_length(xlio_stream_t *ios);
uint64_t xlio_stream_update(xlio_stream_t *ios, uint64_t size);

#endif //__XLIO_H__