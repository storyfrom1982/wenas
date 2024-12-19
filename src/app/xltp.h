#ifndef __XLTP_H__
#define __XLTP_H__


#include "xnet/xmsg.h"
#include "xnet/xmsger.h"

int xltp_request(xltp_t *xltp, xline_t *msg);
int xltp_respose(xltp_t *xltp, xline_t *msg);
int xltp_register(xltp_t *xltp, const char *api, api_cb_t cb);
xltp_t* xltp_create(xpeer_t *peer);
void xltp_free(xltp_t **pptr);


#endif //__XLTP_H__