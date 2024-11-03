#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"
#include "xline.h"


#define XMSG_MIN_SIZE       XLINEKV_SIZE


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;


typedef struct xmsgercb {
    void *ctx;
    void (*on_connect_to_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_connect_from_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_connect_timeout)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_msg_from_peer)(struct xmsgercb*, xchannel_ptr channel, xlinekv_ptr xkv);
    void (*on_msg_to_peer)(struct xmsgercb*, xchannel_ptr channel, xlinekv_ptr xkv);
    void (*on_msg_timeout)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_disconnect)(struct xmsgercb*, xchannel_ptr channel);
}*xmsgercb_ptr;


extern xmsger_ptr xmsger_create(xmsgercb_ptr callback);
extern void xmsger_free(xmsger_ptr *pptr);
extern bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, xlinekv_ptr msg);
extern bool xmsger_connect(xmsger_ptr msger, const char *ip, uint16_t port, void *ctx, xlinekv_ptr firstmsg);
extern bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel);

extern bool xchannel_get_keepalive(xchannel_ptr channel);
extern const char* xchannel_get_ip(xchannel_ptr channel);
extern uint16_t xchannel_get_port(xchannel_ptr channel);
extern struct xchannel_ctx* xchannel_get_ctx(xchannel_ptr channel);
extern void xchannel_set_ctx(xchannel_ptr channel, struct xchannel_ctx*);


#endif //__XMSGER_H__