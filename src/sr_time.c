/*
 * sr_time.c
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of sr_malloc.
 *
 * self-reliance is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * self-reliance is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "sr_time.h"

#include <time.h>
#include <sys/time.h>

#include "sr_log.h"
#include "sr_error.h"


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
