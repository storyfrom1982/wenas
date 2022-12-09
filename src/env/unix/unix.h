#ifndef __ENV_UNIX_H__
#define __ENV_UNIX_H__


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef char __sym;
typedef bool __bool;
typedef int8_t __int8;
typedef int16_t __int16;
typedef int32_t __int32;
typedef int64_t __int64;
typedef uint8_t __uint8;
typedef uint16_t __uint16;
typedef uint32_t __uint32;
typedef uint64_t __uint64;
typedef float __real32;
typedef double __real64;
typedef void __void;

#define __false         0
#define __true          1


#include <errno.h>
#include <string.h>

static inline __int32 env_status(__void)
{
    return errno;
}

static inline __sym* env_status_describe(__int32 status)
{
    return strerror(status);
}


#include <time.h>
#include <sys/time.h>


#undef MILLISEC
#define MILLISEC    1000UL

#undef MICROSEC
#define MICROSEC    1000000UL

#undef NANOSEC
#define NANOSEC     1000000000UL


static inline __uint64 env_time()
{
    struct timespec t;
    //https://man7.org/linux/man-pages/man2/clock_getres.2.html
    if (clock_gettime(CLOCK_REALTIME, &t) == 0){
        return t.tv_sec * NANOSEC + t.tv_nsec;
    }else {
        struct timeval tv;
        //https://man7.org/linux/man-pages/man2/settimeofday.2.html
        if (gettimeofday(&tv, NULL) == 0){
            return tv.tv_sec * NANOSEC + tv.tv_usec * 1000UL;
        }
    }
    //TODO
    return 0;
}

#define env_strftime strftime


#include <stdio.h>
#include <stdarg.h>

#define env_va_list va_list
#define env_va_start va_start
#define env_va_end va_end
#define env_vsnprintf vsnprintf
#define env_snprintf snprintf

#include <stdlib.h>

#define env_malloc malloc
#define env_free free

#define env_memset memset
#define env_memcmp memcmp

#define env_strdup strdup
#define env_strlen strlen

#endif // __ENV_UNIX_H__