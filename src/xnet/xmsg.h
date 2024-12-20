#ifndef __XMSG_H__
#define __XMSG_H__

#include "xline.h"

#define XL_MSG_FLAG_RECV            0x00
#define XL_MSG_FLAG_SEND            0x01
#define XL_MSG_FLAG_BACK            0x02
#define XL_MSG_FLAG_TIMEOUT         0x03
#define XL_MSG_FLAG_BOOT            0x04

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