#ifndef __XPEER_H__
#define __XPEER_H__

#include "xnet/xmsg.h"


int xpeer_echo(xpeer_t *peer, const char *host, uint16_t port);
int xpeer_bootstrap(xpeer_t *peer);

xpeer_t* xpeer_create();
void xpeer_free(xpeer_t**);



#endif //__XPEER_H__