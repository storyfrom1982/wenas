/*** The following code is copy from: https://github.com/awslabs/amazon-kinesis-video-streams-webrtc-sdk-c.git ***/
#if !(defined(_M_IX86) || defined(_M_X64))
#error Atomics are not currently supported for non-x86 MSVC platforms
#endif

#include <env/env.h>

#ifdef _M_IX86
#define INTERLOCKED_OP(x) _Interlocked##x
#else
#define INTERLOCKED_OP(x) _Interlocked##x##64
#endif

inline __uint64 env_atomic_load(volatile __uint64* pAtomic)
{
    __uint64 atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();

    return atomic;
}

inline void env_atomic_store(volatile __uint64* pAtomic, __uint64 var)
{
    _ReadWriteBarrier();
    *pAtomic = var;
    _ReadWriteBarrier();
}

inline __uint64 env_atomic_exchange(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(Exchange)(pAtomic, var);
}

inline __bool_ env_atomic_compare_exchange(volatile __uint64* pAtomic, __uint64* pExpected, __uint64 desired)
{
    __uint64 oldval = INTERLOCKED_OP(CompareExchange)(pAtomic, desired, *pExpected);
    return (oldval == *pExpected);
}

inline __uint64 env_atomic_increment(volatile __uint64* pAtomic)
{
    return INTERLOCKED_OP(Increment)(pAtomic) - 1;
}

inline __uint64 env_atomic_decrement(volatile __uint64* pAtomic)
{
    return INTERLOCKED_OP(Decrement)(pAtomic) + 1;
}

inline __uint64 env_atomic_add(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, var);
}

inline __uint64 env_atomic_subtract(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, -(__sint64) var);
}

inline __uint64 env_atomic_and(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(And)(pAtomic, var);
}

inline __uint64 env_atomic_or(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(Or)(pAtomic, var);
}

inline __uint64 env_atomic_xor(volatile __uint64* pAtomic, __uint64 var)
{
    return INTERLOCKED_OP(Xor)(pAtomic, var);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    __atombool atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();
    return atomic == __true;
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    __atombool atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();
    return atomic == __false;
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    __atombool exchange = __true, compared = __false;
    return InterlockedCompareExchange64(pAtomic, exchange, compared) == compared;
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    __atombool exchange = __false, compared = __true;
    return InterlockedCompareExchange64(pAtomic, exchange, compared) == compared;
}