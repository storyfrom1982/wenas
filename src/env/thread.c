#include "env/env.h"


#if defined(OS_WINDOWS)

#include <Windows.h>
typedef PCONDITION_VARIABLE env_thread_cond_t;
typedef CRITICAL_SECTION env_thread_mutex_t;
#else //!defined(OS_WINDOWS)
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;
#endif //defined(OS_WINDOWS)

#include <stdlib.h>


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


__result env_thread_create(env_thread_t *tid, env_thread_cb cb, __ptr ctx)
{
    __result r = 0;
#if defined(OS_WINDOWS)
	typedef __result(__stdcall *thread_routine)(__ptr);
	*tid = (__uint64)_beginthreadex(NULL, 0, (thread_routine)cb, ctx, 0, NULL);
    if (*tid == NULL) r = -1;
#else
    typedef __ptr (*thread_routine)(__ptr);
    r = pthread_create((pthread_t *)tid, NULL, (thread_routine)cb, ctx);
    errno = r;
#endif
    return r;
}

env_thread_t env_thread_self()
{
#if defined(OS_WINDOWS)
	return (env_thread_t)GetCurrentThread();
#else
	return (env_thread_t)pthread_self();
#endif
}

void env_thread_sleep(__uint64 nano_seconds)
{
#if defined(OS_WINDOWS)
    nano_seconds /= 1000000ULL;
	if (nano_seconds < 1) nano_seconds = 1;
    Sleep(nano_seconds);
#else
    nano_seconds /= 1000ULL;
    if (nano_seconds < 1) nano_seconds = 1;
	usleep(nano_seconds);
#endif
}

__result env_thread_destroy(env_thread_t tid)
{
    __result r = 0;
#if defined(OS_WINDOWS)
        CloseHandle((HANDLE)tid);
#else
        r = pthread_join((pthread_t)tid, NULL);
        errno = r;
#endif
    return r;
}

__result env_thread_mutex_init(env_thread_mutex_t *mutex)
{
    __result r = 0;
#if defined(OS_WINDOWS)
	InitializeCriticalSection(mutex);
#else
    r = pthread_mutex_init(mutex, NULL);
    errno = r;
#endif
    return r;
}

void env_thread_mutex_destroy(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

void env_thread_mutex_lock(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif    
}

void env_thread_mutex_unlock(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif    
}

__result env_thread_cond_init(env_thread_cond_t *cond)
{
    __result r = 0;
#if defined(OS_WINDOWS)
    InitializeConditionVariable(cond);
#else    
    r = pthread_cond_init(cond, NULL);
    errno = r;
#endif
    return r;
}

void env_thread_cond_destroy(env_thread_cond_t *cond)
{
#if !defined(OS_WINDOWS)
    pthread_cond_destroy(cond);
#endif     
}

void env_thread_cond_signal(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
	WakeConditionVariable(cond);
#else
    pthread_cond_signal(cond);
#endif
}

void env_thread_cond_broadcast(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
    WakeAllConditionVariable(cond);
#else
    pthread_cond_broadcast(cond);
#endif
}

void env_thread_cond_wait(env_thread_cond_t *cond, env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
    env_thread_mutex_unlock(mutex);
    SleepConditionVariableCS(cond, mutex, INFINITE);
    env_thread_mutex_lock(mutex);
#else
    pthread_cond_wait(cond, mutex);
#endif
}

__result env_thread_cond_timedwait(env_thread_cond_t *cond, env_thread_mutex_t *mutex, __uint64 timeout)
{
#if defined(OS_WINDOWS)
    env_thread_mutex_unlock(mutex);
    timeout /= 1000000ULL;
    if (timeout < 1) timeout = 1;
    BOOL b = SleepConditionVariableCS(cond, mutex, timeout);
    env_thread_mutex_lock(mutex);
	return b ? 0 : GetLastError() == ERROR_TIMEOUT ? ENV_TIMEDOUT : -1;
#else // !defined(OS_WINDOWS)
#if defined(CLOCK_REALTIME)
	__result r = 0;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
    timeout += (__uint64)ts.tv_sec * NANO_SECONDS + ts.tv_nsec;
#else // !defined(CLOCK_REALTIME)
	__result r = 0;
	struct timeval tv;
	struct timespec ts;
	gettimeofday(&tv, NULL);
    timeout += (__uint64)tv.tv_sec * NANO_SECONDS + tv.tv_usec * MILLI_SECONDS;
#endif // defined(CLOCK_REALTIME)
    ts.tv_sec = timeout / NANO_SECONDS;
    ts.tv_nsec = timeout % NANO_SECONDS;
	r = pthread_cond_timedwait(cond, mutex, &ts);
    errno = r;
    return r == 0 ? 0 : r == ETIMEDOUT ? ENV_TIMEDOUT : -1;
#endif // defined(OS_WINDOWS)
}

__result env_mutex_init(env_mutex_t *mutex)
{
    __result r;
    r = env_thread_mutex_init(mutex->mutex);
    if (r == 0){
        r = env_thread_cond_init(mutex->cond);
        if (r != 0){
            env_thread_mutex_destroy(mutex->mutex);
        }
    }
    return r;
}

env_mutex_t* env_mutex_create(void)
{
    env_mutex_t *mutex = (env_mutex_t*)malloc(sizeof(env_mutex_t));
    if (mutex == NULL){
        return NULL;
    }
    if (env_mutex_init(mutex) != 0){
        free(mutex);
        mutex = NULL;
    }
    return mutex;
}

void env_mutex_destroy(env_mutex_t **pp_mutex)
{
    if (pp_mutex && *pp_mutex){
        env_mutex_t *mutex = *pp_mutex;
        *pp_mutex = NULL;
        env_thread_cond_destroy(mutex->cond);
        env_thread_mutex_destroy(mutex->mutex);
        free(mutex);
    }
}

void env_mutex_lock(env_mutex_t *mutex)
{
    env_thread_mutex_lock(mutex->mutex);
}

void env_mutex_unlock(env_mutex_t *mutex)
{
    env_thread_mutex_unlock(mutex->mutex);
}

void env_mutex_signal(env_mutex_t *mutex)
{
    env_thread_cond_signal(mutex->cond);
}

void env_mutex_broadcast(env_mutex_t *mutex)
{
    env_thread_cond_broadcast(mutex->cond);
}

void env_mutex_wait(env_mutex_t *mutex)
{
    env_thread_cond_wait(mutex->cond, mutex->mutex);
}

__result env_mutex_timedwait(env_mutex_t *mutex, __uint64 timeout)
{
    return env_thread_cond_timedwait(mutex->cond, mutex->mutex, timeout);
}