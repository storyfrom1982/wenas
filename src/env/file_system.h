#ifndef __ENV_FILE_SYSTEM_H__
#define __ENV_FILE_SYSTEM_H__


#include <env/unix/file_system.h>


static inline const char* env_fs_entry_name(env_fs_entry_t *entry)
{
    return entry->d_name;
}

//static inline int64_t env_fs_stat_last_modification(env_fs_stat_t *info)
//{
//    return info->st_mtimespec.tv_sec * 1000000000UL + info->st_mtimespec.tv_nsec;
//}
//
//static inline int64_t env_fs_stat_last_access(env_fs_stat_t *info)
//{
//    return info->st_atimespec.tv_sec * 1000000000UL + info->st_atimespec.tv_nsec;
//}

static inline int64_t env_fs_stat_gid(env_fs_stat_t *info)
{
    return info->st_gid;
}

static inline int64_t env_fs_stat_uid(env_fs_stat_t *info)
{
    return info->st_uid;
}

static inline int64_t env_fs_stat_file_size(env_fs_stat_t *info)
{
    return info->st_size;
}

static inline uint8_t env_fs_stat_is_link(env_fs_stat_t *info)
{
    return S_ISLNK(info->st_mode) ? 1 : 0;
}

static inline uint8_t env_fs_stat_is_dir(env_fs_stat_t *info)
{
    return S_ISDIR(info->st_mode) ? 1 : 0;
}

static inline uint8_t env_fs_stat_is_file(env_fs_stat_t *info)
{
    return S_ISREG(info->st_mode) ? 1 : 0;
}

static inline int env_fs_dir_exists(const char *path)
{
    struct stat info;
    if (lstat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
        return 1;
    }
    return 0;
}

static inline int env_fs_file_exists(const char *path)
{
    struct stat info;
    if (lstat(path, &info) == 0 && S_ISREG(info.st_mode)) {
        return 1;
    }
    return 0;
}

static inline int64_t env_fs_ltell(int fd)
{
    return lseek(fd, 0, SEEK_END);
}

static inline int env_fs_mkpath(const char *path)
{
    int ret = 0;
    size_t len;
    if (path == NULL || (len = strlen(path) <= 0)){
        return -1;
    }

    if (!env_fs_dir_exists(path)){
        char tmp[ENV_PATH_MAX] = {0};
        char *p = NULL;
        snprintf(tmp, ENV_PATH_MAX, "%s", path);
        if(tmp[len - 1] == '/'){
            tmp[len - 1] = '\0';
        }
        for(p = tmp + 1; *p; p++){
            if(*p == '/') {
                *p = '\0';
                ret = env_mkdir(tmp);
                if (ret != 0){
                    break;
                }
                *p = '/';
            }
        }
        if (ret == 0){
            ret = env_mkdir(tmp);
        }
    }

    return ret;
}




#endif