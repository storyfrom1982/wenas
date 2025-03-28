#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"
#include "xmsg.h"


#define XPACK_TYPE_ACK      0x00
#define XPACK_TYPE_REQ      0x01
#define XPACK_TYPE_MSG      0x02
#define XPACK_TYPE_RES      0x03
#define XPACK_TYPE_FLUSH    0xF0
#define XPACK_TYPE_LOCAL    0xF1


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;
// typedef struct xchannel_ctx* xchannel_ctx_ptr;


typedef struct xmsgercb {
    void *ctx;
    int (*on_message_to_peer)(struct xmsgercb*, xchannel_ptr channel, xframe_t *msg);
    int (*on_message_from_peer)(struct xmsgercb*, xchannel_ptr channel, xframe_t *msg);
    int (*on_message_timedout)(struct xmsgercb*, xchannel_ptr channel, xframe_t *msg);
}*xmsgercb_ptr;


extern xmsger_ptr xmsger_create(xmsgercb_ptr callback, uint16_t port);
extern void xmsger_free(xmsger_ptr *pptr);
extern int xmsger_connect(xmsger_ptr msger, xframe_t *msg);
extern int xmsger_disconnect(xmsger_ptr msger, xframe_t *msg);
extern int xmsger_send(xmsger_ptr msger, xchannel_ptr channel, xframe_t *msg);
extern int xmsger_flush(xmsger_ptr msger, xchannel_ptr channel);

// extern const char* xchannel_get_ip(xchannel_ptr channel);
// extern uint16_t xchannel_get_port(xchannel_ptr channel);
extern xframe_t* xchannel_get_req(xchannel_ptr channel);
extern void* xchannel_get_ctx(xchannel_ptr channel);
extern void xchannel_set_ctx(xchannel_ptr channel, void*);


#endif //__XMSGER_H__