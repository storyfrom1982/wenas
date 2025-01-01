#include "app/xltp.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    const char *hostname = "xltp.net";
    uint16_t port = 9256;
    char ip[46] = {0};

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xltp_t *peer = xltp_create(1);
    __xcheck(peer == NULL);


    // const char *cip = "192.168.1.7";
    // const char *cip = "120.78.155.213";
    // const char *cip = "47.92.77.19";
    const char *cip = "2408:4005:303:c200:6377:e67f:7eaf:72be";
    // const char *cip = "47.99.146.226";
    // const char *cip = hostname;
    // const char *cip = "2409:8914:865d:877:5115:1502:14dc:4882";
    // const char *cip = "2409:8a14:8743:9750:350f:784f:8966:8b52";
    // const char *cip = "2409:8a14:8743:9750:7193:6fc2:f49d:3cdb";
    // const char *cip = "2409:8914:8669:1bf8:5c20:3ccc:1d88:ce38";

    mcopy(ip, cip, slength(cip));
    ip[slength(cip)] = '\0';

    char str[1024];
    char input[256];
    char command[256];

    while (1)
    {
        printf("> "); // 命令提示符
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // 读取失败，退出循环
        }

        // 去掉输入字符串末尾的换行符
        input[strcspn(input, "\n")] = 0;

        // 分割命令和参数
        sscanf(input, "%s", command);

        if (strcmp(command, "echo") == 0) {
            xltp_echo(peer, ip, port);

        } else if (strcmp(command, "boot") == 0) {
            // xpeer_bootstrap(peer);

        } else if (strcmp(command, "exit") == 0) {
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        mclear(command, 256);
    }

    xltp_free(&peer);

    xlog_recorder_close();

XClean:

    return 0;
}
