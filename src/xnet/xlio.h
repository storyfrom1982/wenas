#ifndef __XLIO_H__
#define __XLIO_H__

#include "xnet/xmsger.h"

#define XLIO_STREAM_TYPE_READ        0x01
#define XLIO_STREAM_TYPE_WRITE       0x02

typedef struct xlio xlio_t;
typedef struct xlio_stream xlio_stream_t;

xlio_t* xlio_create(xmsger_ptr msger);
void xlio_free(xlio_t **pptr);

xlio_stream_t* xlio_stream_maker(xlio_t *xlio, const char *file_path, int stream_type);
void xlio_stream_free(xlio_stream_t *ios);
int xlio_stream_ready(xlio_stream_t *ios, xline_t *frame);
int xlio_stream_read(xlio_stream_t *ios, xline_t *frame);
int xlio_stream_write(xlio_stream_t *ios, xline_t *frame);
uint64_t xlio_stream_length(xlio_stream_t *ios);
uint64_t xlio_stream_update(xlio_stream_t *ios, uint64_t size);

#endif //__XLIO_H__