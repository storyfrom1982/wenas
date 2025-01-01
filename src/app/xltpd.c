#include "xltp.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/xltpd/log", NULL);

    xltp_t *peer = xltp_create(0);
    __xcheck(peer == NULL);

    while (1){
        sleep(1);
    }

    xltp_free(&peer);

    xlog_recorder_close();

    return 0;
XClean:
    return -1;
}