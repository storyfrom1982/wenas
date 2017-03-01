/*
 * sr_mutex.c
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of self-reliance.
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
#include "sr_memory.h"


struct SR_Mutex{
	pthread_cond_t cond[1];
	pthread_mutex_t mutex[1];
};


int sr_mutex_create(SR_Mutex **pp_mutex)
{
	SR_Mutex *mutex = NULL;

	if (pp_mutex == NULL){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if ((mutex = malloc(sizeof(SR_Mutex))) == NULL){
		loge(ERRMEMORY);
		return ERRMEMORY;
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

void sr_mutex_release(SR_Mutex **pp_mutex)
{
	if (pp_mutex && *pp_mutex){
		SR_Mutex *mutex = *pp_mutex;
		*pp_mutex = NULL;
		pthread_cond_destroy(mutex->cond);
		pthread_mutex_destroy(mutex->mutex);
		free(mutex);
	}
}

inline void sr_mutex_lock(SR_Mutex *mutex)
{
	if (mutex){
		if (pthread_mutex_lock(mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_unlock(SR_Mutex *mutex)
{
	if (mutex){
		if (pthread_mutex_unlock(mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_wait(SR_Mutex *mutex)
{
	if (mutex){
		if (pthread_cond_wait(mutex->cond, mutex->mutex) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_signal(SR_Mutex *mutex)
{
	if (mutex){
		if (pthread_cond_signal(mutex->cond) != 0){
			loge(ERRSYSCALL);
		}
	}
}

inline void sr_mutex_broadcast(SR_Mutex *mutex)
{
	if (mutex){
		if (pthread_cond_broadcast(mutex->cond) != 0){
			loge(ERRSYSCALL);
		}
	}
}
