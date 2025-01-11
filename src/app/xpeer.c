#include "xpeer.h"

#include "xapi/xapi.h"

#include "xltp.h"

typedef struct xltp_io {
    bool running;
    xltp_t *xltp;
    uint16_t port;
    char ip[46];
    uint16_t pub_port;
    char pub_ip[46];
    xline_t parser;
    xline_t msglist;
}xpeer_t;


static inline xline_t* xpeer_make_res(xpeer_t *peer, xline_t *req)
{
    req->wpos = 0;
    __xmsg_set_cb(req, NULL);
    peer->parser = xl_parser(&req->line);
    __xcheck((req->id = xl_find_uint(&peer->parser, "rid")) == XERR);
    __xcheck(xl_add_uint(&req, "rid", req->id) == XERR);
    xl_hold(req);
    return req;
XClean:
    return NULL;
}

static int res_hello(xline_t *res, xmsg_ctx_ptr peer)
{
    peer->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    return 0;
}

static int res_echo(xline_t *res, xmsg_ctx_ptr peer)
{
    peer->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    return 0;
XClean:
    return -1;
}

static int res_boot(xline_t *res, xmsg_ctx_ptr peer)
{
    peer->parser = xl_parser(&res->line);
    xl_printf(&res->line);
    return 0;
XClean:
    return -1;
}

static int api_hello(xline_t *msg, xmsg_ctx_ptr peer)
{
    __xcheck(msg == NULL);
    __xcheck(peer == NULL);
    xl_printf(&msg->line);
    xline_t *res = xpeer_make_res(peer, msg);
    __xcheck(res == NULL);
    xl_add_word(&res, "host", xchannel_get_ip(__xmsg_get_channel(msg)));
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

static int api_echo(xline_t *msg, xmsg_ctx_ptr peer)
{
    __xcheck(msg == NULL);
    __xcheck(peer == NULL);
    xl_printf(&msg->line);
    xline_t *res = xpeer_make_res(peer, msg);
    __xcheck(res == NULL);
    xl_add_word(&res, "host", xchannel_get_ip(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "port", xchannel_get_port(__xmsg_get_channel(msg)));
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->line);
    xl_free(&test);
    xl_add_uint(&res, "code", 200);
    // TODO 这里有可能会阻塞，xpeer 需要运行自己的线程
    xltp_respose(peer->xltp, res);
    return 0;
XClean:
    return -1;
}

static int api_boot(xline_t *msg, xmsg_ctx_ptr peer)
{
    __xcheck(msg == NULL);
    __xcheck(peer == NULL);
    xl_printf(&msg->line);
    xline_t *res = xpeer_make_res(peer, msg);
    __xcheck(res == NULL);
    xl_add_word(&res, "host", xchannel_get_ip(__xmsg_get_channel(msg)));
    xl_add_uint(&res, "port", xchannel_get_port(__xmsg_get_channel(msg)));
    uint8_t uuid[8192];
    xl_add_bin(&res, "uuid", uuid, 8192);
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
    xline_t *msg = xltp_make_req(peer->xltp, "hello", res_hello, peer);
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

int req_echo(xline_t *msg, xmsg_ctx_ptr ctx)
{
    xpeer_t *peer = (xpeer_t *)ctx;
    xl_clear(msg);
    __xmsg_set_cb(msg, res_echo);
    __xmsg_set_ctx(msg, ctx);
    xl_add_word(&msg, "api", "echo");
    xl_add_uint(&msg, "rid", 0);
    xline_t *test = xl_test(10);
    xl_add_obj(&msg, "test", &test->line);
    xl_free(&test);
    __xcheck(xltp_request(peer->xltp, msg) != 0);
    return 0;
XClean:
    return -1;
}

int xpeer_echo(xpeer_t *peer, const char *host, uint16_t port)
{
    // xline_t *msg = xltp_make_req(peer->xltp, "echo", res_echo, peer);
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    __xipaddr_ptr addr = __xapi->udp_host_to_addr(host, port);
    __xmsg_set_ipaddr(msg, addr);
    xl_add_ptr(&msg, "cb", req_echo);
    xl_add_ptr(&msg, "ctx", peer);
    xltp_post(peer->xltp, msg);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

int xpeer_bootstrap(xpeer_t *peer)
{
    xline_t *msg = xltp_make_req(peer->xltp, "boot", res_boot, peer);
    __xcheck(msg == NULL);
    uint8_t uuid[32];
    xl_add_bin(&msg, "uuid", uuid, 32);
    __xcheck(xltp_bootstrap(peer->xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}

xpeer_t* xpeer_create()
{
    xpeer_t *peer = (xpeer_t*)calloc(1, sizeof(xpeer_t));
    __xcheck(peer == NULL);
    peer->xltp = xltp_create(peer);
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