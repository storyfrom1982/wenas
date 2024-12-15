#ifndef __ENV_DISK_H__
#define __ENV_DISK_H__


#include "ex/ex.h"

#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>


__ex_fp __ex_fopen(const char* path, const char* mode)
{
	return fopen(path, mode);
}

bool __ex_fclose(__ex_fp fp)
{
	return fclose((FILE*)fp) == 0 ? true : false;
}

int64_t __ex_ftell(__ex_fp fp)
{
	return ftello((FILE*)fp);
}

int64_t __ex_fflush(__ex_fp fp)
{
	return fflush((FILE*)fp);
}

int64_t __ex_fwrite(__ex_fp fp, void *data, uint64_t size)
{
	return fwrite(data, 1, size, (FILE*)fp);
}

int64_t __ex_fread(__ex_fp fp, void *buf, uint64_t size)
{
	return fread(buf, 1, size, (FILE*)fp);
}

int64_t __ex_fseek(__ex_fp fp, int64_t offset, int32_t whence)
{
	return fseeko((FILE*)fp, offset, whence);
}


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

bool __ex_check_file(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFREG)) ? true : false;
}

/// get file size in bytes
/// return file size
uint64_t __fs_size(const char* filename)
{
	struct stat st;
	if (0 == stat(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
}

bool __ex_check_path(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? true : false;
}

static bool __ex_mkdir(const char* path)
{
	int r = mkdir(path, 0777);
	return 0 == r ? true : false;
}

bool __ex_delete_path(const char* path)
{
	int r = rmdir(path);
	return 0 == r ? true : false;
}

bool __ex_realpath(const char* path, char resolved_path[PATH_MAX])
{
	char* p = realpath(path, resolved_path);
	return p ? true : false;
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
bool __ex_delete_file(const char* path)
{
	int r = remove(path);
	return 0 == r ? true : false;
}

/// change the name or location of a file
/// 0-ok, other-error
bool __ex_move_path(const char* from, const char* to)
{
	int r = rename(from, to);
	return 0 == r ? true : false;
}

bool __ex_make_path(const char* path)
{
    if (path == NULL || path[0] == '\0'){
        return false;
    }

    bool ret = true;
    uint64_t len = strlen(path);

    if (!__ex_check_path(path)){
        char buf[PATH_MAX] = {0};
        snprintf(buf, PATH_MAX, "%s", path);
        if(buf[len - 1] == '/'){
            buf[len - 1] = '\0';
        }
        for(char *p = buf + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                if (!__ex_check_path(buf)){
                    ret = __ex_mkdir(buf);
                    if (!ret){
                        break;
                    }
                }
                *p = '/';
            }
        }
        if (ret){
            ret = __ex_mkdir(buf);
        }
    }

    return ret;
}




#endif //__ENV_DISK_H__