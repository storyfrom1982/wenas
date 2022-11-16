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
    lineardbPipe_release(&lp);
}

static void test_pipe_producer_consumer()
{
    LineardbPipe *lp = lineardbPipe_create(1U << 6);
    Lineardb *ldb, *ldb1 = NULL;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
        while (1){
            n = rand();
            n = snprintf(buf, 1024, "hello world %d %d %.3f", i, n, (double)n / 3.3f);
            buf[n] = '\0';
            n++;
            // fprintf(stdout, "write block: =========== %s\n", buf);
            if (lineardbPipe_write(lp, buf, n) != n){
                break;
            }
        }

        while ((ldb = lineardbPipe_hold_block(lp)) != NULL)
        {
            ldb1 = ldb;
            fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __dataof_block(ldb));
            lineardbPipe_free_block(lp, ldb);
        }
	}

    lineardbPipe_release(&lp);
    fprintf(stdout, "exit\n");
}


void linearPipe_test()
{
    test_pipe_write_read();
    test_pipe_producer_consumer();
}