#include "env/env.h"


#if defined(OS_WINDOWS)

#include <Windows.h>
typedef struct env_thread{
	DWORD id;
	HANDLE handle;
} env_thread_t;
typedef HANDLE	env_thread_cond_t;
typedef CRITICAL_SECTION env_thread_mutex_t;

#else //!defined(OS_WINDOWS)

#include <pthread.h>
typedef struct env_thread{
    pthread_t id;
} env_thread_t;
typedef pthread_cond_t env_thread_cond_t;
typedef pthread_mutex_t env_thread_mutex_t;

#endif //defined(OS_WINDOWS)

#include <stdlib.h>


typedef struct env_mutex {
    env_thread_cond_t cond[1];
    env_thread_mutex_t mutex[1];
}env_mutex_t;


env_thread_t* env_thread_create(__result(*func)(__ptr), __ptr ctx)
{
    env_thread_t *thread = (env_thread_t *)malloc(sizeof(env_thread_t));
#if defined(OS_WINDOWS)
	typedef unsigned __sint32(__stdcall *thread_routine)(__ptr);
	thread->handle = (HANDLE)_beginthreadex(NULL, 0, (thread_routine)func, ctx, 0, (__uint32*)&thread->id);
#else
    typedef __ptr (*thread_routine)(__ptr);
    pthread_create(&thread->id, NULL, (thread_routine)func, ctx);
#endif
    return thread;
}

__uint32 env_thread_self()
{
#if defined(OS_WINDOWS)
	return (__uint32)GetCurrentThreadId();
#else
	return (__uint32)pthread_self();
#endif
}

__result env_thread_detach(env_thread_t *thread)
{
#if defined(OS_WINDOWS)
	CloseHandle(thread->handle);
	return 0;
#else
	return pthread_detach(thread->id);
#endif
}

__result env_thread_destroy(env_thread_t **pp_thread)
{
    __result r = 0;
    if (pp_thread && *pp_thread){
        env_thread_t *thread = *pp_thread;
        *pp_thread = NULL;
#if defined(OS_WINDOWS)
        if(thread->id != GetCurrentThreadId())
            WaitForSingleObjectEx(thread->handle, INFINITE, TRUE);
        CloseHandle(thread->handle);
#else
        void* value = NULL;
        if(pthread_equal(pthread_self(), thread->id))
            r = pthread_detach(thread->id);
        else
            r = pthread_join(thread->id, &value);
#endif
        free(thread);
    }
    return r;
}

__result env_thread_mutex_init(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	InitializeCriticalSection(mutex);
	return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

__void env_thread_mutex_destroy(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	DeleteCriticalSection(mutex);
	return 0;
#else
    pthread_mutex_destroy(mutex);
#endif
}

__void env_thread_mutex_lock(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	EnterCriticalSection(mutex);
	return 0;
#else
    pthread_mutex_lock(mutex);
#endif    
}

__void env_thread_mutex_unlock(env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	LeaveCriticalSection(mutex);
	return 0;
#else
    pthread_mutex_unlock(mutex);
#endif    
}

__result env_thread_cond_init(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
	HANDLE h = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(NULL==h)
		return (int)GetLastError();
	*cond = h;
	return 0;
#else    
    return pthread_cond_init(cond, NULL);
#endif    
}

__void env_thread_cond_destroy(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
	BOOL r = CloseHandle(*cond);
	return r ? 0 : (int)GetLastError();
#else
    pthread_cond_destroy(cond);
#endif     
}

__void env_thread_cond_signal(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
	SetEvent(*cond) ? 0 : (int)GetLastError();
#else
    pthread_cond_signal(cond);
#endif
}

__void env_thread_cond_broadcast(env_thread_cond_t *cond)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
    pthread_cond_broadcast(cond);
#endif
}

__void env_thread_cond_wait(env_thread_cond_t *cond, env_thread_mutex_t *mutex)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*event, INFINITE, TRUE);
	return WAIT_FAILED==r ? GetLastError() : r;
#else
    pthread_cond_wait(cond, mutex);
#endif
}

__result env_thread_cond_timedwait(env_thread_cond_t *cond, env_thread_mutex_t *mutex, __uint64 timeout)
{
#if defined(OS_WINDOWS)
	DWORD r = WaitForSingleObjectEx(*cond, timeout / 1000000ULL, TRUE);
	return WAIT_FAILED == r ? GetLastError() : r;
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
    return r;
#endif // defined(OS_WINDOWS)
}

env_mutex_t* env_mutex_create(__void)
{
    env_mutex_t *mutex = (env_mutex_t*)malloc(sizeof(env_mutex_t));
    env_thread_mutex_init(mutex->mutex);
    env_thread_cond_init(mutex->cond);
    return mutex;
}

__void env_mutex_destroy(env_mutex_t **pp_mutex)
{
    if (pp_mutex && *pp_mutex){
        env_mutex_t *mutex = *pp_mutex;
        *pp_mutex = NULL;
        env_thread_cond_destroy(mutex->cond);
        env_thread_mutex_destroy(mutex->mutex);
        free(mutex);
    }
}

__void env_mutex_lock(env_mutex_t *mutex)
{
    env_thread_mutex_lock(mutex->mutex);
}

__void env_mutex_unlock(env_mutex_t *mutex)
{
    env_thread_mutex_unlock(mutex->mutex);
}

__void env_mutex_signal(env_mutex_t *mutex)
{
    env_thread_cond_signal(mutex->cond);
}

__void env_mutex_broadcast(env_mutex_t *mutex)
{
    env_thread_cond_broadcast(mutex->cond);
}

__void env_mutex_wait(env_mutex_t *mutex)
{
    env_thread_cond_wait(mutex->cond, mutex->mutex);
}

__result env_mutex_timedwait(env_mutex_t *mutex, __sint64 timeout)
{
    return env_thread_cond_timedwait(mutex->cond, mutex->mutex, timeout);
}