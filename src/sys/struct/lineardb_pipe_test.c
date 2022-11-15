#include "lineardb_pipe.h"


static void test_pipe_write_read()
{
    LineardbPipe *lp = lineardbPipe_create(128);
    Lineardb *ldb;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(buf, 1024, "hello world %d %d", i, rand());
        buf[n] = '\0';
        fprintf(stdout, "write block: =========== %s\n", buf);
        lineardbPipe_write(lp, buf, n+1);
        ldb = lineardbPipe_hold_block(lp);
        fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __dataof_block(ldb));
        lineardbPipe_free_block(lp, ldb);
	}
}

static void test_pipe_write_read1()
{
    LineardbPipe *lp = lineardbPipe_create(128);
    Lineardb *ldb, *ldb1 = NULL;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(buf, 1024, "hello world %d %d", i, rand());
        buf[n] = '\0';
        n++;
        // fprintf(stdout, "write block: =========== %s\n", buf);
        while (1){
            if (lineardbPipe_write(lp, buf, n) != n){
                // fprintf(stdout, "ldb: =========== %u %p\n", lp->read_block->byte[0], &lp->read_block->byte[0]);
                break;
            }
        }

        // fprintf(stdout, "ldb :: =========== %u %p\n", lp->read_block->byte[0], &lp->read_block->byte[0]);

        while ((ldb = lineardbPipe_hold_block(lp)) != NULL)
        {
            ldb1 = ldb;
            fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __dataof_block(ldb));
            lineardbPipe_free_block(lp, ldb);
        }
	}
}


void linearPipe_test()
{
    test_pipe_write_read();
    test_pipe_write_read1();
}