#ifndef __ENV_ENV_H__
#define __ENV_ENV_H__

#ifdef _WIN32
#   include <env/win/win.h>
#else
#   include <env/unix/unix.h>
#endif


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
typedef void* __ptr;

#define __false         0
#define __true          1


#endif