#ifndef __MUTEX_H__
#define __MUTEX_H__


#ifdef __cplusplus

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>

#define ___mutex    std::mutex
#define ___signal   std::condition_variable

#define ___mutex_lock(m)                (m)->lock()
#define ___mutex_try_lock(m)            (m)->try_lock()
#define ___mutex_unlock(m)              (m)->unlock()

#define ___signal_notify(s)             (s)->notify_one()
#define ___signal_broadcast(s)          (s)->notify_all()
#define ___signal_wait(s, m)            (s)->wait()
#define ___signal_wait_timer(s, m, t)   (s)->wait_until()

#else //__cplusplus

#include <pthread.h>

#define ___mutex    pthread_mutex_t
#define ___signal   pthread_cond_t


#endif //__cplusplus





#endif //__MUTEX_H__