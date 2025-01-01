#ifndef __XMSG_H__
#define __XMSG_H__

#include "xline.h"

#define XMSG_FLAG_RECV          0x00
#define XMSG_FLAG_POST          0x01
#define XMSG_FLAG_BACK          0x02
#define XMSG_FLAG_TIMEDOUT      0x03

typedef struct xmsg_ctx* xmsg_ctx_ptr;
typedef int(*xmsg_cb_ptr)(xline_t *msg, xmsg_ctx_ptr ctx);

// #define __xmsg_get_cb(msg)                  (xmsg_cb_ptr)(msg)->args[0]
#define __xmsg_get_ctx(msg)                 (xmsg_ctx_ptr)(msg)->args[1]
#define __xmsg_get_ipaddr(msg)              (__xipaddr_ptr)(msg)->args[2]
#define __xmsg_get_channel(msg)             (xchannel_ptr)(msg)->args[3]

// #define __xmsg_set_cb(msg, cb)              (msg)->args[0] = (cb)
#define __xmsg_set_ctx(msg, ctx)            (msg)->args[1] = (ctx)
#define __xmsg_set_ipaddr(msg, addr)        (msg)->args[2] = (addr)
#define __xmsg_set_channel(msg, channel)    (msg)->args[3] = (channel)



#endif