/**
 * Kinesis Video Producer Host Info
 */

//antenna
#define LOG_CLASS "Network"
#include "socket.h"
#include "env/env.h"

//#include <ifaddrs.h>
//#include <sys/socket.h>
//#include <net/if.h>
//#include <netinet/in.h>
//#include <netinet/tcp.h>
//#include <netdb.h>

#if !defined __WINDOWS_BUILD__

#include <signal.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#else //!defined __WINDOWS_BUILD__

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#endif //!defined __WINDOWS_BUILD__

#define MEMCMP        memcmp
#define MEMCPY        memcpy
#define MEMSET        memset
#define MEMMOVE       memmove

#define SNPRINTF   snprintf

#ifndef SIZEOF
#define SIZEOF(x) (sizeof(x))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof *(array))
#endif

#define EMPTY_STRING ((__byte*) "")

#define STATUS_BASE                                     0x00000000
#define STATUS_NULL_ARG                                 STATUS_BASE + 0x00000001
#define STATUS_INVALID_ARG                              STATUS_BASE + 0x00000002
#define STATUS_BUFFER_TOO_SMALL                         STATUS_BASE + 0x00000005


#define STATUS_NETWORKING_BASE                     STATUS_BASE + 0x01000000
#define STATUS_GET_LOCAL_IP_ADDRESSES_FAILED       STATUS_NETWORKING_BASE + 0x00000016
#define STATUS_CREATE_UDP_SOCKET_FAILED            STATUS_NETWORKING_BASE + 0x00000017
#define STATUS_BINDING_SOCKET_FAILED               STATUS_NETWORKING_BASE + 0x00000018
#define STATUS_GET_PORT_NUMBER_FAILED              STATUS_NETWORKING_BASE + 0x00000019
#define STATUS_RESOLVE_HOSTNAME_FAILED             STATUS_NETWORKING_BASE + 0x0000001b
#define STATUS_HOSTNAME_NOT_FOUND                  STATUS_NETWORKING_BASE + 0x0000001c
#define STATUS_SOCKET_CONNECT_FAILED               STATUS_NETWORKING_BASE + 0x0000001d
#define STATUS_SOCKET_SET_SEND_BUFFER_SIZE_FAILED  STATUS_NETWORKING_BASE + 0x00000023
#define STATUS_GET_SOCKET_FLAG_FAILED              STATUS_NETWORKING_BASE + 0x00000024
#define STATUS_SET_SOCKET_FLAG_FAILED              STATUS_NETWORKING_BASE + 0x00000025
#define STATUS_CLOSE_SOCKET_FAILED                 STATUS_NETWORKING_BASE + 0x00000026

#ifndef ENTERS
#define ENTERS() LOGD("Network", "Enter")
#endif
#ifndef LEAVES
#define LEAVES() LOGD("Network", "Leave")
#endif

