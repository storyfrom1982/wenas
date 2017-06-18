/*
 * sr_common.c
 *
 *  Created on: 2017年6月18日
 *      Author: kly
 */


#include "sr_common.h"


#include <time.h>
#include <sys/time.h>

#include "sr_log.h"


int64_t sr_starting_time()
{
	struct timeval tv = {0};
	if (gettimeofday(&tv, NULL) != 0){
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}
	return 1000000LL * tv.tv_sec + tv.tv_usec;
}


int64_t sr_calculate_time(int64_t start_microsecond)
{
	struct timeval tv = {0};
	if (gettimeofday(&tv, NULL) != 0){
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}
	return (1000000LL * tv.tv_sec + tv.tv_usec) - start_microsecond;
}
