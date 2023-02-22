#include "env/env.h"

#include <stdio.h>
#include <string.h>

#include <time.h>
#include <errno.h>


inline const char* env_check(void)
{
	return strerror(errno);
}

inline const char* env_parser(int32_t error)
{
	return strerror(error);
}

/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
inline uint64_t env_time(void)
{
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (__uint64)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

///@return nanoseconds(relative time)
inline uint64_t env_clock(void)
{
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (uint64_t)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
}

inline uint64_t env_strftime(char *buf, uint64_t size, uint64_t seconds)
{
	time_t sec = (time_t)seconds;
    return strftime(buf, size, "%Y-%m-%d %H:%M:%S", localtime(&sec));
}