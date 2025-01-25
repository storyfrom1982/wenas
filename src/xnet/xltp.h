#ifndef __XLTP_H__
#define __XLTP_H__


#include "xnet/xmsg.h"
#include "xnet/xmsger.h"

typedef struct xltp xltp_t;

int xltp_put(xltp_t *xltp, const char *local_path, const char *uri, __xipaddr_ptr ipaddr);
int xltp_get(xltp_t *xltp, const char *local_path, const char *uri, __xipaddr_ptr ipaddr);
int xltp_echo(xltp_t *tp, const char *ip, uint16_t port);
xltp_t* xltp_create(int boot);
void xltp_free(xltp_t **pptr);

#endif //__XLTP_H__