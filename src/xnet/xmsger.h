#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"

typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;


#define XMSG_CMD_SIZE       64

typedef struct xmessage {
    uint32_t type;
    uint32_t sid;
    void *data;
    size_t wpos, rpos, len, range;
    xchannel_ptr channel;
    struct xmessage *next;
    uint8_t cmd[XMSG_CMD_SIZE];
}*xmessage_ptr;


typedef struct xmsgercb {
    void *ctx;
    void (*on_channel_to_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_from_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_break)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_timeout)(struct xmsgercb*, xchannel_ptr channel, xmessage_ptr resendmsg);
    void (*on_msg_from_peer)(struct xmsgercb*, xchannel_ptr channel, void *msg, size_t len);
    void (*on_msg_to_peer)(struct xmsgercb*, xchannel_ptr channel, void *msg, size_t len);
}*xmsgercb_ptr;


extern xmsger_ptr xmsger_create(xmsgercb_ptr callback, int sock);
extern void xmsger_free(xmsger_ptr *pptr);
extern void xmsger_notify(xmsger_ptr msger, uint64_t timing);
extern bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_send_stream(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_send_file(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, __xipaddr_ptr addr, void *peerctx);
extern bool xmsger_reconnect(xmsger_ptr msger, __xipaddr_ptr addr, void *peerctx, xmessage_ptr resendmsg);
extern bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel);
extern void* xmsger_get_channel_ctx(xchannel_ptr channel);
extern void xmsger_set_channel_ctx(xchannel_ptr channel, void *ctx);
extern __xipaddr_ptr xmsger_get_channel_ipaddr(xchannel_ptr channel);


#endif //__XMSGER_H__