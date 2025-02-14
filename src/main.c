#include "xnet/xltp.h"
#include <stdio.h>
#include <string.h>
#include "xnet/xlio.h"


static int scandir_cb(const char *name, int type, uint64_t size, void **ctx)
{
    xframe_t **frame = (xframe_t**)ctx;
    uint64_t pos = xl_obj_begin(frame, NULL);
    xl_add_word(frame, "path", name);
    xl_add_int(frame, "type", type);
    xl_add_uint(frame, "size", size);
    xl_obj_end(frame, pos);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *hostname = "xltp.net";
    uint16_t port = 9256;
    char ip[46] = {0};

    size_t cwd_size = 1024;
    char cwd_path[cwd_size];
    __xapi->fs_path_cwd(cwd_path, &cwd_size);
    __xlogd("pwd======%s\n", cwd_path);

    xlog_recorder_open("./tmp/xpeer/log", NULL);

    xltp_t *peer = xltp_create(1);
    __xcheck(peer == NULL);

    // const char *cip = "192.168.1.4";
    // const char *cip = "120.78.155.213";
    // const char *cip = "47.92.77.19";
    // const char *cip = "2408:4005:303:c200:6377:e67f:7eaf:72be";
    const char *cip = "47.99.146.226";
    // const char *cip = hostname;
    // const char *cip = "2409:8914:865d:877:5115:1502:14dc:4882";
    // const char *cip = "2409:8a14:8743:9750:350f:784f:8966:8b52";
    // const char *cip = "2409:8a14:8745:8d90:8:4168:3641:fb61"; // 
    // const char *cip = "2409:8914:8669:1bf8:5c20:3ccc:1d88:ce38";

    xcopy(ip, cip, xlen(cip));
    ip[xlen(cip)] = '\0';

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

        } else if (strcmp(command, "put") == 0) {
            size_t cwd_size = 1024;
            char cwd_path[cwd_size];
            __xapi->fs_path_cwd(cwd_path, &cwd_size);
            __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
            xltp_put(peer, cwd_path, "/tmp/wenas", addr, NULL);
            // xltp_put(peer, "./build/xltpd", "wenas", ip, port);
            // xltp_put(peer, "xltpd", ip, port);

        } else if (strcmp(command, "get") == 0) {
            __xipaddr_ptr addr = __xapi->udp_host_to_addr(ip, port);
            xltp_get(peer, "/tmp/wenas", "/tmp/wenas/Kangzixin", addr, NULL);
            // xltp_put(peer, "xltpd", ip, port);

        } else if (strcmp(command, "scan") == 0) {

            // xline_t *dirlist = xl_maker();
            // // uint64_t pos = xl_list_begin(&dirlist, "list");
            // // int name_pos = 0;
            // // while (cwd_path[cwd_size - name_pos -1] != '/')
            // // {
            // //     name_pos++;
            // // }
            // // __xapi->fs_path_scanner(cwd_path, cwd_size - name_pos, scandir_cb, (void**)&dirlist);
            // // xl_list_end(&dirlist, pos);
            // // xl_printf(&dirlist->line);
            // xlio_path_scanner(cwd_path, &dirlist);
            // xline_t xllist = xl_parser(&dirlist->line);
            // xline_t *dlist = xl_find(&xllist, "list");
            // xllist = xl_parser(dlist);
            // xline_t *xd;
            // while ((xd = xl_list_next(&xllist)) != NULL){
            //     xl_printf(xd);
            // }
            // __xlogd("dir size = %lu\n", dirlist->range);
            // xl_free(&dirlist);

        } else if (strcmp(command, "dir") == 0) {

            size_t cwd_size = 1024;
            char cwd_path[cwd_size];
            __xapi->fs_path_cwd(cwd_path, &cwd_size);
            // int path_len = xlen(cwd_path);
            int dir_name_pos = 0;
            while (cwd_path[cwd_size - dir_name_pos - 1] != '/'){
                dir_name_pos++;
            }
            dir_name_pos = cwd_size - dir_name_pos;
            __xfs_scanner_ptr scanner = __xapi->fs_scanner_open(cwd_path);
            __xfs_item_ptr item;
            while ((item = __xapi->fs_scanner_read(scanner)) != NULL)
            {
                __xlogd("scanner --- type(%d) size:%lu path_len:%d %s\n", item->type, item->size, item->path_len, item->path + dir_name_pos);
            }
            __xapi->fs_scanner_close(scanner);
            

        } else if (strcmp(command, "exit") == 0) {
            __xlogi("再见！\n");
            break;
        } else {
            __xlogi("未知命令: %s\n", command);
        }

        xclear(command, 256);
    }

    xltp_free(&peer);

    xlog_recorder_close();

XClean:

    return 0;
}
