#include <stdio.h>
#include "linear_data_block.h"

extern int test_lineardb();

int main(int argc, char *argv[]) 
{
	fprintf(stdout, "hello world\n");

#ifdef __LITTLE_ENDIAN__
	fprintf(stdout, "__LITTLE_ENDIAN__\n");
#endif

	test_lineardb();

	return 0;
}
