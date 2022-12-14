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

#define __pass(condition) \
    do { \
        if (!(condition)) { \
            printf("Check condition failed: %s, %s\n", #condition, env_check()); \
            goto Reset; \
        } \
    } while (__false)

void storage_test()
{
    __sint64 ret;
    __fp fp;
    char buf[1024] = {0};

    if (env_find_file(TEST_MOVE_FILE)){
        printf("del file %s\n", TEST_MOVE_FILE);
        __pass(env_remove_file(TEST_MOVE_FILE) == __true);
    }

    if (env_find_path(TEST_PATH)){
        printf("rm dir %s\n", TEST_PATH);        
        __pass(env_remove_path(TEST_PATH) == __true);
    }

    printf("mkpath %s\n", TEST_PATH);
    __pass(env_make_path(TEST_PATH) == __true);

    printf("open file %s\n", TEST_FILE);
    fp = env_fopen(TEST_FILE, "a+t");
    __pass(fp != NULL);

    printf("write file %s\n", TEST_FILE);
    __pass(env_fwrite(fp, TEST_WRITE_TEXT, strlen(TEST_WRITE_TEXT)) >= 0);

    env_fflush(fp);

    printf("file length %lld\n", env_ftell(fp));

    env_fclose(fp);

    printf("move file %s to %s\n", TEST_FILE, TEST_MOVE_FILE);
    env_move_path(TEST_FILE, TEST_MOVE_FILE);

    fp = env_fopen(TEST_MOVE_FILE, "r+");

    ret = env_fread(fp, buf, 256);

    printf("ret = %llu %s\n", ret, buf);

    __pass(env_fclose(fp) == __true);

Reset:
    return;
}