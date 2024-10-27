#ifndef __XMSGER_H__
#define __XMSGER_H__


#include "xmalloc.h"
#include "xline.h"


#define XMSG_MIN_SIZE       XLINEKV_SIZE


typedef struct xmsger* xmsger_ptr;
typedef struct xchannel* xchannel_ptr;


typedef struct xmsg {
    __atom_size ref;
    uint32_t flag;
    uint32_t streamid;
    uint64_t sendpos, recvpos, range;
    struct xchannel *channel;
    struct xchannel_ctx *ctx;
    struct xmsg *prev, *next;
    struct xlinekv lkv;
}*xmsg_ptr;


extern xmsg_ptr xmsg_maker();
extern xmsg_ptr xmsg_create(uint64_t size);
extern void xmsg_ref(xmsg_ptr msg);
extern void xmsg_final(xmsg_ptr msg);
extern void xmsg_free(xmsg_ptr msg);

#define xmsg_printf(msg) \
    do { \
        char buf[XLINEKV_SIZE] = {0}; \
        uint64_t pos = 0; \
        xl_format((&msg->lkv.line), "root", 1, buf, &pos, XLINEKV_SIZE); \
        __xlogd("len=%lu\n%s\n", pos, buf); \
    }while(0)

typedef struct xmsgercb {
    void *ctx;
    void (*on_channel_to_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_from_peer)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_break)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_channel_timeout)(struct xmsgercb*, xchannel_ptr channel);
    void (*on_msg_from_peer)(struct xmsgercb*, xchannel_ptr channel, xmsg_ptr msg);
    void (*on_msg_to_peer)(struct xmsgercb*, xchannel_ptr channel, xmsg_ptr msg);
}*xmsgercb_ptr;


extern xmsger_ptr xmsger_create(xmsgercb_ptr callback, int sock);
extern void xmsger_free(xmsger_ptr *pptr);
extern bool xmsger_send_message(xmsger_ptr msger, xchannel_ptr channel, xmsg_ptr msg);
// extern bool xmsger_send_stream(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
// extern bool xmsger_send_file(xmsger_ptr msger, xchannel_ptr channel, void *data, size_t size);
extern bool xmsger_connect(xmsger_ptr msger, const char *ip, uint16_t port, struct xchannel_ctx *ctx, xmsg_ptr firstmsg);
extern bool xmsger_disconnect(xmsger_ptr msger, xchannel_ptr channel);

extern bool xchannel_get_keepalive(xchannel_ptr channel);
extern const char* xchannel_get_ip(xchannel_ptr channel);
extern uint16_t xchannel_get_port(xchannel_ptr channel);
extern struct xchannel_ctx* xchannel_get_ctx(xchannel_ptr channel);
extern void xchannel_set_ctx(xchannel_ptr channel, struct xchannel_ctx*);


#endif //__XMSGER_H__