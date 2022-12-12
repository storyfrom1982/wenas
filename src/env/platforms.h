#ifndef __ENV_PLATFORMS_H__
#define __ENV_PLATFORMS_H__

#ifdef _WIN64
#   define OS_WINDOWS
#endif

#ifdef __APPLE__
#   define OS_APPLE
#endif

#ifdef __ANDROID__
#   define OS_ANDROID
#endif

#define MILLI_SECONDS    1000ULL
#define MICRO_SECONDS    1000000ULL
#define NANO_SECONDS     1000000000ULL

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef char __sym;
typedef uint8_t __uint8;
typedef uint16_t __uint16;
typedef uint32_t __uint32;
typedef uint64_t __uint64;
typedef float __real32;
typedef double __real64;
typedef void __void;
typedef void* __ptr;

#define __false         0
#define __true          1

#if !defined(OS_WINDOWS)
#include <stdbool.h>
typedef bool __bool;
typedef int8_t __int8;
typedef int16_t __int16;
typedef int32_t __int32;
typedef int64_t __int64;
#endif


#if defined(OS_WINDOWS)
#include <Windows.h>
#else
#include <time.h>
#include <errno.h>
#endif


static inline const __sym* env_status(void)
{
#if defined(OS_WINDOWS)
	return strerror((int)GetLastError());
#else
	return strerror(errno);
#endif
}

/*** The following code is referencing: https://github.com/ireader/sdk.git ***/

/// nanoseconds since the Epoch(1970-01-01 00:00:00 +0000 (UTC))
static inline __uint64 env_time(void)
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
static inline __uint64 env_clock(void)
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


#endif //__ENV_PLATFORMS_H__