/*******************************************
HostInfo internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__

#pragma once

#include <stdint.h>
#include <stdbool.h>

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

typedef char CHAR;
typedef short WCHAR;
typedef uint8_t UINT8;
typedef int8_t INT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef int32_t INT32;
typedef uint64_t UINT64;
typedef int64_t INT64;
typedef double DOUBLE;
typedef long double LDOUBLE;
typedef float FLOAT;
typedef bool BOOL;
#ifndef VOID
#define VOID void
#endif

typedef UINT8 BYTE;
typedef VOID* PVOID;
typedef BYTE* PBYTE;
typedef BOOL* PBOOL;
typedef CHAR* PCHAR;
typedef WCHAR* PWCHAR;
typedef INT8* PINT8;
typedef UINT8* PUINT8;
typedef INT16* PINT16;
typedef UINT16* PUINT16;
typedef INT32* PINT32;
typedef UINT32* PUINT32;
typedef INT64* PINT64;
typedef UINT64* PUINT64;
typedef long LONG, *PLONG;
typedef unsigned long ULONG, *PULONG;
typedef DOUBLE* PDOUBLE;
typedef LDOUBLE* PLDOUBLE;
typedef FLOAT* PFLOAT;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#define STATUS UINT32

#define STATUS_SUCCESS ((STATUS) 0x00000000)

#define STATUS_FAILED(x)    (((STATUS)(x)) != STATUS_SUCCESS)
#define STATUS_SUCCEEDED(x) (!STATUS_FAILED(x))

#define IPV6_ADDRESS_LENGTH (UINT16) 16
#define IPV4_ADDRESS_LENGTH (UINT16) 4

#define CHK(condition, errRet)                                                                                                                       \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            goto CleanUp;                                                                                                                            \
        }                                                                                                                                            \
    } while (FALSE)


typedef enum {
    KVS_IP_FAMILY_TYPE_IPV4 = (UINT16) 0x0001,
    KVS_IP_FAMILY_TYPE_IPV6 = (UINT16) 0x0002,
} KVS_IP_FAMILY_TYPE;

typedef struct {
    UINT16 family;
    UINT16 port;                       // port is stored in network byte order
    BYTE address[IPV6_ADDRESS_LENGTH]; // address is stored in network byte order
    BOOL isPointToPoint;
} KvsIpAddress, *PKvsIpAddress;

#define IS_IPV4_ADDR(pAddress) ((pAddress)->family == KVS_IP_FAMILY_TYPE_IPV4)

typedef enum {
    KVS_SOCKET_PROTOCOL_NONE,
    KVS_SOCKET_PROTOCOL_TCP,
    KVS_SOCKET_PROTOCOL_UDP,
} KVS_SOCKET_PROTOCOL;

typedef BOOL (*IceSetInterfaceFilterFunc)(UINT64, PCHAR);

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
STATUS getLocalhostIpAddresses(PKvsIpAddress, PUINT32, IceSetInterfaceFilterFunc, UINT64);

/**
 * @param - KVS_IP_FAMILY_TYPE - IN - Family for the socket. Must be one of KVS_IP_FAMILY_TYPE
 * @param - KVS_SOCKET_PROTOCOL - IN - either tcp or udp
 * @param - UINT32 - IN - send buffer size in bytes
 * @param - PINT32 - OUT - PINT32 for the socketfd
 *
 * @return - STATUS status of execution
 */
STATUS createSocket(KVS_IP_FAMILY_TYPE, KVS_SOCKET_PROTOCOL, UINT32, PINT32);

/**
 * @param - INT32 - IN - INT32 for the socketfd
 *
 * @return - STATUS status of execution
 */
STATUS closeSocket(INT32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to bind. PKvsIpAddress->port will be changed to the actual port number
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
STATUS socketBind(PKvsIpAddress, INT32);

/**
 * @param - PKvsIpAddress - IN - address for the socket to connect.
 * @param - INT32 - IN - valid socket fd
 *
 * @return - STATUS status of execution
 */
STATUS socketConnect(PKvsIpAddress, INT32);

/**
 * @param - PCHAR - IN - hostname to resolve
 *
 * @param - PKvsIpAddress - OUT - resolved ip address
 *
 * @return - STATUS status of execution
 */
STATUS getIpWithHostName(PCHAR, PKvsIpAddress);

STATUS getIpAddrStr(PKvsIpAddress, PCHAR, UINT32);

BOOL isSameIpAddress(PKvsIpAddress, PKvsIpAddress, BOOL);

/**
 * @return - INT32 error code
 */
INT32 getErrorCode(VOID);

/**
 * @param - INT32 - IN - error code
 *
 * @return - PCHAR string associated with error code
 */
PCHAR getErrorString(INT32);

#ifdef _WIN32
#define POLL WSAPoll
#else
#define POLL poll
#endif

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_NETWORK__ */
