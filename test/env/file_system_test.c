#include <env/env.h>
#include <env/file_system.h>

#define TEST_PATH  "/tmp/test/dir" 
#define TEST_FILE  "/tmp/test/dir/test.txt" 
#define TEST_MOVE_FILE  "/tmp/test/dir/test1.txt"
#define TEST_WRITE_TEXT  "/tmp/test/dir/test1.txt\n"

void file_system_test()
{
    size_t ret;
    env_file_t *fp;
    char buf[1024] = {0};

    if (env_fs_file_exists(TEST_MOVE_FILE)){
        LOGD("FILESYS", "del file %s\n", TEST_MOVE_FILE);
        env_unlink(TEST_MOVE_FILE);
    }

    if (env_fs_dir_exists(TEST_PATH)){
        LOGD("FILESYS", "rm dir %s\n", TEST_PATH);
        env_rmdir(TEST_PATH);
    }

    LOGD("FILESYS", "mkpath %s\n", TEST_PATH);
    env_fs_mkpath(TEST_PATH);

    LOGD("FILESYS", "open file %s\n", TEST_FILE);
    fp = env_fopen(TEST_FILE, "a+t");

    env_fwrite(fp, TEST_WRITE_TEXT, strlen(TEST_WRITE_TEXT));

    env_fflush(fp);

    LOGD("FILESYS", "file length %lld\n", env_ftell(fp));

    env_fclose(fp);

    LOGD("FILESYS", "move file %s to %s\n", TEST_FILE, TEST_MOVE_FILE);
    env_rename(TEST_FILE, TEST_MOVE_FILE);

    fp = env_fopen(TEST_MOVE_FILE, "r+");

    ret = env_fread(fp, buf, 256);

    LOGD("FILESYS", "ret = %llu %s\n", ret, buf);

    env_fclose(fp);

    env_dir_t *dir = env_opendir(TEST_PATH);

    env_fs_entry_t *entry = env_readdir(dir);
    entry = env_readdir(dir);
    entry = env_readdir(dir);

    LOGD("FILESYS", "get fs path %s\n", env_fs_entry_name(entry));

    ret = snprintf(buf, 1024, "%s/%s", TEST_PATH, entry->d_name);
    buf[ret] = '\0';

    env_fs_stat_t info;
    ret = env_stat(buf, &info);
    LOGD("FILESYS", "get stat ret = %d\n", ret);

    LOGD("FILESYS", "is dir %u\n", env_fs_stat_is_dir(&info));
    LOGD("FILESYS", "is file %u\n", env_fs_stat_is_file(&info));
    LOGD("FILESYS", "get size %lld\n", env_fs_stat_file_size(&info));
    LOGD("FILESYS", "get uid %lld\n", env_fs_stat_uid(&info));
    LOGD("FILESYS", "get gid %lld\n", env_fs_stat_gid(&info));
//    LOGD("FILESYS", "get last access %lld\n", env_fs_stat_last_access(&info));
//    LOGD("FILESYS", "get last modification %lld\n", env_fs_stat_last_modification(&info));
}