#define CHK_ERR(condition, errRet, errorMessage, ...)                                                                                                \
    do {                                                                                                                                             \
        if (!(condition)) {                                                                                                                          \
            retStatus = (errRet);                                                                                                                    \
            LOGE("Network", ##__VA_ARGS__);                                                                                                      \
            goto Reset;                                                                                                                            \
        }                                                                                                                                            \
    } while (__false)

#define CHK_STATUS(condition)                                                                                                                        \
    do {                                                                                                                                             \
        __res __status = condition;                                                                                                                 \
        if (__failed(__status)) {                                                                                                               \
            retStatus = (__status);                                                                                                                  \
            goto Reset;                                                                                                                            \
        }                                                                                                                                            \
    } while (__false)

__res getLocalhostIpAddresses(PKvsIpAddress destIpList, __uint32* pDestIpListLen, IceSetInterfaceFilterFunc filter, __uint64 customData)
{
    ENTERS();
    __res retStatus = 0;
    __uint32 ipCount = 0, destIpListLen;
    __bool filterSet = __true;

#ifdef _WIN32
    DWORD retWinStatus, sizeAAPointer;
    PIP_ADAPTER_ADDRESSES adapterAddresses, aa = NULL;
    PIP_ADAPTER_UNICAST_ADDRESS ua;
#else
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
#endif
    struct sockaddr_in* pIpv4Addr = NULL;
    struct sockaddr_in6* pIpv6Addr = NULL;

    __check(destIpList != NULL && pDestIpListLen != NULL, STATUS_NULL_ARG);
    __check(*pDestIpListLen != 0, STATUS_INVALID_ARG);

    destIpListLen = *pDestIpListLen;
#ifdef _WIN32
    retWinStatus = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &sizeAAPointer);
    __check(retWinStatus == ERROR_BUFFER_OVERFLOW, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);

    adapterAddresses = (PIP_ADAPTER_ADDRESSES) MEMALLOC(sizeAAPointer);

    retWinStatus = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapterAddresses, &sizeAAPointer);
    __check(retWinStatus == ERROR_SUCCESS, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);

    for (aa = adapterAddresses; aa != NULL && ipCount < destIpListLen; aa = aa->Next) {
        char ifa_name[BUFSIZ];
        memset(ifa_name, 0, BUFSIZ);
        WideCharToMultiByte(CP_ACP, 0, aa->FriendlyName, wcslen(aa->FriendlyName), ifa_name, BUFSIZ, NULL, NULL);

        for (ua = aa->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            if (filter != NULL) {
                DLOGI("Callback set to allow network interface filtering");
                // The callback evaluates to a FALSE if the application is interested in black listing an interface
                if (filter(customData, ifa_name) == FALSE) {
                    filterSet = FALSE;
                } else {
                    filterSet = __true;
                }
            }

            // If filter is set, ensure the details are collected for the interface
            if (filterSet == __true) {
                int family = ua->Address.lpSockaddr->sa_family;

                if (family == AF_INET) {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV4;
                    destIpList[ipCount].port = 0;

                    pIpv4Addr = (struct sockaddr_in*) (ua->Address.lpSockaddr);
                    MEMCPY(destIpList[ipCount].address, &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
                } else {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV6;
                    destIpList[ipCount].port = 0;

                    pIpv6Addr = (struct sockaddr_in6*) (ua->Address.lpSockaddr);
                    // Ignore unspecified addres: the other peer can't use this address
                    // Ignore link local: not very useful and will add work unnecessarily
                    // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                    if (IN6_IS_ADDR_UNSPECIFIED(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) ||
                        IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
                        continue;
                    }
                    MEMCPY(destIpList[ipCount].address, &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                }

                // in case of overfilling destIpList
                ipCount++;
            }
        }
    }
#else
    __check(getifaddrs(&ifaddr) != -1, STATUS_GET_LOCAL_IP_ADDRESSES_FAILED);
    for (ifa = ifaddr; ifa != NULL && ipCount < destIpListLen; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr != NULL && (ifa->ifa_flags & IFF_LOOPBACK) == 0 && // ignore loopback interface
            (ifa->ifa_flags & IFF_RUNNING) > 0 &&                            // interface has to be allocated
            (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
            // mark vpn interface
            destIpList[ipCount].isPointToPoint = ((ifa->ifa_flags & IFF_POINTOPOINT) != 0);

            if (filter != NULL) {
                // The callback evaluates to a FALSE if the application is interested in black listing an interface
                if (filter(customData, ifa->ifa_name) == __false) {
                    filterSet = __false;
                } else {
                    filterSet = __true;
                }
            }

            // If filter is set, ensure the details are collected for the interface
            if (filterSet == __true) {
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV4;
                    destIpList[ipCount].port = 0;
                    pIpv4Addr = (struct sockaddr_in*) ifa->ifa_addr;
                    MEMCPY(destIpList[ipCount].address, &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);

                } else {
                    destIpList[ipCount].family = KVS_IP_FAMILY_TYPE_IPV6;
                    destIpList[ipCount].port = 0;
                    pIpv6Addr = (struct sockaddr_in6*) ifa->ifa_addr;
                    // Ignore unspecified addres: the other peer can't use this address
                    // Ignore link local: not very useful and will add work unnecessarily
                    // Ignore site local: https://tools.ietf.org/html/rfc8445#section-5.1.1.1
                    if (IN6_IS_ADDR_UNSPECIFIED(&pIpv6Addr->sin6_addr) || IN6_IS_ADDR_LINKLOCAL(&pIpv6Addr->sin6_addr) ||
                        IN6_IS_ADDR_SITELOCAL(&pIpv6Addr->sin6_addr)) {
                        continue;
                    }
                    MEMCPY(destIpList[ipCount].address, &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                }

                // in case of overfilling destIpList
                ipCount++;
            }
        }
    }
#endif

Reset:

#ifdef _WIN32
    if (adapterAddresses != NULL) {
        SAFE_MEMFREE(adapterAddresses);
    }
#else
    if (ifaddr != NULL) {
        freeifaddrs(ifaddr);
    }
#endif

    if (pDestIpListLen != NULL) {
        *pDestIpListLen = ipCount;
    }

    LEAVES();
    return retStatus;
}

