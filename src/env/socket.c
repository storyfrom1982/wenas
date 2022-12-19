/**
 * Kinesis Video Producer Host Info
 */
#include "env/env.h"

#ifdef OS_WINDOWS

#include <Winsock2.h>
#include <WS2tcpip.h>
#include <ws2ipdef.h>

#define socket_invalid	INVALID_SOCKET
#define socket_error	SOCKET_ERROR

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
	#define IPV6_MTU_DISCOVER	71
	#define IP_PMTUDISC_DO		1
	#define IP_PMTUDISC_DONT	2
#endif

#ifndef AI_V4MAPPED
	#define AI_V4MAPPED	0x00000800
#endif
#ifndef AI_NUMERICSERV
	#define AI_NUMERICSERV	0x00000008
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

#define socket_invalid	-1
#define socket_error	-1

#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


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

__sint32 env_socket_geterror(void)
{
#if defined(OS_WINDOWS)
	return WSAGetLastError();
#else
	return errno;
#endif
}

env_socket_t env_socket_tcp(void)
{
	return socket(PF_INET, SOCK_STREAM, 0);
}

env_socket_t env_socket_udp(void)
{
	return socket(PF_INET, SOCK_DGRAM, 0);
}

__sint32 env_socket_shutdown(env_socket_t sock, __sint32 flag)
{
	return shutdown(sock, flag);
}

__sint32 env_socket_close(env_socket_t sock)
{
#if defined(OS_WINDOWS)
	// MSDN:
	// If closesocket fails with WSAEWOULDBLOCK the socket handle is still valid, 
	// and a disconnect is not initiated. The application must call closesocket again to close the socket. 
	return closesocket(sock);
#else
	return close(sock);
#endif
}

__ret env_socket_connect(env_socket_t sock, env_sockaddr_ptr addr, env_socklen_t addrlen)
{
	return connect(sock, (const struct sockaddr*)addr, addrlen);
}

__ret env_socket_bind(env_socket_t sock, env_sockaddr_ptr addr, env_socklen_t addrlen)
{
	return bind(sock, (const struct sockaddr*)addr, addrlen);
}

__ret env_socket_listen(env_socket_t sock, __sint32 backlog)
{
	return listen(sock, backlog);
}

env_socket_t env_socket_accept(env_socket_t sock, env_sockaddr_ptr addr, env_socklen_t* addrlen)
{
	return accept(sock, (struct sockaddr*)addr, (socklen_t*)addrlen);
}

__ret env_socket_send(env_socket_t sock, const void* buf, __uint64 len, __sint32 flags)
{
#if defined(OS_WINDOWS)
	return send(sock, (const char*)buf, (int)len, flags);
#else
	return (int)send(sock, buf, len, flags);
#endif
}

__ret env_socket_recv(env_socket_t sock, void* buf, __uint64 len, __sint32 flags)
{
#if defined(OS_WINDOWS)
	return recv(sock, (char*)buf, (int)len, flags);
#else
	return recv(sock, buf, len, flags);
#endif
}

__ret env_socket_sendto(env_socket_t sock, const void* buf, __uint64 len, __sint32 flags, env_sockaddr_ptr to, env_socklen_t tolen)
{
#if defined(OS_WINDOWS)
	return sendto(sock, (const char*)buf, (int)len, flags, to, tolen);
#else
	return sendto(sock, buf, len, flags, (struct sockaddr *)to, (socklen_t)tolen);
#endif    
}

__ret env_socket_recvfrom(env_socket_t sock, void* buf, __uint64 len, __sint32 flags, env_sockaddr_ptr from, env_socklen_t* fromlen)
{
    #if defined(OS_WINDOWS)
	return recvfrom(sock, (char*)buf, (int)len, flags, from, fromlen);
#else
	return recvfrom(sock, buf, len, flags, (struct sockaddr *)from, (socklen_t*)fromlen);
#endif
}

