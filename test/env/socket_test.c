#include <env/env.h>



void socket_test()
{
    __uint16 port;
    env_sockaddr_ptr addr;
    env_socklen_t addrlen;
    __fp fp = env_fopen("/tmp/ip.txt", "r+t");
    __pass(fp != NULL);
    __sym ip[ENV_SOCKET_ADDRLEN] = {0};
    __sint64 ret = env_fread(fp, ip, ENV_SOCKET_ADDRLEN);
    env_fclose(fp);
    __pass(ret > 0);
    ip[ret] = '\0';
    __logd("ret = %ld %s\n", ret, ip);

    env_socket_t sock = env_socket_udp();
    __pass(sock > 0);

    __pass(env_socket_ip2addr(&addr, ip, 3721) == 0);
    __pass(env_socket_addr2ip(addr, ip, &port) == 0);
    __logd("addr to ip = %s:%u\n", ip, port);

    addrlen = env_socket_addr_len(addr);
    __pass(ret > 0);

    ret = env_socket_sendto(sock, "123\n", 4, 0, addr, addrlen);
    __logd("env_socket_sendto ret: %d\n", ret);

    // ret = env_socket_bind(sock, addr, addrlen);
    // __pass(ret == 0);

Reset:
    return;
}