#include <stdio.h>

extern void linearkv_test();
extern void lineardb_test();
extern void linearPipe_test();

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");
#ifdef __LITTLE_ENDIAN__
	fprintf(stdout, "__LITTLE_ENDIAN__\n");
#else
	fprintf(stdout, "__BIG_ENDIAN__\n");
#endif

	// lineardb_test();
	// linearkv_test();
	linearPipe_test();

	return 0;
}
