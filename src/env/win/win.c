#include "win.h"

#include <time.h>
#include <math.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <direct.h>
#include <winsock2.h>
#include <winperf.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <windows.h>
#include <userenv.h>


enum aws_timestamp_unit {
    AWS_TIMESTAMP_SECS = 1,
    AWS_TIMESTAMP_MILLIS = 1000,
    AWS_TIMESTAMP_MICROS = 1000000,
    AWS_TIMESTAMP_NANOS = 1000000000,
};


static const uint64_t FILE_TIME_TO_NS = 100;
static const uint64_t EC_TO_UNIX_EPOCH = 11644473600LL;
static const uint64_t WINDOWS_TICK = 10000000;

static INIT_ONCE s_timefunc_init_once = INIT_ONCE_STATIC_INIT;
typedef VOID WINAPI timefunc_t(LPFILETIME);
static VOID WINAPI s_get_system_time_func_lazy_init(LPFILETIME filetime_p);
static timefunc_t *volatile s_p_time_func = s_get_system_time_func_lazy_init;


/**
 * Multiplies a * b. If the result overflows, returns 2^64 - 1.
 */
static inline uint64_t aws_mul_u64_saturating(uint64_t a, uint64_t b) {
    if (a > 0 && b > 0 && a > (UINT64_MAX / b))
        return UINT64_MAX;
    return a * b;
}

/**
 * Adds a + b.  If the result overflows returns 2^64 - 1.
 */
static inline uint64_t aws_add_u64_saturating(uint64_t a, uint64_t b) {
    if ((b > 0) && (a > (UINT64_MAX - b)))
        return UINT64_MAX;
    return a + b;
}

/**
 * Converts 'timestamp' from unit 'convert_from' to unit 'convert_to', if the units are the same then 'timestamp' is
 * returned. If 'remainder' is NOT NULL, it will be set to the remainder if convert_from is a more precise unit than
 * convert_to (but only if the old frequency is a multiple of the new one). If conversion would lead to integer
 * overflow, the timestamp returned will be the highest possible time that is representable, i.e. UINT64_MAX.
 */
static inline uint64_t
    aws_timestamp_convert_u64(uint64_t ticks, uint64_t old_frequency, uint64_t new_frequency, uint64_t *remainder) {

    assert(old_frequency > 0 && new_frequency > 0);

    /*
     * The remainder, as defined in the contract of the original version of this function, only makes mathematical
     * sense when the old frequency is a positive multiple of the new frequency.  The new convert function needs to be
     * backwards compatible with the old version's remainder while being a lot more accurate with its conversions
     * in order to handle extreme edge cases of large numbers.
     */
    if (remainder != NULL) {
        *remainder = 0;
        /* only calculate remainder when going from a higher to lower frequency */
        if (new_frequency < old_frequency) {
            uint64_t frequency_remainder = old_frequency % new_frequency;
            /* only calculate remainder when the old frequency is evenly divisible by the new one */
            if (frequency_remainder == 0) {
                uint64_t frequency_ratio = old_frequency / new_frequency;
                *remainder = ticks % frequency_ratio;
            }
        }
    }

    /*
     * Now do the actual conversion.
     */
    uint64_t old_seconds_elapsed = ticks / old_frequency;
    uint64_t old_remainder = ticks - old_seconds_elapsed * old_frequency;

    uint64_t new_ticks_whole_part = aws_mul_u64_saturating(old_seconds_elapsed, new_frequency);

    /*
     * This could be done in one of three ways:
     *
     * (1) (old_remainder / old_frequency) * new_frequency - this would be completely wrong since we know that
     * (old_remainder / old_frequency) < 1 = 0
     *
     * (2) old_remainder * (new_frequency / old_frequency) - this only gives a good solution when new_frequency is
     * a multiple of old_frequency
     *
     * (3) (old_remainder * new_frequency) / old_frequency - this is how we do it below, the primary concern is if
     * the initial multiplication can overflow.  For that to be the case, we would need to be using old and new
     * frequencies in the billions.  This does not appear to be the case in any current machine's hardware counters.
     *
     * Ignoring arbitrary frequencies, even a nanosecond to nanosecond conversion would not overflow either.
     *
     * If this did become an issue, we would potentially need to use intrinsics/platform support for 128 bit math.
     *
     * For review consideration:
     *   (1) should we special case frequencies being a multiple of the other?
     *   (2) should we special case frequencies being the same?  A ns-to-ns conversion does the full math and
     *   approaches overflow (but cannot actually do so).
     */
    uint64_t new_ticks_remainder_part = aws_mul_u64_saturating(old_remainder, new_frequency) / old_frequency;

    return aws_add_u64_saturating(new_ticks_whole_part, new_ticks_remainder_part);
}

static BOOL CALLBACK s_get_system_time_init_once(PINIT_ONCE init_once, PVOID param, PVOID *context) {
    (void)init_once;
    (void)param;
    (void)context;

    HMODULE kernel = GetModuleHandleW("Kernel32.dll");
    timefunc_t *time_func = (timefunc_t *)GetProcAddress(kernel, "GetSystemTimePreciseAsFileTime");

    if (time_func == NULL) {
        time_func = GetSystemTimeAsFileTime;
    }

    s_p_time_func = time_func;

    return TRUE;
}

static VOID WINAPI s_get_system_time_func_lazy_init(LPFILETIME filetime_p) {
    BOOL status = InitOnceExecuteOnce(&s_timefunc_init_once, s_get_system_time_init_once, NULL, NULL);

    if (status) {
        (*s_p_time_func)(filetime_p);
    } else {
        /* Something went wrong in static initialization? Should never happen, but deal with it somehow...*/
        GetSystemTimeAsFileTime(filetime_p);
    }
}

static uint64_t aws_high_res_clock_get_ticks(void) {
    LARGE_INTEGER ticks, frequency;
    /* QPC runs on sub-microsecond precision, convert to nanoseconds */
    if (QueryPerformanceFrequency(&frequency) && QueryPerformanceCounter(&ticks)) {
        return aws_timestamp_convert_u64(
            (uint64_t)ticks.QuadPart, (uint64_t)frequency.QuadPart, AWS_TIMESTAMP_NANOS, NULL);
    }

    return 0;
}

static uint64_t aws_sys_clock_get_ticks(void) {
    FILETIME ticks;
    /*GetSystemTimePreciseAsFileTime() returns 100 nanosecond precision. Convert to nanoseconds.
     *Also, this function returns a different epoch than unix, so we add a conversion to handle that as well. */
    (*s_p_time_func)(&ticks);

    /*if this looks unnecessary, see:
     * https://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
     */
    ULARGE_INTEGER int_conv;
    int_conv.LowPart = ticks.dwLowDateTime;
    int_conv.HighPart = ticks.dwHighDateTime;

    return (int_conv.QuadPart - (WINDOWS_TICK * EC_TO_UNIX_EPOCH)) * FILE_TIME_TO_NS;
}

uint64_t env_time()
{
  return aws_sys_clock_get_ticks();
}


uint64_t env_sys_time()
{
  return aws_high_res_clock_get_ticks();
}

void win32_init(void)
{

}