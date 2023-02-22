#ifndef __ENV_DISK_H__
#define __ENV_DISK_H__


#include "env/env.h"

#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>


__fp env_fopen(const char* path, const char* mode)
{
	return fopen(path, mode);
}

bool env_fclose(__fp fp)
{
	return fclose((FILE*)fp) == 0 ? true : false;
}

int64_t env_ftell(__fp fp)
{
	return ftello((FILE*)fp);
}

int64_t env_fflush(__fp fp)
{
	return fflush((FILE*)fp);
}

int64_t env_fwrite(__fp fp, __ptr data, uint64_t size)
{
	return fwrite(data, 1, size, (FILE*)fp);
}

int64_t env_fread(__fp fp, __ptr buf, uint64_t size)
{
	return fread(buf, 1, size, (FILE*)fp);
}

int64_t env_fseek(__fp fp, int64_t offset, int32_t whence)
{
	return fseeko((FILE*)fp, offset, whence);
}


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

bool env_find_file(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFREG)) ? true : false;
}

/// get file size in bytes
/// return file size
uint64_t env_file_size(const char* filename)
{
	struct stat st;
	if (0 == stat(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
}

bool env_find_path(const char* path)
{
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? true : false;
}

static bool env_mkdir(const char* path)
{
	int r = mkdir(path, 0777);
	return 0 == r ? true : false;
}

bool env_remove_path(const char* path)
{
	int r = rmdir(path);
	return 0 == r ? true : false;
}

bool env_realpath(const char* path, char resolved_path[PATH_MAX])
{
	char* p = realpath(path, resolved_path);
	return p ? true : false;
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
bool env_remove_file(const char* path)
{
	int r = remove(path);
	return 0 == r ? true : false;
}

/// change the name or location of a file
/// 0-ok, other-error
bool env_move_path(const char* from, const char* to)
{
	int r = rename(from, to);
	return 0 == r ? true : false;
}

bool env_make_path(const char* path)
{
    if (path == NULL || path[0] == '\0'){
        return false;
    }

    bool ret = true;
    uint64_t len = strlen(path);

    if (!env_find_path(path)){
        char buf[PATH_MAX] = {0};
        snprintf(buf, PATH_MAX, "%s", path);
        if(buf[len - 1] == '/'){
            buf[len - 1] = '\0';
        }
        for(char *p = buf + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                if (!env_find_path(buf)){
                    ret = env_mkdir(buf);
                    if (!ret){
                        break;
                    }
                }
                *p = '/';
            }
        }
        if (ret){
            ret = env_mkdir(buf);
        }
    }

    return ret;
}




#endif //__ENV_DISK_H__