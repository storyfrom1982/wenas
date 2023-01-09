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

inline uint64_t env_atomic_load(volatile uint64_t* pAtomic)
{
    uint64_t atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();

    return atomic;
}

inline void env_atomic_store(volatile uint64_t* pAtomic, uint64_t var)
{
    _ReadWriteBarrier();
    *pAtomic = var;
    _ReadWriteBarrier();
}

inline uint64_t env_atomic_exchange(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(Exchange)(pAtomic, var);
}

inline bool env_atomic_compare_exchange(volatile uint64_t* pAtomic, uint64_t* pExpected, uint64_t desired)
{
    uint64_t oldval = INTERLOCKED_OP(CompareExchange)(pAtomic, desired, *pExpected);
    return (oldval == *pExpected);
}

inline uint64_t env_atomic_increment(volatile uint64_t* pAtomic)
{
    return INTERLOCKED_OP(Increment)(pAtomic) - 1;
}

inline uint64_t env_atomic_decrement(volatile uint64_t* pAtomic)
{
    return INTERLOCKED_OP(Decrement)(pAtomic) + 1;
}

inline uint64_t env_atomic_add(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, var);
}

inline uint64_t env_atomic_subtract(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(ExchangeAdd)(pAtomic, -(int64_t) var);
}

inline uint64_t env_atomic_and(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(And)(pAtomic, var);
}

inline uint64_t env_atomic_or(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(Or)(pAtomic, var);
}

inline uint64_t env_atomic_xor(volatile uint64_t* pAtomic, uint64_t var)
{
    return INTERLOCKED_OP(Xor)(pAtomic, var);
}

inline __atombool env_atomic_is_true(volatile __atombool* pAtomic)
{
    __atombool atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();
    return atomic == true;
}

inline __atombool env_atomic_is_false(volatile __atombool* pAtomic)
{
    __atombool atomic;
    _ReadWriteBarrier();
    atomic = *pAtomic;
    _ReadWriteBarrier();
    return atomic == false;
}

inline __atombool env_atomic_set_true(volatile __atombool* pAtomic)
{
    __atombool exchange = true, compared = false;
    return INTERLOCKED_OP(CompareExchange)(pAtomic, exchange, compared) == compared;
}

inline __atombool env_atomic_set_false(volatile __atombool* pAtomic)
{
    __atombool exchange = false, compared = true;
    return INTERLOCKED_OP(CompareExchange)(pAtomic, exchange, compared) == compared;
}

void env_backtrace_setup(){}
uint64_t env_backtrace(__ptr* array, int32_t depth)
{
    return 0;
}