#include "xpeer.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    xlog_recorder_open("./tmp/xltpd/log", NULL);

    xpeer_t *peer = xpeer_create();
    __xcheck(peer == NULL);

    while (1){
        sleep(1);
    }

    xpeer_free(&peer);

    xlog_recorder_close();

    return 0;
XClean:
    return -1;
}