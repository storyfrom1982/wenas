#include <env/env.h>


#define ENV_SOCKET_ADDRLEN 1024
void socket_test()
{
    __uint16 port = 37211;
    __fp fp = env_fopen("/tmp/ip.txt", "r+t");
    __pass(fp != NULL);
    __sym ip[ENV_SOCKET_ADDRLEN] = {0};
    __sint64 ret = env_fread(fp, ip, ENV_SOCKET_ADDRLEN);
    env_fclose(fp);
    __pass(ret > 0); 
    ip[ret] = '\0';
    __logd("ret = %ld %s\n", ret, ip);

    __socket_ptr sock = env_socket_creatre(ip, port);
    __pass(sock != NULL);

    __pass(env_socket_bind(sock) == 0); 
    // __pass(env_socket_connect(sock) == 0); 

    while (1){
        __pass((ret = env_socket_sendto(sock, "123\n", 4)) > 0); 
        __logd("env_socket_sendto ret: %d\n", ret);
        env_thread_sleep(1000000000);
    }

Reset:
    env_socket_destroy(&sock);
    return;
}    