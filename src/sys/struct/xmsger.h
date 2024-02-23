#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;

typedef struct xmsglistener {
    void *ctx;
    // TODO 使用cid替换channel指针
    void (*onChannelToPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onChannelFromPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onChannelBreak)(struct xmsglistener*, xchannel_ptr channel);
    void (*onChannelTimeout)(struct xmsglistener*, xchannel_ptr channel);
    void (*onMessageFromPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg, size_t len);
    void (*onMessageToPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg);
}*xmsglistener_ptr;

extern xmsger_ptr xmsger_create(xmsglistener_ptr listener);
extern void xmsger_free(xmsger_ptr *pptr);
extern bool xmsger_ping(xmsger_ptr msger, xchannel_ptr channel);
extern bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_send_stream1(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_send_stream2(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, const char *addr, uint16_t port);
extern bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel);


#endif //__XMSGER_H__