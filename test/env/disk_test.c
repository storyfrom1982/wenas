#include <env/env.h>


#if defined(OS_WINDOWS)
#define TEST_PATH  "C://tmp/test/dir" 
#define TEST_FILE  "C://tmp/test/dir/test.txt" 
#define TEST_MOVE_FILE  "C://tmp/test/dir/test1.txt"
#define TEST_WRITE_TEXT  "C://tmp/test/dir/test1.txt\n"
#else
#define TEST_PATH  "/tmp/test/dir" 
#define TEST_FILE  "/tmp/test/dir/test.txt" 
#define TEST_MOVE_FILE  "/tmp/test/dir/test1.txt"
#define TEST_WRITE_TEXT  "/tmp/test/dir/test1.txt\n"
#endif


void disk_test()
{
    size_t ret;
    FILE *fp;
    char buf[1024] = {0};

    if (env_file_exists(TEST_MOVE_FILE)){
        printf("del file %s\n", TEST_MOVE_FILE);
        __pass(env_remove(TEST_MOVE_FILE) == 0);
    }

    if (env_path_exists(TEST_PATH)){
        printf("rm dir %s\n", TEST_PATH);        
        __pass(env_rmdir(TEST_PATH) == 0);
    }

    printf("mkpath %s\n", TEST_PATH);
    __pass(env_mkpath(TEST_PATH) == 0);

    printf("open file %s\n", TEST_FILE);
    fp = fopen(TEST_FILE, "a+t");
    __pass(fp != NULL);

    printf("write file %s\n", TEST_FILE);
    __pass(fwrite(TEST_WRITE_TEXT, 1, strlen(TEST_WRITE_TEXT), fp) >= 0);

    fflush(fp);

    printf("file length %lld\n", ftell(fp));

    fclose(fp);

    printf("move file %s to %s\n", TEST_FILE, TEST_MOVE_FILE);
    env_rename(TEST_FILE, TEST_MOVE_FILE);

    fp = fopen(TEST_MOVE_FILE, "r+");

    ret = fread(buf, 1, 256, fp);

    printf("ret = %llu %s\n", ret, buf);

    fclose(fp);

    printf("get size %lld\n", env_file_size(TEST_MOVE_FILE));

Reset:
    return;
}