#include "unix.h"

#include <errno.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>

#include <errno.h>
#include <string.h>

#include <stdlib.h>
#include <stdarg.h>

// #define env_strftime strftime

// #define env_va_list va_list
// #define env_va_start va_start
// #define env_va_end va_end
// #define env_vsnprintf vsnprintf
// #define env_snprintf snprintf

// #define env_malloc malloc
// #define env_free free

// #define env_memset memset
// #define env_memcmp memcmp

// #define env_strdup strdup
// #define env_strlen strlen


enum aws_timestamp_unit {
    AWS_TIMESTAMP_SECS = 1,
    AWS_TIMESTAMP_MILLIS = 1000,
    AWS_TIMESTAMP_MICROS = 1000000,
    AWS_TIMESTAMP_NANOS = 1000000000,
};


#ifdef __APPLE__

#include <mach/mach.h>
#include <mach/mach_time.h>

static uint64_t (*time_func)(void);
static mach_timebase_info_data_t timebase;

inline static void uv__hrtime_init_once(void) {
  if (KERN_SUCCESS != mach_timebase_info(&timebase))
    abort();
  time_func = (uint64_t (*)(void)) dlsym(RTLD_DEFAULT, "mach_continuous_time");
  if (time_func == NULL)
    time_func = mach_absolute_time;
}

inline static uint64_t uv__hrtime() {
  return time_func() * timebase.numer / timebase.denom;
}

#else // LINUX

inline static uint64_t uv__hrtime() {
  struct timespec ts;
  //https://man7.org/linux/man-pages/man2/clock_getres.2.html
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0){
      return ts.tv_sec * AWS_TIMESTAMP_NANOS + ts.tv_nsec;
  }
  abort();
  return 0;
}


#endif


void unix_init(void)
{
#ifdef __APPLE__
  uv__hrtime_init_once();
#endif
}

uint64_t env_time(void)
{
  struct timespec ts;
  //https://man7.org/linux/man-pages/man2/clock_getres.2.html
  if (clock_gettime(CLOCK_REALTIME, &ts) == 0){
      return ts.tv_sec * AWS_TIMESTAMP_NANOS + ts.tv_nsec;
  }else {
      struct timeval tv;
      //https://man7.org/linux/man-pages/man2/settimeofday.2.html
      if (gettimeofday(&tv, NULL) == 0){
          return tv.tv_sec * AWS_TIMESTAMP_NANOS + tv.tv_usec * 1000UL;
      }
  }
  abort();
  return 0;  
}

uint64_t env_sys_time(void)
{
  return uv__hrtime();
}

int env_status(void)
{
    return errno;
}

char* env_status_describe(int status)
{
    return strerror(status);
}