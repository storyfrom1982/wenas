/*** The following code is copy from: https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git ***/
#if defined(__GNUC__) || defined(__clang__)

#include <env/env.h>

#if defined(__ATOMIC_RELAXED)

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc11-extensions"
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

inline __uint64 env_atomic_load(volatile __uint64* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST);
}

inline void env_atomic_store(volatile __uint64* pAtomic, __uint64 var)
{
    __atomic_store_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_exchange(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_exchange_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_compare_exchange(volatile __uint64* pAtomic, __uint64* pExpected, __uint64 desired)
{
    return __atomic_compare_exchange_n(pAtomic, pExpected, desired, __false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_increment(volatile __uint64* pAtomic)
{
    return __atomic_fetch_add(pAtomic, 1, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_decrement(volatile __uint64* pAtomic)
{
    return __atomic_fetch_sub(pAtomic, 1, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_add(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_fetch_add(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_subtract(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_fetch_sub(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_and(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_fetch_and(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_or(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_fetch_or(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __uint64 env_atomic_xor(volatile __uint64* pAtomic, __uint64 var)
{
    return __atomic_fetch_xor(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST) == __true;
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST) == __false;
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    __atombool compare = __false;
    return __atomic_compare_exchange_n(pAtomic, &compare, __true, __false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    __atombool compare = __true;
    return __atomic_compare_exchange_n(pAtomic, &compare, __false, __false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

#else //!defined(__ATOMIC_RELAXED)


#if defined(__GNUC__)
#if (__GNUC__ < 4)
#error GCC versions before 4.1.2 are not supported
#elif (defined(__arm__) || defined(__ia64__)) && (__GNUC__ == 4 && __GNUC_MINOR__ < 4)
/* See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36793 Itanium codegen */
/* https://bugs.launchpad.net/ubuntu/+source/gcc-4.4/+bug/491872 ARM codegen*/
/* https://gcc.gnu.org/bugzilla/show_bug.cgi?id=42263 ARM codegen */
#error GCC versions before 4.4.0 are not supported on ARM or Itanium
#elif (defined(__x86_64__) || defined(__i386__)) && (__GNUC__ == 4 && (__GNUC_MINOR__ < 1 || (__GNUC_MINOR__ == 1 && __GNUC_PATCHLEVEL__ < 2)))
/* 4.1.2 is the first gcc version with 100% working atomic intrinsics on Intel */
#error GCC versions before 4.1.2 are not supported on x86/x64
#endif
#endif

static inline void atomicFullBarrier()
{
    __sync_synchronize();
    __asm__ __volatile__("" : : : "memory");
}

inline __uint64 env_atomic_load(volatile __uint64* pAtomic)
{
    __uint64 atomic;

    atomicFullBarrier();
    atomic = *pAtomic;
    atomicFullBarrier();

    return atomic;
}

inline void env_atomic_store(volatile __uint64* pAtomic, __uint64 var)
{
    atomicFullBarrier();
    *pAtomic = var;
    atomicFullBarrier();
}

inline __uint64 env_atomic_exchange(volatile __uint64* pAtomic, __uint64 var)
{
    /*
     * GCC 4.6 and before have only __sync_lock_test_and_set as an exchange operation,
     * which may not support arbitrary values on all architectures. We simply emulate
     * with a CAS instead.
     */

    __uint64 oldval;
    do {
        oldval = *pAtomic;
    } while (!__sync_bool_compare_and_swap(pAtomic, oldval, var));

    /* __sync_bool_compare_and_swap implies a full barrier */

    return oldval;
}

inline __uint64 env_atomic_compare_exchange(volatile __uint64* pAtomic, __uint64* pExpected, __uint64 desired)
{
    __uint64 result = __sync_bool_compare_and_swap(pAtomic, *pExpected, desired);
    if (!result) {
        *pExpected = *pAtomic;
    }

    return result;
}

inline __uint64 env_atomic_increment(volatile __uint64* pAtomic)
{
    return __sync_fetch_and_add(pAtomic, 1);
}

inline __uint64 env_atomic_decrement(volatile __uint64* pAtomic)
{
    return __sync_fetch_and_sub(pAtomic, 1);
}

inline __uint64 env_atomic_add(volatile __uint64* pAtomic, __uint64 var)
{
    return __sync_fetch_and_add(pAtomic, var);
}

inline __uint64 env_atomic_subtract(volatile __uint64* pAtomic, __uint64 var)
{
    return __sync_fetch_and_sub(pAtomic, var);
}

inline __uint64 env_atomic_and(volatile __uint64* pAtomic, __uint64 var)
{
    return __sync_fetch_and_and(pAtomic, var);
}

inline __uint64 env_atomic_or(volatile __uint64* pAtomic, __uint64 var)
{
    return __sync_fetch_and_or(pAtomic, var);
}

inline __uint64 env_atomic_xor(volatile __uint64* pAtomic, __uint64 var)
{
    return __sync_fetch_and_xor(pAtomic, var);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, __true, __true);
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, __false, __false);
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, __false, __true);
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, __true, __false);
}

#endif //defined(__ATOMIC_RELAXED)

#else
#error No atomics implementation for your compiler is available
#endif