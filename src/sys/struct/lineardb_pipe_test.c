#include "lineardb_pipe.h"


static void test_pipe_write_read()
{
    LineardbPipe *lp = lineardbPipe_create(256);
    Lineardb *ldb;
	char buf[1024] = {0};
	int32_t n;
	for (int i = 0; i < 100; ++i){
		n = snprintf(buf, 1024, "hello world %d %d", i, rand());
        buf[n] = '\0';
        // fprintf(stdout, "write block: =========== %s\n", buf);
        lineardbPipe_write(lp, buf, n+1);
        ldb = lineardbPipe_hold_block(lp);
        fprintf(stdout, "hold block: >>>>> %s <<<<<\n", __byteof_block(ldb));
        lineardbPipe_free_block(lp, ldb);
	}
}


void linearPipe_test()
{
    test_pipe_write_read();
}