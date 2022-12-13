/*******************************************
HostInfo internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__

#pragma once

#include "env/env.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOCAL_NETWORK_INTERFACE_COUNT 128

// string buffer size for ipv4 and ipv6. Null terminator included.
// for ipv6: 0000:0000:0000:0000:0000:0000:0000:0000 = 39
// for ipv4 mapped ipv6: 0000:0000:0000:0000:0000:ffff:192.168.100.228 = 45
#define KVS_IP_ADDRESS_STRING_BUFFER_LEN 46

// 000.000.000.000
#define KVS_MAX_IPV4_ADDRESS_STRING_LEN 15

#define KVS_GET_IP_ADDRESS_PORT(a) ((UINT16) getInt16((a)->port))

#if defined(__MACH__)
#define NO_SIGNAL SO_NOSIGPIPE
#else
#define NO_SIGNAL MSG_NOSIGNAL
#endif

// Some systems such as Windows doesn't have this value
#ifndef EAI_SYSTEM
#define EAI_SYSTEM -11
#endif

// Windows uses EWOULDBLOCK (WSAEWOULDBLOCK) to indicate connection attempt
// cannot be completed immediately, whereas POSIX uses EINPROGRESS.
#ifdef _WIN32
#define KVS_SOCKET_IN_PROGRESS EWOULDBLOCK
#else
#define KVS_SOCKET_IN_PROGRESS EINPROGRESS
#endif

#define __res           __int32
#define __failed(x)     (((__res)(x)) != 0)

#define __check(condition, x) \
    do { \
        if ((condition)) { \
            LOGE("CHECK", "Describe: %s, %s\n", #condition, env_status_describe(env_status())); \
            goto Reset; \
        } \
    } while (__false)

#define IPV6_ADDRESS_LENGTH (__uint16) 16
#define IPV4_ADDRESS_LENGTH (__uint16) 4

typedef enum {
    KVS_IP_FAMILY_TYPE_IPV4 = (__uint16) 0x0001,
    KVS_IP_FAMILY_TYPE_IPV6 = (__uint16) 0x0002,
} KVS_IP_FAMILY_TYPE;

typedef struct {
    __uint16 family;
    __uint16 port;                       // port is stored in network byte order
    __sym address[IPV6_ADDRESS_LENGTH]; // address is stored in network byte order
    __bool isPointToPoint;
} KvsIpAddress, *PKvsIpAddress;

#define IS_IPV4_ADDR(pAddress) ((pAddress)->family == KVS_IP_FAMILY_TYPE_IPV4)

typedef enum {
    KVS_SOCKET_PROTOCOL_NONE,
    KVS_SOCKET_PROTOCOL_TCP,
    KVS_SOCKET_PROTOCOL_UDP,
} KVS_SOCKET_PROTOCOL;

typedef __bool (*IceSetInterfaceFilterFunc)(__uint64, __sym*);

/**
 * @param - PKvsIpAddress - IN/OUT - array for getLocalhostIpAddresses to store any local ips it found. The ip address and port
 *                                   will be in network byte order.
 * @param - UINT32 - IN/OUT - length of the array, upon return it will be updated to the actual number of ips in the array
 *
 *@param - IceSetInterfaceFilterFunc - IN - set to custom interface filter callback
 *
 *@param - UINT64 - IN - Set to custom data that can be used in the callback later
 * @return - STATUS status of execution
 */
__res getLocalhostIpAddresses(PKvsIpAddress, __uint32*, IceSetInterfaceFilterFunc, __uint64);

/**
 * @param - KVS_IP_FAMILY_TYPE - IN - Family for the socket. Must be one of KVS_IP_FAMILY_TYPE
 * @param - KVS_SOCKET_PROTOCOL - IN - either tcp or udp
 * @param - UINT32 - IN - send buffer size in bytes
 * @param - __int32* - OUT - __int32* for the socketfd
 *
 * @return - STATUS status of execution
 */
__res createSocket(KVS_IP_FAMILY_TYPE, KVS_SOCKET_PROTOCOL, __uint32, __sint32*);

/**
 * @param - INT32 - IN - INT32 for the socketfd
 *
 * @return - STATUS status of execution
 */
__res closeSocket(__sint32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to bind. PKvsIpAddress->port will be changed to the actual port number
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
__res socketBind(PKvsIpAddress, __sint32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to connect.
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
__res socketConnect(PKvsIpAddress, __sint32);

/**
 * @param - __byte* - IN - hostname to resolve
 *
 * @param - PKvsIpAddress - OUT - resolved ip address
 *
 * @return - STATUS status of execution
 */
__res getIpWithHostName(__sym*, PKvsIpAddress);

__res getIpAddrStr(PKvsIpAddress, __sym*, __uint32);

__bool isSameIpAddress(PKvsIpAddress, PKvsIpAddress, __bool);

/**
 * @return - INT32 error code
 */
__sint32 getErrorCode(__void);

/**
 * @param - INT32 - IN - error code
 *
 * @return - __byte* string associated with error code
 */
__sym* getErrorString(__sint32);

#ifdef _WIN32
#define POLL WSAPoll
#else
#define POLL poll
#endif

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__ */
