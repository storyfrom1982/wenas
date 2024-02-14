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
#   include <intrin.h>
#   pragma intrinsic(_InterlockedCompareExchange64, _InterlockedExchangeAdd64, _InterlockedExchange64)
#   define __is_true(x)             _InterlockedCompareExchange64((__int64*)&(x), true, true)
#   define __is_false(x)            _InterlockedCompareExchange64((__int64*)&(x), false, false)
#   define __set_true(x)            _InterlockedCompareExchange64((__int64*)&(x), true, false)
#   define __set_false(x)           _InterlockedCompareExchange64((__int64*)&(x), false, true)
#   define __atom_add(x, y)         _InterlockedExchangeAdd64((__int64*)&(x), (y))
#   define __atom_sub(x, y)         _InterlockedExchangeAdd64((__int64*)&(x), -(y))
#   define __atom_set(x, y)         _InterlockedExchange64((__int64*)&(x), (y))
#else
#   error "This OS is unsupported !"
#endif

#define __atom_lock(x)			while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define __atom_try_lock(x)		__set_true(x)
#define __atom_unlock(x)		__set_false(x)

#endif