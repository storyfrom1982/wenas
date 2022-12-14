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


__fp env_fopen(const __symptr path, const __symptr mode)
{
	return fopen(path, mode);
}

__bool_ env_fclose(__fp fp)
{
	return fclose((FILE*)fp) == 0 ? __true : __false;
}

__sint64 env_ftell(__fp fp)
{
#if defined(OS_WINDOWS)
	return _ftelli64((FILE*)fp);
#else
	return ftello((FILE*)fp);
#endif
}

__sint64 env_fflush(__fp fp)
{
	return fflush((FILE*)fp);
}

__sint64 env_fwrite(__fp fp, __ptr data, __uint64 size)
{
	return fwrite(data, 1, size, (FILE*)fp);
}

__sint64 env_fread(__fp fp, __ptr buf, __uint64 size)
{
	return fread(buf, 1, size, (FILE*)fp);
}

__sint64 env_fseek(__fp fp, __sint64 offset, __sint32 whence)
{
#if defined(OS_WINDOWS)
	return _fseeki64((FILE*)fp, offset, whence);
#else
	return fseeko((FILE*)fp, offset, whence);
#endif	
}


/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

__bool_ env_find_file(const __symptr path)
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
__uint64 env_file_size(const __symptr filename)
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

__bool_ env_find_path(const __symptr path)
{
#if defined(OS_WINDOWS)
	DWORD ret = GetFileAttributesA(path);
	return ((ret != INVALID_FILE_ATTRIBUTES) && (ret & FILE_ATTRIBUTE_DIRECTORY)) ? __true : __false;
#else
	struct stat info;
	return (stat(path, &info)==0 && (info.st_mode&S_IFDIR)) ? __true : __false;
#endif
}

static __bool_ env_mkdir(const __symptr path)
{
#if defined(OS_WINDOWS)
	BOOL r = CreateDirectoryA(path, NULL);
	return r ? __true : __false;
#else
	int r = mkdir(path, 0777);
	return 0 == r ? __true : __false;
#endif
}

__bool_ env_remove_path(const __symptr path)
{
#if defined(OS_WINDOWS)
	BOOL r = RemoveDirectoryA(path);
	return r ? __true : __false;
#else
	int r = rmdir(path);
	return 0 == r ? __true : __false;
#endif
}

__bool_ env_realpath(const __symptr path, __sym resolved_path[PATH_MAX])
{
#if defined(OS_WINDOWS)
	DWORD r = GetFullPathNameA(path, PATH_MAX, resolved_path, NULL);
	return r > 0 ? __true : __false;
#else
	char* p = realpath(path, resolved_path);
	return p ? __true : __false;
#endif
}

/// delete a name and possibly the file it refers to
/// 0-ok, other-error
__bool_ env_remove_file(const __symptr path)
{
#if defined(OS_WINDOWS)
	BOOL r = DeleteFileA(path);
	return r ? __true : __false;
#else
	int r = remove(path);
	return 0 == r ? __true : __false;
#endif
}

/// change the name or location of a file
/// 0-ok, other-error
__bool_ env_move_path(const __symptr from, const __symptr to)
{
#if defined(OS_WINDOWS)
	BOOL r = MoveFileA(from, to);
	return r ? __true : __false;
#else
	int r = rename(from, to);
	return 0 == r ? __true : __false;
#endif
}

__bool_ env_make_path(const __symptr path)
{
    if (path == NULL || path[0] == '\0'){
        return __false;
    }

    __bool_ ret = __true;
    __uint64 len = strlen(path);

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