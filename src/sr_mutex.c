/*
 * sr_mutex.c
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of sr_malloc.
 *
 * self-reliance is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * self-reliance is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "sr_mutex.h"



#include "sr_log.h"
#include "sr_error.h"
#include "sr_malloc.h"


struct Sr_mutex{
	pthread_cond_t cond[1];
	pthread_mutex_t mutex[1];
};


int sr_mutex_create(Sr_mutex **pp_mutex)
{
	Sr_mutex *mutex = NULL;

	if (pp_mutex == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if ((mutex = malloc(sizeof(Sr_mutex))) == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	if (pthread_cond_init(mutex->cond, NULL) != 0){
		sr_mutex_release(&mutex);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	if (pthread_mutex_init(mutex->mutex, NULL) != 0){
		sr_mutex_release(&mutex);
		loge(ERRSYSCALL);
		return ERRSYSCALL;
	}

	*pp_mutex = mutex;

	return 0;
}

void sr_mutex_release(Sr_mutex **pp_mutex)
{
	if (pp_mutex && *pp_mutex){
		Sr_mutex *mutex = *pp_mutex;
		*pp_mutex = NULL;
		pthread_cond_destroy(mutex->cond);
		pthread_mutex_destroy(mutex->mutex);
		free(mutex);
	}
}

inline void sr_mutex_lock(Sr_mutex *mutex)
{
	if (mutex){
		if (pthread_mutex_lock(mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_unlock(Sr_mutex *mutex)
{
	if (mutex){
		if (pthread_mutex_unlock(mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_wait(Sr_mutex *mutex)
{
	if (mutex){
		if (pthread_cond_wait(mutex->cond, mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_signal(Sr_mutex *mutex)
{
	if (mutex){
		if (pthread_cond_signal(mutex->cond) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_broadcast(Sr_mutex *mutex)
{
	if (mutex){
		if (pthread_cond_broadcast(mutex->cond) != 0){
			loge(ERRSYSCALL);
		}
	}
}

//int sr_mutex_timedwait(SR_Mutex *mutex, uint32_t millisecond)
//{
//	int result = 0;
//
//	struct timeval delta;
//	struct timespec abstime;
//
//	gettimeofday(&delta, NULL);
//
//	abstime.tv_sec = delta.tv_sec + (millisecond / 1000);
//	abstime.tv_nsec = (delta.tv_usec + (millisecond % 1000) * 1000) * 1000;
//
//	if (abstime.tv_nsec > 1000000000) {
//		abstime.tv_sec += 1;
//		abstime.tv_nsec -= 1000000000;
//	}
//
//	TRYAGAIN:
//	result = pthread_cond_timedwait(mutex->cond, mutex->mutex, &abstime);
//	switch (result) {
//		case EINTR:
//			goto TRYAGAIN;
//		case ETIMEDOUT:
//			result = ERRTIMEOUT;
//			break;
//		case 0:
//			break;
//		default:
//			result = ERRSYSCALL;
//	}
//
//	return result;
//}
