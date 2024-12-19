#include "xpeer.h"

#include "xapi/xapi.h"

#include "xltp.h"

typedef struct xpeer {
    bool running;
    xltp_t *xltp;
    uint16_t port;
    char ip[46];
    uint16_t pub_port;
    char pub_ip[46];
    uint64_t msgid;
    xline_t parser;    
    xline_t msglist;
}xpeer_t;

static inline xline_t* msg_make_req(xpeer_t *peer, const char *api, msg_cb_t cb)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->id = peer->msgid++;
    __xmsg_set_cb(msg, cb);
    __xmsg_set_peer(msg, peer);
    __xmsg_set_xltp(msg, peer->xltp);
    __xcheck(xl_add_word(&msg, "api", api) == EENDED);
    __xcheck(xl_add_uint(&msg, "mid", msg->id) == EENDED);
    return msg;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return NULL;
}

static inline xline_t* msg_make_res(xpeer_t *peer, xline_t *msg, uint64_t mid)
{
    xl_hold(msg);
    msg->wpos = 0;
    msg->id = mid;
    __xmsg_set_cb(msg, NULL);
    __xcheck(xl_add_uint(&msg, "mid", msg->id) == EENDED);
    xl_printf(&msg->data);
    return msg;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return NULL;
}

static int res_hello(xline_t *res)
{
    xpeer_t *peer = __xmsg_get_peer(res);
    peer->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
}

static int res_echo(xline_t *res)
{
    xpeer_t *peer = __xmsg_get_peer(res);
    peer->parser = xl_parser(&res->data);
    xl_printf(&res->data);
    return 0;
XClean:
    return -1;
}

static int api_hello(xline_t *msg)
{
    xpeer_t *peer = __xmsg_get_peer(msg);
    xl_parser(&msg->data);
    uint64_t mid = xl_find_uint(&peer->parser, "mid");
    xline_t *res = msg_make_res(peer, msg, mid);
    xl_add_word(&res, "host", xchannel_get_host(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "port", xchannel_get_port(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "code", 200);
    xltp_respose(peer->xltp, res);
    return 0;
XClean:
    if (res != NULL){
        xl_free(&res);
    }
    return -1;
}

static int api_echo(xline_t *msg)
{
    xpeer_t *peer = __xmsg_get_peer(msg);
    xl_parser(&msg->data);
    uint64_t mid = xl_find_uint(&peer->parser, "mid");
    xline_t *res = msg_make_res(peer, msg, mid);
    xl_add_word(&res, "host", xchannel_get_host(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "port", xchannel_get_port(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "code", 200);
    xltp_respose(peer->xltp, res);
    return 0;
XClean:
    if (res != NULL){
        xl_free(&res);
    }
    return -1;
}

static int req_hello(xpeer_t *peer)
{
    xline_t *msg = msg_make_req(peer, "echo", res_hello);
    __xcheck(msg == NULL);
    xl_add_word(&msg, "host", peer->ip);
    xl_add_uint(&msg, "port", peer->port);
    uint8_t uuid[4096];
    xl_add_bin(&msg, "uuid", uuid, 4096);
    __xcheck(xltp_request(peer->xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int req_echo(xpeer_t *peer, const char *host, uint16_t port)
{
    xline_t *msg = msg_make_req(peer, "echo", res_echo);
    __xcheck(msg == NULL);
    xl_add_word(&msg, "host", host);
    xl_add_uint(&msg, "port", port);
    uint8_t uuid[32];
    xl_add_bin(&msg, "uuid", uuid, 32);
    xl_printf(&msg->data);
    __xcheck(xltp_request(peer->xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xpeer_send_echo(xpeer_t *peer, const char *host, uint16_t port)
{
    return req_echo(peer, host, port);
}

xpeer_t* xpeer_create()
{
    xpeer_t *peer = (xpeer_t*)calloc(1, sizeof(struct xpeer));
    __xcheck(peer == NULL);
    peer->xltp = xltp_create(peer);
    __xcheck(xltp_register(peer->xltp, "echo", api_echo) != 0);
    __xcheck(xltp_register(peer->xltp, "hello", api_hello) != 0);
    return peer;
XClean:
    if (peer){
        xpeer_free(&peer);
    }
    return NULL;
}

void xpeer_free(xpeer_t **pptr)
{
    if (pptr && *pptr){
        xpeer_t *peer = *pptr;
        xltp_free(&peer->xltp);
        free(peer);
    }
}