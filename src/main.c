#include <stdio.h>

// extern int test_lineardb();
// extern int test_linearkv();
extern void lineardb_test();

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");
#ifdef __LITTLE_ENDIAN__
	fprintf(stdout, "__LITTLE_ENDIAN__\n");
#else
	fprintf(stdout, "__BIG_ENDIAN__\n");
#endif

	// test_lineardb();
	// test_linearkv();
	lineardb_test();

	return 0;
}
