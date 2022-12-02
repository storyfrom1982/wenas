#include "sys/struct/lineardb_pipe.h"
#include <stdio.h>

static void test_pipe_write_read()
{
    linedb_pipe_t *lp = linedb_pipe_build(128);
    linedb_t *ldb;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(buf, 1024, "hello world %d %d", i, rand());
        buf[n] = '\0';
        fprintf(stdout, "write block: =========== %s\n", buf);
        linedb_pipe_write(lp, buf, n+1);
        ldb = linedb_pipe_hold_block(lp);
        fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __dataof_linedb(ldb));
        linedb_pipe_free_block(lp, ldb);
	}
    linedb_pipe_destroy(&lp);
}

static void test_pipe_producer_consumer()
{
    linedb_pipe_t *lp = linedb_pipe_build(1U << 6);
    linedb_t *ldb, *ldb1 = NULL;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
        while (1){
            n = rand();
            n = snprintf(buf, 1024, "hello world %d %d %.3f", i, n, (double)n / 3.3f);
            buf[n] = '\0';
            n++;
            // fprintf(stdout, "write block: =========== %s\n", buf);
            if (linedb_pipe_write(lp, buf, n) != n){
                break;
            }
        }

        while ((ldb = linedb_pipe_hold_block(lp)) != NULL)
        {
            ldb1 = ldb;
            fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __dataof_linedb(ldb));
            linedb_pipe_free_block(lp, ldb);
        }
	}

    linedb_pipe_destroy(&lp);
    fprintf(stdout, "exit\n");
}


void lineardb_pipe_test()
{
    test_pipe_write_read();
    test_pipe_producer_consumer();
}