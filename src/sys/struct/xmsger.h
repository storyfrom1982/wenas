#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"

#include <sys/struct/xtree.h>
#include <sys/struct/xbuf.h>


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;
typedef struct xchannellist* xchannellist_ptr;
typedef struct xmsglistener* xmsglistener_ptr;


struct xmsglistener {
    void *ctx;
    // TODO 使用cid替换channel指针
    void (*onConnectionToPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onConnectionFromPeer)(struct xmsglistener*, xchannel_ptr channel);
    void (*onDisconnection)(struct xmsglistener*, xchannel_ptr channel);
    void (*onMessageFromPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg, size_t len);
    void (*onMessageToPeer)(struct xmsglistener*, xchannel_ptr channel, void *msg);
};


extern xmsger_ptr xmsger_create(xmsglistener_ptr listener);
extern void xmsger_free(xmsger_ptr *pptr);
extern bool xmsger_ping(xmsger_ptr msger, xchannel_ptr channel);
extern bool xmsger_send(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, const char *addr, uint16_t port);
extern bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel);


#endif //__XMSGER_H__