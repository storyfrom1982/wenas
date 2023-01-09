#include <env/env.h>
#include <stdio.h>
#include <string.h>

#if defined(OS_WINDOWS)
#define TEST_PATH  "C:/tmp/test/dir" 
#define TEST_FILE  "C:/tmp/test/dir/test.txt" 
#define TEST_MOVE_FILE  "C:/tmp/test/dir/test1.txt"
#define TEST_WRITE_TEXT  "C:/tmp/test/dir/test1.txt\n"
#else
#define TEST_PATH  "/tmp/test/dir" 
#define TEST_FILE  "/tmp/test/dir/test.txt" 
#define TEST_MOVE_FILE  "/tmp/test/dir/test1.txt"
#define TEST_WRITE_TEXT  "/tmp/test/dir/test1.txt\n"
#endif

void storage_test()
{
    int64_t ret;
    __fp fp;
    char buf[1024] = {0};

    if (env_find_file(TEST_MOVE_FILE)){
        __logd("del file %s\n", TEST_MOVE_FILE);
        __pass(env_remove_file(TEST_MOVE_FILE) == true);
    }

    if (env_find_path(TEST_PATH)){
        __logd("rm dir %s\n", TEST_PATH);        
        __pass(env_remove_path(TEST_PATH) == true);
    }

    __logd("mkpath %s\n", TEST_PATH);
    __pass(env_make_path(TEST_PATH) == true);

    __logd("open file %s\n", TEST_FILE);
    fp = env_fopen(TEST_FILE, "a+t");
    __pass(fp != NULL);

    __logd("write file %s\n", TEST_FILE);
    __pass(env_fwrite(fp, TEST_WRITE_TEXT, strlen(TEST_WRITE_TEXT)) >= 0);

    env_fflush(fp);

    __logd("file length %lld\n", env_ftell(fp));

    env_fclose(fp);

    __logd("move file %s to %s\n", TEST_FILE, TEST_MOVE_FILE);
    env_move_path(TEST_FILE, TEST_MOVE_FILE);

    fp = env_fopen(TEST_MOVE_FILE, "r+");

    ret = env_fread(fp, buf, 256);

    __logd("ret = %llu %s\n", ret, buf);

    __pass(env_fclose(fp) == true);

Reset:
    return;
}