__res createSocket(KVS_IP_FAMILY_TYPE familyType, KVS_SOCKET_PROTOCOL protocol, __uint32 sendBufSize, __int32* pOutSockFd)
{
    __res retStatus = 0;

    __int32 sockfd, sockType, flags;
    __int32 optionValue;

    __check(pOutSockFd != NULL, STATUS_NULL_ARG);

    sockType = protocol == KVS_SOCKET_PROTOCOL_UDP ? SOCK_DGRAM : SOCK_STREAM;

    sockfd = socket(familyType == KVS_IP_FAMILY_TYPE_IPV4 ? AF_INET : AF_INET6, sockType, 0);
    if (sockfd == -1) {
        LOGW("Network", "socket() failed to create socket with errno %s", getErrorString(getErrorCode()));
        __check(__false, STATUS_CREATE_UDP_SOCKET_FAILED);
    }

    optionValue = 1;
    if (setsockopt(sockfd, SOL_SOCKET, NO_SIGNAL, &optionValue, SIZEOF(optionValue)) < 0) {
        LOGW("Network", "setsockopt() NO_SIGNAL failed with errno %s", getErrorString(getErrorCode()));
    }

    if (sendBufSize > 0 && setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendBufSize, SIZEOF(sendBufSize)) < 0) {
        LOGW("Network", "setsockopt() SO_SNDBUF failed with errno %s", getErrorString(getErrorCode()));
        __check(__false, STATUS_SOCKET_SET_SEND_BUFFER_SIZE_FAILED);
    }

    *pOutSockFd = (__int32) sockfd;

#ifdef _WIN32
    UINT32 nonblock = 1;
    ioctlsocket(sockfd, FIONBIO, &nonblock);
#else
    // Set the non-blocking mode for the socket
    flags = fcntl(sockfd, F_GETFL, 0);
    CHK_ERR(flags >= 0, STATUS_GET_SOCKET_FLAG_FAILED, "Failed to get the socket flags with system error %s", strerror(errno));
    CHK_ERR(0 <= fcntl(sockfd, F_SETFL, flags | O_NONBLOCK), STATUS_SET_SOCKET_FLAG_FAILED, "Failed to Set the socket flags with system error %s",
            strerror(errno));
#endif

    // done at this point for UDP
    __check(protocol == KVS_SOCKET_PROTOCOL_TCP, retStatus);

    /* disable Nagle algorithm to not delay sending packets. We should have enough density to justify using it. */
    optionValue = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optionValue, SIZEOF(optionValue)) < 0) {
        LOGW("Network", "setsockopt() TCP_NODELAY failed with errno %s", getErrorString(getErrorCode()));
    }

Reset:

    return retStatus;
}

__res closeSocket(__int32 sockfd)
{
    __res retStatus = 0;

#ifdef _WIN32
    CHK_ERR(closesocket(sockfd) == 0, STATUS_CLOSE_SOCKET_FAILED, "Failed to close the socket %s", getErrorString(getErrorCode()));
#else
    CHK_ERR(close(sockfd) == 0, STATUS_CLOSE_SOCKET_FAILED, "Failed to close the socket %s", strerror(errno));
#endif

Reset:

    return retStatus;
}

