#include "env/env.h"

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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define socket_invalid  -1
#define socket_error    -1

struct __socket{
    int32_t connected;
    int32_t sock;
    struct sockaddr local;
    struct sockaddr remote;
};


static __atombool __init_socket = false;


static inline int socket_setopt(int32_t sock, int optname, int enable)
{
	return setsockopt(sock, SOL_SOCKET, optname, &enable, sizeof(enable));
}

int32_t env_socket_set_nonblock(__socket sock, int noblock)
{
	// http://stackoverflow.com/questions/1150635/unix-nonblocking-i-o-o-nonblock-vs-fionbio
	// Prior to standardization there was ioctl(...FIONBIO...) and fcntl(...O_NDELAY...) ...
	// POSIX addressed this with the introduction of O_NONBLOCK.
	int flags = fcntl(sock, F_GETFL, 0);
	return fcntl(sock, F_SETFL, noblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
	//return ioctl(sock, FIONBIO, &noblock);
}

__socket env_socket_open()
{
    __socket sock;
    __pass((sock = socket(PF_INET, SOCK_DGRAM, 0)) > 0);
    __pass(socket_setopt(sock, SO_REUSEADDR, 1) == 0);
	__pass(socket_setopt(sock, SO_REUSEPORT, 1) == 0);
    return sock;
Reset:
    return -1;
}

void env_socket_close(__socket sock)
{
	close(sock);
}

__sockaddr_ptr env_socket_addr_create(char* host, uint16_t port)
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

bool env_socket_addr_compare(__sockaddr_ptr a, __sockaddr_ptr b)
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

int32_t env_socket_connect(__socket sock, __sockaddr_ptr addr)
{
    return connect(sock, (const struct sockaddr*)addr, (socklen_t)sizeof(struct sockaddr_in));
}

int32_t env_socket_bind(__socket sock, __sockaddr_ptr addr)
{
    return bind(sock, (const struct sockaddr*)addr, (socklen_t)sizeof(struct sockaddr_in));
}

int32_t env_socket_send(__socket sock, const void* buf, uint64_t size)
{
	return send(sock, buf, size, 0);
}

int32_t env_socket_recv(__socket sock, void* buf, uint64_t size)
{
	return recv(sock, buf, size, 0);
}

int32_t env_socket_sendto(__socket sock, const void* buf, uint64_t size, __sockaddr_ptr addr)
{
	static socklen_t len = sizeof(struct sockaddr_in);
	return sendto(sock, buf, size, 0, addr, len);
}

int32_t env_socket_recvfrom(__socket sock, void* buf, uint64_t size, __sockaddr_ptr addr)
{
	static socklen_t len = sizeof(struct sockaddr_in);
	return recvfrom(sock, buf, size, 0, addr, &len);
}