#ifndef __UNIX_FILE_SYSTEM_H__
#define __UNIX_FILE_SYSTEM_H__


#include "unix.h"

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>


#define ENV_PATH_MAX    PATH_MAX


typedef int env_fd_t;
typedef DIR env_dir_t;
typedef FILE env_file_t;
typedef struct dirent env_fs_entry_t;
typedef struct stat env_fs_stat_t;



static inline int env_stat(const char *path, env_fs_stat_t *info)
{
    return lstat(path, info);
}

static inline env_fs_entry_t* env_readdir(env_dir_t *dir)
{
    return readdir(dir);
}

static inline env_dir_t* env_opendir(const char *path)
{
    return opendir(path);
}

static inline int env_closedir(env_dir_t *dir)
{
    return closedir(dir);
}

static inline int env_mkdir(const char *path)
{
    int ret = mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static inline int env_rmdir(const char *path)
{
    return rmdir(path);
}

static inline int env_rename_path(const char *from_path, const char *to_path)
{
    return rename(from_path, to_path);
}

static inline int env_remove(const char *path)
{
    return remove(path);
}

static inline int env_unlink(const char *path)
{
    return unlink(path);
}

static inline env_file_t* env_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

static inline int env_fclose(env_file_t *fp)
{
    return fclose(fp);
}

static inline size_t env_fread(env_file_t *fp, void *buf, size_t size)
{
    return fread(buf, 1, size, fp);
}

static inline size_t env_fwrite(env_file_t *fp, void *data, size_t size)
{
    return fwrite(data, 1, size, fp);
}

static inline int env_fseek(env_file_t *fp, int64_t offset, int whence)
{
    return fseeko(fp, offset, whence);
}

static inline int64_t env_ftell(env_file_t *fp)
{
    return ftello(fp);
}

static inline int env_fflush(env_file_t *fp)
{
    return fflush(fp);
}

static inline int env_open(const char *path, int flags, int mask)
{
    return open(path, flags, mask);
}

static inline int env_close(int fd)
{
    return close(fd);
}

static inline ssize_t env_read(int fd, void *buf, size_t size)
{
    return read(fd, buf, size);
}

static inline ssize_t env_write(int fd, void *data, size_t size)
{
    return write(fd, data, size);
}

static inline int64_t env_lseek(int fd, int64_t offset, int64_t whence)
{
    return lseek(fd, offset, whence);
}


#endif