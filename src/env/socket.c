#include "env/env.h"

#ifdef OS_WINDOWS

#include <Winsock2.h>
#include <WS2tcpip.h>
#include <ws2ipdef.h>

#define socket_invalid  INVALID_SOCKET
#define socket_error    SOCKET_ERROR

#if defined(_MSC_VER)
#pragma comment(lib, "Ws2_32.lib")
#pragma warning(push)
#pragma warning(disable: 6031) // warning C6031: Return value ignored: 'snprintf'
#endif

#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH

#ifndef ETIMEDOUT
    #define ETIMEDOUT 138
#endif

// IPv6 MTU
#ifndef IPV6_MTU_DISCOVER
    #define IPV6_MTU_DISCOVER   71
    #define IP_PMTUDISC_DO      1
    #define IP_PMTUDISC_DONT    2                                                                                                                                                                                 
#endif

#ifndef AI_V4MAPPED
    #define AI_V4MAPPED 0x00000800
#endif
#ifndef AI_NUMERICSERV
    #define AI_NUMERICSERV  0x00000008
#endif

#else

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#define socket_invalid  -1
#define socket_error    -1

#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

struct __socket{
    __sint32 connected;
    __sint32 sock;
    struct sockaddr local;
    struct sockaddr remote;
};

__sint32 env_socket_init(void)
{
#if defined(OS_WINDOWS)
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(2, 2);
    return WSAStartup(wVersionRequested, &wsaData);
#else
    return 0;
#endif
}

__sint32 env_socket_cleanup(void)
{
#if defined(OS_WINDOWS)
    return WSACleanup();
#else
    return 0;
#endif
}

static inline int socket_setopt(__sint32 sock, int optname, int enable)
{
#if defined(OS_WINDOWS)
    BOOL v = enable ? TRUE : FALSE;
    return setsockopt(sock, SOL_SOCKET, optname, (const char*)&v, sizeof(v));
#else
    return setsockopt(sock, SOL_SOCKET, optname, &enable, sizeof(enable));
#endif
}

__socket_ptr env_socket_creatre(__symptr host, __uint16 port)
{
    __socket_ptr sock = malloc(sizeof(struct __socket));
    __pass(sock != NULL);
    memset(sock, 0, sizeof(struct __socket));
    struct sockaddr_in* local = (struct sockaddr_in*)&sock->local;
    struct sockaddr_in* remote = (struct sockaddr_in*)&sock->remote;

    __pass((sock->sock = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
    __pass(socket_setopt(sock->sock, SO_REUSEADDR, 1) == 0);
    __pass(socket_setopt(sock->sock, SO_REUSEPORT, 1) == 0);

    local->sin_family = AF_INET;
    local->sin_port = htons(port);
    local->sin_addr.s_addr = INADDR_ANY;

    __logd("inet_aton %s\n", host);
    remote->sin_family = AF_INET;
    remote->sin_port = htons(port);
    __pass(inet_aton(host, &remote->sin_addr) == 1);

    // __pass(socket_addr_from_ipv4(remote, host, port) == 0);


    // __pass(inet_pton(AF_INET, host, &remote->sin_addr) == 1);
    inet_ntop(AF_INET, &remote->sin_addr, host, 46);
    __logd("inet_ntop %s\n", host);
    return sock;

Reset:
    return NULL;
}

void env_socket_destroy(__socket_ptr *pp_sock)
{
if (pp_sock && *pp_sock){
    __socket_ptr sock = *pp_sock;
    *pp_sock = NULL;
    shutdown(sock->sock, 0);
#if defined(OS_WINDOWS)
    // MSDN:
    // If closesocket fails with WSAEWOULDBLOCK the socket handle is still valid,
    // and a disconnect is not initiated. The application must call closesocket again to close the socket.
    closesocket(sock->sock);
#else
    close(sock->sock);
#endif
    free(sock);
}
}

__sint32 env_socket_connect(__socket_ptr sock)
{
    __pass(connect(sock->sock, (const struct sockaddr*)&sock->remote, (socklen_t)sizeof(struct sockaddr_in)) == 0);
    sock->connected = 1;
    return 0;
Reset:
    return -1;
}

__sint32 env_socket_bind(__socket_ptr sock)
{
    return bind(sock->sock, (const struct sockaddr*)&sock->local, (socklen_t)sizeof(struct sockaddr_in));
}

__sint32 env_socket_send(__socket_ptr sock, const void* buf, __uint64 size)
{
#if defined(OS_WINDOWS)
    return send(sock->sock, (const char*)buf, (int)size, 0);
#else
    return send(sock->sock, buf, size, 0);
#endif
}

__sint32 env_socket_recv(__socket_ptr sock, void* buf, __uint64 size)
{
#if defined(OS_WINDOWS)
    return recv(sock->sock, (char*)buf, (int)size, 0);
#else
    return recv(sock->sock, buf, size, 0);
#endif
}

__sint32 env_socket_sendto(__socket_ptr sock, const void* buf, __uint64 size)
{
#if defined(OS_WINDOWS)
    return sendto(sock->sock, (const char*)buf, (int)size, 0, (struct sockaddr *)&sock->remote, (socklen_t)sizeof(struct sockaddr_in));
#else
    return sendto(sock->sock, buf, size, 0, (struct sockaddr *)&sock->remote, (socklen_t)sizeof(struct sockaddr_in));
#endif
}

__sint32 env_socket_recvfrom(__socket_ptr sock, void* buf, __uint64 size)
{
    socklen_t addrlen;
#if defined(OS_WINDOWS)
    return recvfrom(sock->sock, (char*)buf, (int)size, 0, (struct sockaddr *)&sock->remote, (socklen_t*)&sock->remote.sa_len);
#else
    return recvfrom(sock->sock, buf, size, 0, (struct sockaddr *)&sock->remote, (socklen_t*)&sock->remote.sa_len);
#endif
}