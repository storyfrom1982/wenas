#ifndef __ENV_UNIX_H__
#define __ENV_UNIX_H__


#include <stdint.h>
#include <stddef.h>
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


#endif // __ENV_UNIX_H__