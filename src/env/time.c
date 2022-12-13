#include "env/env.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <time.h>
#include <errno.h>
#endif


inline const __sym* env_status(void)
{
#if defined(OS_WINDOWS)
	return strerror((int)GetLastError());
#else
	return strerror(errno);
#endif
}

/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
__uint64 env_time(void)
{
#if defined(OS_WINDOWS)
	FILETIME ticks;
	GetSystemTimeAsFileTime(&ticks);
    return ((((__uint64) ticks.dwHighDateTime << 32) | ticks.dwLowDateTime) - 116444736000000000ULL) * 100ULL;
#else
#if defined(CLOCK_REALTIME)
	struct timespec tp;
	clock_gettime(CLOCK_REALTIME, &tp);
	return (__uint64)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (__uint64)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
#endif
}

///@return nanoseconds(relative time)
__uint64 env_clock(void)
{
#if defined(OS_WINDOWS)
	LARGE_INTEGER freq;
	LARGE_INTEGER count;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&count);
	return ((__uint64)(count.QuadPart / freq.QuadPart) * NANO_SECONDS) 
            + ((count.QuadPart % freq.QuadPart) * (NANO_SECONDS / freq.QuadPart));
#else            
#if defined(CLOCK_MONOTONIC)
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (__uint64)tp.tv_sec * NANO_SECONDS + tp.tv_nsec;
#else
	// POSIX.1-2008 marks gettimeofday() as obsolete, recommending the use of clock_gettime(2) instead.
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (__uint64)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif
#endif
}