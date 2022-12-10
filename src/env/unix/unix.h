#ifndef __ENV_UNIX_H__
#define __ENV_UNIX_H__


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <errno.h>
#include <string.h>

static inline int env_status(void)
{
    return errno;
}

static inline char* env_status_describe(int status)
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


static inline uint64_t env_time()
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