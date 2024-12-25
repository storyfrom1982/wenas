#ifndef __XLTP_H__
#define __XLTP_H__


#include "xnet/xmsg.h"
#include "xnet/xmsger.h"

typedef struct xltp xltp_t;

xline_t* xltp_make_req(xltp_t *xltp, const char *api, xapi_cb_ptr cb, xapi_ctx_ptr ctx);

int xltp_request(xltp_t *xltp, xline_t *msg);
int xltp_respose(xltp_t *xltp, xline_t *msg);
int xltp_register(xltp_t *xltp, const char *api, xapi_cb_ptr cb);
int xltp_bootstrap(xltp_t *xltp, xline_t *msg);
xltp_t* xltp_create(xapi_ctx_ptr ctx);
void xltp_free(xltp_t **pptr);


#endif //__XLTP_H__