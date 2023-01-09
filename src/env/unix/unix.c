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

inline uint64_t env_atomic_load(volatile uint64_t* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST);
}

inline void env_atomic_store(volatile uint64_t* pAtomic, uint64_t var)
{
    __atomic_store_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_exchange(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_exchange_n(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline bool env_atomic_compare_exchange(volatile uint64_t* pAtomic, uint64_t* pExpected, uint64_t desired)
{
    return __atomic_compare_exchange_n(pAtomic, pExpected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_increment(volatile uint64_t* pAtomic)
{
    return __atomic_fetch_add(pAtomic, 1, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_decrement(volatile uint64_t* pAtomic)
{
    return __atomic_fetch_sub(pAtomic, 1, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_add(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_fetch_add(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_subtract(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_fetch_sub(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_and(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_fetch_and(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_or(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_fetch_or(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline uint64_t env_atomic_xor(volatile uint64_t* pAtomic, uint64_t var)
{
    return __atomic_fetch_xor(pAtomic, var, __ATOMIC_SEQ_CST);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST) == true;
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    return __atomic_load_n(pAtomic, __ATOMIC_SEQ_CST) == false;
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    __atombool compare = false;
    return __atomic_compare_exchange_n(pAtomic, &compare, true, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    __atombool compare = true;
    return __atomic_compare_exchange_n(pAtomic, &compare, false, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
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

inline uint64_t env_atomic_load(volatile uint64_t* pAtomic)
{
    uint64_t atomic;

    atomicFullBarrier();
    atomic = *pAtomic;
    atomicFullBarrier();

    return atomic;
}

inline void env_atomic_store(volatile uint64_t* pAtomic, uint64_t var)
{
    atomicFullBarrier();
    *pAtomic = var;
    atomicFullBarrier();
}

inline uint64_t env_atomic_exchange(volatile uint64_t* pAtomic, uint64_t var)
{
    /*
     * GCC 4.6 and before have only __sync_lock_test_and_set as an exchange operation,
     * which may not support arbitrary values on all architectures. We simply emulate
     * with a CAS instead.
     */

    uint64_t oldval;
    do {
        oldval = *pAtomic;
    } while (!__sync_bool_compare_and_swap(pAtomic, oldval, var));

    /* __sync_bool_compare_and_swap implies a full barrier */

    return oldval;
}

inline bool env_atomic_compare_exchange(volatile uint64_t* pAtomic, uint64_t* pExpected, uint64_t desired)
{
    return __sync_bool_compare_and_swap(pAtomic, *pExpected, desired);
}

inline uint64_t env_atomic_increment(volatile uint64_t* pAtomic)
{
    return __sync_fetch_and_add(pAtomic, 1);
}

inline uint64_t env_atomic_decrement(volatile uint64_t* pAtomic)
{
    return __sync_fetch_and_sub(pAtomic, 1);
}

inline uint64_t env_atomic_add(volatile uint64_t* pAtomic, uint64_t var)
{
    return __sync_fetch_and_add(pAtomic, var);
}

inline uint64_t env_atomic_subtract(volatile uint64_t* pAtomic, uint64_t var)
{
    return __sync_fetch_and_sub(pAtomic, var);
}

inline uint64_t env_atomic_and(volatile uint64_t* pAtomic, uint64_t var)
{
    return __sync_fetch_and_and(pAtomic, var);
}

inline uint64_t env_atomic_or(volatile uint64_t* pAtomic, uint64_t var)
{
    return __sync_fetch_and_or(pAtomic, var);
}

inline uint64_t env_atomic_xor(volatile uint64_t* pAtomic, uint64_t var)
{
    return __sync_fetch_and_xor(pAtomic, var);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, true, true);
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, false, false);
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, false, true);
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    return __sync_bool_compare_and_swap(pAtomic, true, false);
}

#endif //defined(__ATOMIC_RELAXED)

#else
#error No atomics implementation for your compiler is available
#endif