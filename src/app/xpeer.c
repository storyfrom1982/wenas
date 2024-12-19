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

static inline xline_t* msg_maker(xpeer_t *peer, const char *api, msg_cb_t cb)
{
    xline_t *msg = xl_maker();
    __xcheck(msg == NULL);
    msg->id = peer->msgid++;
    __xmsg_set_cb(msg, cb);
    __xmsg_set_peer(msg, peer);
    __xmsg_set_xltp(msg, peer->xltp);
    __xcheck(xl_add_word(&msg, "api", api) == EENDED);
    __xcheck(xl_add_uint(&msg, "tid", msg->id) == EENDED);
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
    xline_t *msg = msg_maker(peer, "bye", NULL);
    __xcheck(msg == NULL);
    __xmsg_set_channel(msg, __xmsg_get_channel(res));
    xltp_respose(peer->xltp, msg);
    return 0;
XClean:
    if (msg){
        xl_free(&msg);
    }
    return -1;
}

static int api_hello(xline_t *msg)
{
    xpeer_t *peer = __xmsg_get_peer(msg);
    uint64_t tid = xl_find_uint(&peer->parser, "tid");
    xline_t *res = xl_maker();
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "echo");
    xl_add_uint(&res, "tid", tid);
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
    uint64_t tid = xl_find_uint(&peer->parser, "tid");
    xline_t *res = xl_maker();
    xl_add_word(&res, "api", "res");
    xl_add_word(&res, "req", "echo");
    xl_add_uint(&res, "tid", tid);
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
    xline_t *msg = msg_maker(peer, "echo", res_hello);
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
    xline_t *msg = msg_maker(peer, "echo", res_echo);
    __xcheck(msg == NULL);
    xl_add_word(&msg, "host", peer->ip);
    xl_add_uint(&msg, "port", peer->port);
    uint8_t uuid[32];
    xl_add_bin(&msg, "uuid", uuid, 32);
    __xcheck(xltp_request(peer->xltp, msg) != 0);
    return 0;
XClean:
    if (msg != NULL){
        xl_free(&msg);
    }
    return -1;
}


#include <stdio.h>
#include <string.h>
int main(int argc, char *argv[])
{
    const char *hostname = "healthtao.cn";
    uint16_t port = 9256;

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xpeer_t *peer = (xpeer_t*)calloc(1, sizeof(struct xpeer));
    __xcheck(peer == NULL);

    peer->xltp = xltp_create(peer);
    peer->running = true;

    
    __xcheck(xltp_register(peer->xltp, "echo", api_echo) != 0);
    __xcheck(xltp_register(peer->xltp, "hello", api_hello) != 0);

    peer->port = 9256;

    // __xapi->udp_addrinfo(peer->ip, hostname);
    // __xlogd("host ip = %s port=%u\n", peer->ip, peer->port);

    // const char *cip = "192.168.1.6";
    // const char *cip = "120.78.155.213";
    const char *cip = "47.92.77.19";
    // const char *cip = "47.99.146.226";
    // const char *cip = hostname;
    // const char *cip = "2409:8a14:8743:9750:350f:784f:8966:8b52";
    // const char *cip = "2409:8a14:8743:9750:7193:6fc2:f49d:3cdb";
    // const char *cip = "2409:8914:8669:1bf8:5c20:3ccc:1d88:ce38";

    mcopy(peer->ip, cip, slength(cip));
    peer->ip[slength(cip)] = '\0';

    char str[1024];
    char input[256];
    char command[256];
    char ip[256] = {0};
    uint64_t cid, key;
    while (peer->running)
    {
        printf("> "); // 命令提示符
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // 读取失败，退出循环
        }

        // 去掉输入字符串末尾的换行符
        input[strcspn(input, "\n")] = 0;

        // 分割命令和参数
        sscanf(input, "%s", command);

        if (strcmp(command, "echo") == 0) {
            req_echo(peer, peer->ip, peer->port);

        } else if (strcmp(command, "hello") == 0) {
            req_echo(peer, peer->ip, peer->port);

        } else if (strcmp(command, "exit") == 0) {
            __set_false(peer->running);
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        mclear(command, 256);
    }

    xltp_free(&peer->xltp);

    free(peer);

    xlog_recorder_close();

XClean:

    __xlogi("exit\n");

    return 0;
}
