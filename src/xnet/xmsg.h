#ifndef __XMSG_H__
#define __XMSG_H__

#include "xline.h"

typedef struct xltp xltp_t;
typedef struct xpeer xpeer_t;
typedef struct xchannel xchannel_t;

typedef int(*api_cb_t)(xline_t *req);
typedef int(*msg_cb_t)(xline_t *res);

#define __xmsg_get_cb(msg)                  (msg_cb_t)(msg)->args[0]
#define __xmsg_get_peer(msg)                (xpeer_t*)(msg)->args[1]
#define __xmsg_get_xltp(msg)                (xltp_t*)(msg)->args[2]
#define __xmsg_get_channel(msg)             (xchannel_t*)(msg)->args[3]

#define __xmsg_set_cb(msg, cb)              (msg)->args[0] = (cb)
#define __xmsg_set_peer(msg, peer)          (msg)->args[1] = (peer)
#define __xmsg_set_xltp(msg, xltp)          (msg)->args[2] = (xltp)
#define __xmsg_set_channel(msg, channel)    (msg)->args[3] = (channel)



#endif