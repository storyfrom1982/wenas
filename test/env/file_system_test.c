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
        printf("del file %s\n", TEST_MOVE_FILE);
        env_unlink(TEST_MOVE_FILE);
    }

    if (env_fs_dir_exists(TEST_PATH)){
        printf("rm dir %s\n", TEST_PATH);
        env_rmdir(TEST_PATH);
    }

    printf("mkpath %s\n", TEST_PATH);
    env_fs_mkpath(TEST_PATH);

    printf("open file %s\n", TEST_FILE);
    fp = env_fopen(TEST_FILE, "a+t");

    env_fwrite(fp, TEST_WRITE_TEXT, strlen(TEST_WRITE_TEXT));

    env_fflush(fp);

    printf("file length %lld\n", env_ftell(fp));

    env_fclose(fp);

    printf("move file %s to %s\n", TEST_FILE, TEST_MOVE_FILE);
    env_rename(TEST_FILE, TEST_MOVE_FILE);

    fp = env_fopen(TEST_MOVE_FILE, "r+");

    ret = env_fread(fp, buf, 256);

    printf("ret = %llu %s\n", ret, buf);

    env_fclose(fp);

    env_dir_t *dir = env_opendir(TEST_PATH);

    env_fs_entry_t *entry = env_readdir(dir);
    entry = env_readdir(dir);
    entry = env_readdir(dir);

    printf("get fs path %s\n", env_fs_entry_name(entry));

    ret = snprintf(buf, 1024, "%s/%s", TEST_PATH, entry->d_name);
    buf[ret] = '\0';

    env_fs_stat_t info;
    ret = env_stat(buf, &info);
    printf("get stat ret = %d\n", ret);

    printf("is dir %u\n", env_fs_stat_is_dir(&info));
    printf("is file %u\n", env_fs_stat_is_file(&info));
    printf("get size %lld\n", env_fs_stat_file_size(&info));
    printf("get uid %lld\n", env_fs_stat_uid(&info));
    printf("get gid %lld\n", env_fs_stat_gid(&info));
    printf("get last access %lld\n", env_fs_stat_last_access(&info));
    printf("get last modification %lld\n", env_fs_stat_last_modification(&info));
}