#ifndef __ENV_DISK_H__
#define __ENV_DISK_H__


#include "env/env.h"

#if defined(OS_WINDOWS)
#include <Windows.h>
#include <direct.h>
#define PATH_MAX _MAX_PATH
#else
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#endif

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
#if defined(OS_WINDOWS)
	return _ftelli64((FILE*)fp);
#else
	return ftello((FILE*)fp);
#endif
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
#if defined(OS_WINDOWS)
	return _fseeki64((FILE*)fp, offset, whence);
#else
	return fseeko((FILE*)fp, offset, whence);
#endif	
}


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

bool env_find_file(const char* path)
{
#if defined(OS_WINDOWS)
	// we must use GetFileAttributes() instead of the ANSI C functions because
	// it can cope with network (UNC) paths unlike them
    DWORD ret = GetFileAttributesA(path);
	return ((ret != INVALID_FILE_ATTRIBUTES) && !(ret & FILE_ATTRIBUTE_DIRECTORY)) ? true : false;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFREG)) ? true : false;
#endif
}

/// get file size in bytes
/// return file size
uint64_t env_file_size(const char* filename)
{
#if defined(OS_WINDOWS)
	struct _stat64 st;
	if (0 == _stat64(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
#else
	struct stat st;
	if (0 == stat(filename, &st) && (st.st_mode & S_IFREG))
		return st.st_size;
	return -1;
#endif
}

bool env_find_path(const char* path)
{
#if defined(OS_WINDOWS)
	DWORD ret = GetFileAttributesA(path);
	return ((ret != INVALID_FILE_ATTRIBUTES) && (ret & FILE_ATTRIBUTE_DIRECTORY)) ? true : false;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? true : false;
#endif
}

static bool env_mkdir(const char* path)
{
#if defined(OS_WINDOWS)
	bool r = CreateDirectoryA(path, NULL);
	return r ? true : false;
#else
	int r = mkdir(path, 0777);
	return 0 == r ? true : false;
#endif
}

bool env_remove_path(const char* path)
{
#if defined(OS_WINDOWS)
	bool r = RemoveDirectoryA(path);
	return r ? true : false;
#else
	int r = rmdir(path);
	return 0 == r ? true : false;
#endif
}

bool env_realpath(const char* path, char resolved_path[PATH_MAX])
{
#if defined(OS_WINDOWS)
	DWORD r = GetFullPathNameA(path, PATH_MAX, resolved_path, NULL);
	return r > 0 ? true : false;
#else
	char* p = realpath(path, resolved_path);
	return p ? true : false;
#endif
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
bool env_remove_file(const char* path)
{
#if defined(OS_WINDOWS)
	bool r = DeleteFileA(path);
	return r ? true : false;
#else
	int r = remove(path);
	return 0 == r ? true : false;
#endif
}

/// change the name or location of a file
/// 0-ok, other-error
bool env_move_path(const char* from, const char* to)
{
#if defined(OS_WINDOWS)
	bool r = MoveFileA(from, to);
	return r ? true : false;
#else
	int r = rename(from, to);
	return 0 == r ? true : false;
#endif
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