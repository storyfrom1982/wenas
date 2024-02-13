#ifndef __XATOM_H__
#define __XATOM_H__

#include <stdint.h>

typedef uint64_t __atom_size;
typedef uint64_t __atom_bool;

// Built-in Function: bool __sync_bool_compare_and_swap (type *ptr, type oldval, type newval, ...)
// __int64 _InterlockedCompareExchange64_HLERelease(
//    __int64 volatile * Destination,
//    __int64 Exchange,
//    __int64 Comparand
// );

#if defined(__linux__) || defined(__linux)
#   define __is_true(x)             __sync_bool_compare_and_swap(&(x), true, true)
#   define __is_false(x)            __sync_bool_compare_and_swap(&(x), false, false)
#   define __set_true(x)            __sync_bool_compare_and_swap(&(x), false, true)
#   define __set_false(x)           __sync_bool_compare_and_swap(&(x), true, false)
#   define __atom_add(x, y)         __sync_add_and_fetch(&(x), (y))
#   define __atom_sub(x, y)         __sync_sub_and_fetch(&(x), (y))
#   define __atom_set(x, y)         __sync_lock_test_and_set(&(x), (y))
#elif defined(WIN64) || defined(_WIN64) || defined(__WIN64__)
#   define __is_true(x)             InterlockedCompareExchange64(&(x), true, true)
#   define __is_false(x)            InterlockedCompareExchange64(&(x), false, false)
#   define __set_true(x)            InterlockedCompareExchange64(&(x), true, false)
#   define __set_false(x)           InterlockedCompareExchange64(&(x), false, true)
#   define __atom_add(x, y)         InterlockedExchangeAdd64(&(x), (y))
#   define __atom_sub(x, y)         InterlockedExchangeAdd64(&(x), -(y))
#   define __atom_set(x, y)         InterlockedExchange64(&(x), (y))
#else
#   error "This OS is unsupported !"
#endif

#define __atom_lock(x)			while(!___set_true(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define __atom_try_lock(x)		___set_true(x)
#define __atom_unlock(x)		___set_false(x)

#endif