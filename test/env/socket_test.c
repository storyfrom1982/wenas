#include <env/env.h>


#define ENV_SOCKET_ADDRLEN 1024
void socket_test()
{
    __uint16 port = 37213;
    __fp fp = env_fopen("/tmp/ip.txt", "r+t");
    __pass(fp != NULL);
    __sym host[ENV_SOCKET_ADDRLEN] = {0};
    __sint64 ret = env_fread(fp, host, ENV_SOCKET_ADDRLEN);
    env_fclose(fp);
    __pass(ret > 0); 
    host[ret] = '\0';
    __logd("ret = %ld %s\n", ret, host);

    __socket sock = env_socket_open(host, port);
    __pass(sock > 0);

    __pass(env_socket_set_nonblock(sock, 1) == 0);

    __ipaddr ip;
    __sockaddr_ptr raddr = env_socket_addr_create(host, port);
    __sockaddr_ptr laddr = env_socket_addr_create(NULL, port);

    // __pass(env_socket_bind(sock, laddr) == 0); 
    // __pass(env_socket_connect(sock) == 0); 

    __sym buf[ENV_SOCKET_ADDRLEN] = {0};
    while (1){
        __pass((ret = env_socket_sendto(sock, "123\n", 4, raddr)) > 0); 
        __logd("env_socket_sendto ret: %d\n", ret);

        ret = env_socket_recvfrom(sock, buf, ENV_SOCKET_ADDRLEN, laddr);
        __logd("env_socket_recvfrom ret: %d %s\n", ret, buf);
        
        env_socket_addr_get_ip(laddr, &ip);
        __logd("env_socket_recvfrom ip: %u.%u.%u.%u %d\n", 
        ((unsigned char*)&ip.ip)[0], ((unsigned char*)&ip.ip)[1], ((unsigned char*)&ip.ip)[2], ((unsigned char*)&ip.ip)[3], ip.port);
        env_thread_sleep(1000000000);
    }

Reset:
    env_socket_close(sock);
    return;
}    