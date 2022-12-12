#ifndef __ENV_DISK_H__
#define __ENV_DISK_H__


#include "env/platforms.h"

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


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

static inline __bool env_file_exists(const char *path)
{
#if defined(OS_WINDOWS)
	// we must use GetFileAttributes() instead of the ANSI C functions because
	// it can cope with network (UNC) paths unlike them
    DWORD ret = GetFileAttributesA(path);
	return ((ret != INVALID_FILE_ATTRIBUTES) && !(ret & FILE_ATTRIBUTE_DIRECTORY)) ? __true : __false;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFREG)) ? __true : __false;
#endif
}

/// get file size in bytes
/// return file size
static inline int64_t env_file_size(const char* filename)
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

static inline __bool env_path_exists(const char *path)
{
#if defined(OS_WINDOWS)
	DWORD ret = GetFileAttributesA(path);
	return ((ret != INVALID_FILE_ATTRIBUTES) && (ret & FILE_ATTRIBUTE_DIRECTORY)) ? __true : __false;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? __true : __false;
#endif
}

static inline int env_mkdir(const char* path)
{
#if defined(OS_WINDOWS)
	BOOL r = CreateDirectoryA(path, NULL);
	return r ? 0 : (int)GetLastError();
#else
	int r = mkdir(path, 0777);
	return 0 == r ? 0 : errno;
#endif
}

static inline int env_rmdir(const char* path)
{
#if defined(OS_WINDOWS)
	BOOL r = RemoveDirectoryA(path);
	return r ? 0 : (int)GetLastError();
#else
	int r = rmdir(path);
	return 0 == r ? 0 : errno;
#endif
}

static inline int env_realpath(const char* path, char resolved_path[PATH_MAX])
{
#if defined(OS_WINDOWS)
	DWORD r = GetFullPathNameA(path, PATH_MAX, resolved_path, NULL);
	return r > 0 ? 0 : (int)GetLastError();
#else
	char* p = realpath(path, resolved_path);
	return p ? 0 : errno;
#endif
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
static inline int env_remove(const char* file)
{
#if defined(OS_WINDOWS)
	BOOL r = DeleteFileA(file);
	return r ? 0 : (int)GetLastError();
#else
	int r = remove(file);
	return 0 == r ? 0 : errno;
#endif
}

/// change the name or location of a file
/// 0-ok, other-error
static inline int env_rename(const char* oldpath, const char* newpath)
{
#if defined(OS_WINDOWS)
	BOOL r = MoveFileA(oldpath, newpath);
	return r ? 0:(int)GetLastError();
#else
	int r = rename(oldpath, newpath);
	return 0 == r? 0 : errno;
#endif
}

static inline __int32 env_mkpath(const char *path)
{
    if (path == NULL || path[0] == '\0'){
        return -1;
    }

    __int32 ret = 0;
    __uint64 len = strlen(path);

    if (!env_path_exists(path)){
        char buf[PATH_MAX] = {0};
        snprintf(buf, PATH_MAX, "%s", path);
        if(buf[len - 1] == '/'){
            buf[len - 1] = '\0';
        }
        for(char *p = buf + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                if (!env_path_exists(buf)){
                    ret = env_mkdir(buf);
                    if (ret != 0){
                        break;
                    }
                }
                *p = '/';
            }
        }
        if (ret == 0){
            ret = env_mkdir(buf);
        }
    }

    return ret;
}




#endif //__ENV_DISK_H__