__ret env_socket_ip2addr(env_sockaddr_ptr sockaddr, const __sym* ip, __uint16 port)
{
	int r;
	char portstr[16];
	struct addrinfo hints, *addr;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
//	hints.ai_flags = AI_ADDRCONFIG;
	snprintf(portstr, sizeof(portstr), "%hu", port);
	r = getaddrinfo(ip, portstr, &hints, &addr);
	if (0 != r)
		return r;

	// fixed ios getaddrinfo don't set port if node is ipv4 address
	env_socket_addr_set_port(addr->ai_addr, (socklen_t)addr->ai_addrlen, port);
	assert(sizeof(struct sockaddr_in) == addr->ai_addrlen);
	memcpy(sockaddr, addr->ai_addr, addr->ai_addrlen);
	freeaddrinfo(addr);
	return 0;
}

__ret env_socket_addr2ip(env_sockaddr_ptr sa, env_socklen_t salen, __sym ip[ENV_SOCKET_ADDRLEN], __uint16* port)
{
	if (AF_INET == ((struct sockaddr*)sa)->sa_family)
	{
		struct sockaddr_in* in = (struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		inet_ntop(AF_INET, &in->sin_addr, ip, ENV_SOCKET_ADDRLEN);
		if(port) *port = ntohs(in->sin_port);
	}
	else if (AF_INET6 == ((struct sockaddr*)sa)->sa_family)
	{
		struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		inet_ntop(AF_INET6, &in6->sin6_addr, ip, ENV_SOCKET_ADDRLEN);
		if (port) *port = ntohs(in6->sin6_port);
	}
	else
	{
		return -1; // unknown address family
	}

	(void)salen;
	return 0;
}

__ret env_socket_addr_name(env_sockaddr_ptr sa, env_socklen_t salen, __sym* host, env_socklen_t hostlen)
{
    return getnameinfo(sa, salen, host, hostlen, NULL, 0, 0);
}

__ret env_socket_addr_set_port(env_sockaddr_ptr sa, env_socklen_t salen, __uint16 port)
{
	if (AF_INET == ((struct sockaddr*)sa)->sa_family)
	{
		struct sockaddr_in* in = (struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		in->sin_port = htons(port);
	}
	else if (AF_INET6 == ((struct sockaddr*)sa)->sa_family)
	{
		struct sockaddr_in6* in6 = (struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		in6->sin6_port = htons(port);
	}
	else
	{
		assert(0);
		return -1;
	}

	(void)salen;
	return 0;
}

__ret env_socket_addr_is_local(env_sockaddr_ptr sa, env_socklen_t salen)
{
	if (AF_INET == ((struct sockaddr*)sa)->sa_family)
	{
		// unspecified: 0.0.0.0
		// loopback: 127.x.x.x
		// link-local unicast: 169.254.x.x
		const struct sockaddr_in* in = (const struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		return 0 == in->sin_addr.s_addr || 127 == (htonl(in->sin_addr.s_addr) >> 24) || (0xA9FE == (htonl(in->sin_addr.s_addr) >> 16)) ? 1 : 0;
	}
	else if (AF_INET6 == ((struct sockaddr*)sa)->sa_family)
	{		
		// unspecified: ::
		// loopback: ::1
		// link-local unicast: 0xFE 0x80
		// link-local multicast: 0xFF 0x01/0x02
		static const unsigned char ipv6_unspecified[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // IN6ADDR_ANY_INIT
		static const unsigned char ipv6_loopback[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		return 0 == memcmp(ipv6_unspecified, in6->sin6_addr.s6_addr, sizeof(ipv6_unspecified)) // IN6_IS_ADDR_UNSPECIFIED
			|| 0 == memcmp(ipv6_loopback, in6->sin6_addr.s6_addr, sizeof(ipv6_loopback)) // IN6_IS_ADDR_LOOPBACK
			|| (in6->sin6_addr.s6_addr[0] == 0xfe && (in6->sin6_addr.s6_addr[1] & 0xc0) == 0x80) // IN6_IS_ADDR_LINKLOCAL
			|| (in6->sin6_addr.s6_addr[0] == 0xff && ((in6->sin6_addr.s6_addr[1] & 0x0f) == 0x01 || (in6->sin6_addr.s6_addr[1] & 0x0f) == 0x02)) // IN6_IS_ADDR_MULTICAST
			? 1 : 0;
	}
	else
	{
		assert(0);
	}

	(void)salen;
	return 0;    
}

__ret env_socket_addr_is_multicast(env_sockaddr_ptr sa, env_socklen_t salen)
{
	if (AF_INET == ((struct sockaddr*)sa)->sa_family)
	{
		// IN_MULTICAST
		// 224.x.x.x ~ 239.x.x.x
		// b1110xxxx xxxxxxxx xxxxxxxx xxxxxxxx
		const struct sockaddr_in* in = (const struct sockaddr_in*)sa;
		assert(sizeof(struct sockaddr_in) == salen);
		return (ntohl(in->sin_addr.s_addr) & 0xf0000000) == 0xe0000000 ? 1 : 0;
	}
	else if (AF_INET6 == ((struct sockaddr*)sa)->sa_family)
	{
		// FFxx::/8
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)sa;
		assert(sizeof(struct sockaddr_in6) == salen);
		return in6->sin6_addr.s6_addr[0] == 0xff ? 1 : 0;
	}
	else
	{
		assert(0);
	}

	(void)salen;
	return 0;
}

__ret env_socket_addr_compare(env_sockaddr_ptr sa, env_sockaddr_ptr sb)
{
	if(((struct sockaddr*)sa)->sa_family != ((struct sockaddr*)sb)->sa_family)
		return ((struct sockaddr*)sa)->sa_family - ((struct sockaddr*)sb)->sa_family;

	// https://opensource.apple.com/source/postfix/postfix-197/postfix/src/util/sock_addr.c
	switch (((struct sockaddr*)sa)->sa_family)
	{
	case AF_INET:
		return ((struct sockaddr_in*)sa)->sin_port != ((struct sockaddr_in*)sb)->sin_port ? 
			((struct sockaddr_in*)sa)->sin_port - ((struct sockaddr_in*)sb)->sin_port : 
			memcmp(&((struct sockaddr_in*)sa)->sin_addr, &((struct sockaddr_in*)sb)->sin_addr, sizeof(struct in_addr));

	case AF_INET6:
		return ((struct sockaddr_in6*)sa)->sin6_port != ((struct sockaddr_in6*)sb)->sin6_port ?
			((struct sockaddr_in6*)sa)->sin6_port - ((struct sockaddr_in6*)sb)->sin6_port :
			memcmp(&((struct sockaddr_in6*)sa)->sin6_addr, &((struct sockaddr_in6*)sb)->sin6_addr, sizeof(struct in6_addr));

#if defined(OS_LINUX) || defined(OS_MAC) // Windows build 17061
	// https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
	case AF_UNIX:	return memcmp(sa, sb, sizeof(struct sockaddr_un));
#endif
	default:		return -1;
	}
}
__ret env_socket_addr_len(env_sockaddr_ptr addr)
{
	switch (((struct sockaddr*)addr)->sa_family)
	{
	case AF_INET:	return sizeof(struct sockaddr_in);
	case AF_INET6:	return sizeof(struct sockaddr_in6);
#if defined(OS_LINUX) || defined(OS_MAC)// Windows build 17061
		// https://blogs.msdn.microsoft.com/commandline/2017/12/19/af_unix-comes-to-windows/
	case AF_UNIX:	return sizeof(struct sockaddr_un);
#endif
#if defined(AF_NETLINK)
	//case AF_NETLINK:return sizeof(struct sockaddr_nl);
#endif
	default: return 0;
	}
}

__ret env_socket_multicast_join(env_socket_t sock, const __sym* group, const __sym* local)
{
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr));
}

__ret env_socket_multicast_leave(env_socket_t sock, const __sym* group, const __sym* local)
{
	struct ip_mreq imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr));
}

__ret env_socket_multicast_join_source(env_socket_t sock, const __sym* group, const __sym* source, const __sym* local)
{
	struct ip_mreq_source imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, source, &imr.imr_sourceaddr);
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &imr, sizeof(imr));
}

__ret env_socket_multicast_leave_source(env_socket_t sock, const __sym* group, const __sym* source, const __sym* local)
{
	struct ip_mreq_source imr;
	memset(&imr, 0, sizeof(imr));
	inet_pton(AF_INET, source, &imr.imr_sourceaddr);
	inet_pton(AF_INET, group, &imr.imr_multiaddr);
	inet_pton(AF_INET, local, &imr.imr_interface);
	return setsockopt(sock, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (char *)&imr, sizeof(imr));
}