__res socketBind(PKvsIpAddress pHostIpAddress, __int32 sockfd)
{
    __res retStatus = 0;
    struct sockaddr_in ipv4Addr;
    struct sockaddr_in6 ipv6Addr;
    struct sockaddr* sockAddr = NULL;
    socklen_t addrLen;

    __sym ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    __check(pHostIpAddress != NULL, STATUS_NULL_ARG);

    if (pHostIpAddress->family == KVS_IP_FAMILY_TYPE_IPV4) {
        MEMSET(&ipv4Addr, 0x00, SIZEOF(ipv4Addr));
        ipv4Addr.sin_family = AF_INET;
        ipv4Addr.sin_port = 0; // use next available port
        MEMCPY(&ipv4Addr.sin_addr, pHostIpAddress->address, IPV4_ADDRESS_LENGTH);
        // TODO: Properly handle the non-portable sin_len field if needed per https://issues.amazon.com/KinesisVideo-4952
        // ipv4Addr.sin_len = SIZEOF(ipv4Addr);
        sockAddr = (struct sockaddr*) &ipv4Addr;
        addrLen = SIZEOF(struct sockaddr_in);

    } else {
        MEMSET(&ipv6Addr, 0x00, SIZEOF(ipv6Addr));
        ipv6Addr.sin6_family = AF_INET6;
        ipv6Addr.sin6_port = 0; // use next available port
        MEMCPY(&ipv6Addr.sin6_addr, pHostIpAddress->address, IPV6_ADDRESS_LENGTH);
        // TODO: Properly handle the non-portable sin6_len field if needed per https://issues.amazon.com/KinesisVideo-4952
        // ipv6Addr.sin6_len = SIZEOF(ipv6Addr);
        sockAddr = (struct sockaddr*) &ipv6Addr;
        addrLen = SIZEOF(struct sockaddr_in6);
    }

    if (bind(sockfd, sockAddr, addrLen) < 0) {
        CHK_STATUS(getIpAddrStr(pHostIpAddress, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
//        LOGW("Network", "bind() failed for ip%s address: %s, port %u with errno %s", IS_IPV4_ADDR(pHostIpAddress) ? EMPTY_STRING : "V6", ipAddrStr,
//              (UINT16) getInt16(pHostIpAddress->port), getErrorString(getErrorCode()));
        __check(__false, STATUS_BINDING_SOCKET_FAILED);
    }

    if (getsockname(sockfd, sockAddr, &addrLen) < 0) {
        LOGW("Network", "getsockname() failed with errno %s", getErrorString(getErrorCode()));
        __check(__false, STATUS_GET_PORT_NUMBER_FAILED);
    }

    pHostIpAddress->port = (__uint16) pHostIpAddress->family == KVS_IP_FAMILY_TYPE_IPV4 ? ipv4Addr.sin_port : ipv6Addr.sin6_port;

Reset:
    return retStatus;
}

__res socketConnect(PKvsIpAddress pPeerAddress, __int32 sockfd)
{
    __res retStatus = 0;
    struct sockaddr_in ipv4PeerAddr;
    struct sockaddr_in6 ipv6PeerAddr;
    struct sockaddr* peerSockAddr = NULL;
    socklen_t addrLen;
    __int32 retVal;

    __check(pPeerAddress != NULL, STATUS_NULL_ARG);

    if (pPeerAddress->family == KVS_IP_FAMILY_TYPE_IPV4) {
        MEMSET(&ipv4PeerAddr, 0x00, SIZEOF(ipv4PeerAddr));
        ipv4PeerAddr.sin_family = AF_INET;
        ipv4PeerAddr.sin_port = pPeerAddress->port;
        MEMCPY(&ipv4PeerAddr.sin_addr, pPeerAddress->address, IPV4_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr*) &ipv4PeerAddr;
        addrLen = SIZEOF(struct sockaddr_in);
    } else {
        MEMSET(&ipv6PeerAddr, 0x00, SIZEOF(ipv6PeerAddr));
        ipv6PeerAddr.sin6_family = AF_INET6;
        ipv6PeerAddr.sin6_port = pPeerAddress->port;
        MEMCPY(&ipv6PeerAddr.sin6_addr, pPeerAddress->address, IPV6_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr*) &ipv6PeerAddr;
        addrLen = SIZEOF(struct sockaddr_in6);
    }

    retVal = connect(sockfd, peerSockAddr, addrLen);
    CHK_ERR(retVal >= 0 || getErrorCode() == KVS_SOCKET_IN_PROGRESS, STATUS_SOCKET_CONNECT_FAILED, "connect() failed with errno %s",
            getErrorString(getErrorCode()));

Reset:
    return retStatus;
}

__res getIpWithHostName(__sym* hostname, PKvsIpAddress destIp)
{
    __res retStatus = 0;
    __int32 errCode;
    __sym* errStr;
    struct addrinfo *res, *rp;
    __bool resolved = __false;
    struct sockaddr_in* ipv4Addr;
    struct sockaddr_in6* ipv6Addr;

    __check(hostname != NULL, STATUS_NULL_ARG);

    errCode = getaddrinfo(hostname, NULL, NULL, &res);
    if (errCode != 0) {
        errStr = errCode == EAI_SYSTEM ? strerror(errno) : (__sym*) gai_strerror(errCode);
        CHK_ERR(__false, STATUS_RESOLVE_HOSTNAME_FAILED, "getaddrinfo() with errno %s", errStr);
    }

    for (rp = res; rp != NULL && !resolved; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            ipv4Addr = (struct sockaddr_in*) rp->ai_addr;
            destIp->family = KVS_IP_FAMILY_TYPE_IPV4;
            MEMCPY(destIp->address, &ipv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
            resolved = __true;
        } else if (rp->ai_family == AF_INET6) {
            ipv6Addr = (struct sockaddr_in6*) rp->ai_addr;
            destIp->family = KVS_IP_FAMILY_TYPE_IPV6;
            MEMCPY(destIp->address, &ipv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
            resolved = __true;
        }
    }

    freeaddrinfo(res);
    CHK_ERR(resolved, STATUS_HOSTNAME_NOT_FOUND, "could not find network address of %s", hostname);

Reset:

//    CHK_LOG_ERR(retStatus);

    return retStatus;
}

__res getIpAddrStr(PKvsIpAddress pKvsIpAddress, __sym* pBuffer, __uint32 bufferLen)
{
    __res retStatus = 0;
    __uint32 generatedStrLen = 0; // number of characters written if buffer is large enough not counting the null terminator

    __check(pKvsIpAddress != NULL, STATUS_NULL_ARG);
    __check(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);

    if (IS_IPV4_ADDR(pKvsIpAddress)) {
        generatedStrLen = SNPRINTF(pBuffer, bufferLen, "%u.%u.%u.%u", pKvsIpAddress->address[0], pKvsIpAddress->address[1], pKvsIpAddress->address[2],
                                   pKvsIpAddress->address[3]);
    } else {
        generatedStrLen = SNPRINTF(pBuffer, bufferLen, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                                   pKvsIpAddress->address[0], pKvsIpAddress->address[1], pKvsIpAddress->address[2], pKvsIpAddress->address[3],
                                   pKvsIpAddress->address[4], pKvsIpAddress->address[5], pKvsIpAddress->address[6], pKvsIpAddress->address[7],
                                   pKvsIpAddress->address[8], pKvsIpAddress->address[9], pKvsIpAddress->address[10], pKvsIpAddress->address[11],
                                   pKvsIpAddress->address[12], pKvsIpAddress->address[13], pKvsIpAddress->address[14], pKvsIpAddress->address[15]);
    }

    // bufferLen should be strictly larger than generatedStrLen because bufferLen includes null terminator
    __check(generatedStrLen < bufferLen, STATUS_BUFFER_TOO_SMALL);

Reset:

    return retStatus;
}

__bool isSameIpAddress(PKvsIpAddress pAddr1, PKvsIpAddress pAddr2, __bool checkPort)
{
    __bool ret;
    __uint32 addrLen;

    if (pAddr1 == NULL || pAddr2 == NULL) {
        return __false;
    }

    addrLen = IS_IPV4_ADDR(pAddr1) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;

    ret =
        (pAddr1->family == pAddr2->family && MEMCMP(pAddr1->address, pAddr2->address, addrLen) == 0 && (!checkPort || pAddr1->port == pAddr2->port));

    return ret;
}

#ifdef _WIN32
INT32 getErrorCode(VOID)
{
    INT32 error = WSAGetLastError();
    switch (error) {
        case WSAEWOULDBLOCK:
            error = EWOULDBLOCK;
            break;
        case WSAEINPROGRESS:
            error = EINPROGRESS;
            break;
        case WSAEISCONN:
            error = EISCONN;
            break;
        case WSAEINTR:
            error = EINTR;
            break;
        default:
            /* leave unchanged */
            break;
    }
    return error;
}
#else
__int32 getErrorCode(__void)
{
    return errno;
}
#endif

#ifdef _WIN32
__sym* getErrorString(INT32 error)
{
    static __sym buffer[1024];
    switch (error) {
        case EWOULDBLOCK:
            error = WSAEWOULDBLOCK;
            break;
        case EINPROGRESS:
            error = WSAEINPROGRESS;
            break;
        case EISCONN:
            error = WSAEISCONN;
            break;
        case EINTR:
            error = WSAEINTR;
            break;
        default:
            /* leave unchanged */
            break;
    }
    if (FormatMessage((FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS), NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer,
                      SIZEOF(buffer), NULL) == 0) {
        SNPRINTF(buffer, SIZEOF(buffer), "error code %d", error);
    }

    return buffer;
}
#else
__sym* getErrorString(__int32 error)
{
    return strerror(error);
}
#endif
