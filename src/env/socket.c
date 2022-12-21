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


static __atombool __init_socket = __false;

static inline __sint32 env_socket_init(void)
{
	if (__atom_try_lock(__init_socket)){
#if defined(OS_WINDOWS)
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    return WSAStartup(wVersionRequested, &wsaData); 
#endif
	}
	return 0;
}

static inline __sint32 env_socket_cleanup(void)
{
	if (__atom_unlock(__init_socket)){
#if defined(OS_WINDOWS)
    return WSACleanup();
#endif
	}
	return 0;
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

__sint32 env_socket_set_nonblock(__socket sock, int noblock)
{
	// 0-block, 1-no-block
#if defined(OS_WINDOWS)
	u_long arg = noblock;
	return ioctlsocket(sock, FIONBIO, &arg);
#else
	// http://stackoverflow.com/questions/1150635/unix-nonblocking-i-o-o-nonblock-vs-fionbio
	// Prior to standardization there was ioctl(...FIONBIO...) and fcntl(...O_NDELAY...) ...
	// POSIX addressed this with the introduction of O_NONBLOCK.
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, noblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
	//return ioctl(sock, FIONBIO, &noblock);
#endif
}

__socket env_socket_open()
{
    __socket sock;
	env_socket_init();
    __pass((sock = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
    __pass(socket_setopt(sock, SO_REUSEADDR, 1) == 0);
    __pass(socket_setopt(sock, SO_REUSEPORT, 1) == 0);
    return sock;
Reset:
    return -1;
}

void env_socket_close(__socket sock)
{
    shutdown(sock, 0);
#if defined(OS_WINDOWS)
    // MSDN:
    // If closesocket fails with WSAEWOULDBLOCK the socket handle is still valid,
    // and a disconnect is not initiated. The application must call closesocket again to close the socket.
    closesocket(sock);
#else
    close(sock);
#endif
}

__sockaddr_ptr env_socket_addr_create(char* host, __uint16 port)
{
	__sockaddr_ptr addr = (__sockaddr_ptr)malloc(sizeof(struct sockaddr_in));
	__pass(addr != NULL);
	memset(addr, 0, sizeof(struct sockaddr_in));
	struct sockaddr_in* in = (struct sockaddr_in*)addr;
    in->sin_family = AF_INET;
    in->sin_port = htons(port);
	if (host != NULL){
		__pass(inet_aton(host, &in->sin_addr) == 1);
	}else {
		in->sin_addr.s_addr = INADDR_ANY;
	}
	return addr;
Reset:
	if (addr)free(addr);
	return NULL;
}

void env_socket_addr_destroy(__sockaddr_ptr *pp_addr)
{
	if (pp_addr && *pp_addr){
		__sockaddr_ptr sock = *pp_addr;
		*pp_addr = NULL;
		free(sock);
	}
}

void env_socket_addr_copy(__sockaddr_ptr src, __sockaddr_ptr dst)
{
	assert(src != NULL && dst != NULL);
	memcpy(dst, src, sizeof(struct sockaddr_in));
}

__bool env_socket_addr_compare(__sockaddr_ptr a, __sockaddr_ptr b)
{
	assert(a != NULL && b != NULL);
	return ((struct sockaddr_in*)a)->sin_addr.s_addr == ((struct sockaddr_in*)b)->sin_addr.s_addr;
}

void env_socket_addr_get_ip(__sockaddr_ptr addr, __ipaddr *ip)
{
	assert(addr != NULL && ip != NULL);
	if (addr && ip){
		struct sockaddr_in* in = (struct sockaddr_in*)addr;
		ip->ip = in->sin_addr.s_addr;
		ip->port = ntohs(in->sin_port);
		// ip->sa = addr;
		// __logd("%u.%u.%u.%u\n", ((unsigned char*)&in->sin_addr.s_addr)[0], ((unsigned char*)&in->sin_addr.s_addr)[1],
		// ((unsigned char*)&in->sin_addr.s_addr)[2], ((unsigned char*)&in->sin_addr.s_addr)[3]);
	}
}

__sint32 env_socket_connect(__socket sock, __sockaddr_ptr addr)
{
    return connect(sock, (const struct sockaddr*)addr, (socklen_t)sizeof(struct sockaddr_in));
}

__sint32 env_socket_bind(__socket sock, __sockaddr_ptr addr)
{
    return bind(sock, (const struct sockaddr*)addr, (socklen_t)sizeof(struct sockaddr_in));
}

__sint32 env_socket_send(__socket sock, const void* buf, __uint64 size)
{
#if defined(OS_WINDOWS)
    return send(sock->sock, (const char*)buf, (int)size, 0);
#else
    return send(sock, buf, size, 0);
#endif
}

__sint32 env_socket_recv(__socket sock, void* buf, __uint64 size)
{
#if defined(OS_WINDOWS)
    return recv(sock->sock, (char*)buf, (int)size, 0);
#else
    return recv(sock, buf, size, 0);
#endif
}

__sint32 env_socket_sendto(__socket sock, const void* buf, __uint64 size, __sockaddr_ptr addr)
{
	static socklen_t len = sizeof(struct sockaddr_in);
#if defined(OS_WINDOWS)
    return sendto(sock, (const char*)buf, (int)size, 0, addr, len);
#else
    return sendto(sock, buf, size, 0, addr, len);
#endif
}

__sint32 env_socket_recvfrom(__socket sock, void* buf, __uint64 size, __sockaddr_ptr addr)
{
	static socklen_t len = sizeof(struct sockaddr_in);
#if defined(OS_WINDOWS)
    return recvfrom(sock, (char*)buf, (int)size, 0, addr, &len);
#else
	return recvfrom(sock, buf, size, 0, addr, &len);
#endif
}