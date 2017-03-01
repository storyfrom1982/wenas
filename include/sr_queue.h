/*
 * sr_queue.h
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

#ifndef INCLUDE_SR_QUEUE_H_
#define INCLUDE_SR_QUEUE_H_


#include "sr_error.h"
#include "sr_atom.h"

#ifdef ___MEMORY_MANAGER___
# include "sr_memory.h"
#else
# include <stdlib.h>
#endif



#define SR_QUEUE_ENABLE(x) \
    struct { \
        struct x *prev; \
        struct x *next; \
    }


#define SR_QUEUE_DEFINE(x) \
    struct x##_Queue \
	{ \
        bool lock; \
        bool stopped; \
        uint32_t size; \
        uint32_t pushable; \
        uint32_t popable; \
        void (*clean)(void*); \
        x head; \
        x end; \
    }


#define SR_QUEUE_DECLARE(x)	struct x##_Queue


#define sr_queue_init(q, s) \
    do { \
        (q)->lock = false; \
        (q)->stopped = false; \
        (q)->size = (s); \
        (q)->pushable = (s); \
        (q)->popable = 0; \
        (q)->head.prev = NULL; \
        (q)->head.next = &((q)->end); \
        (q)->end.next = NULL; \
        (q)->end.prev = &((q)->head); \
        (q)->clean = NULL; \
    }while(0)


#define sr_queue_push_to_head(q, x, result) \
	do { \
		if ((x) == NULL){ \
			result = ERRPARAM; \
			break; \
		} \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if (ISTRUE((q)->stopped)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			result = ERRCANCEL; \
			break; \
		} \
		if ((q)->pushable == 0){ \
			SR_ATOM_UNLOCK((q)->lock); \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x)->next = (q)->head.next; \
		(x)->next->prev = (x); \
		(x)->prev = &((q)->head); \
		(q)->head.next = (x); \
		(q)->popable++; \
		(q)->pushable--; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_push_to_end(q, x, result) \
	do { \
		if (NULL == (x)){ \
			result = ERRPARAM; \
			break; \
		} \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if (ISTRUE((q)->stopped)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			result = ERRCANCEL; \
			break; \
		} \
		if ((q)->pushable == 0){ \
			SR_ATOM_UNLOCK((q)->lock); \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x)->prev = (q)->end.prev; \
		(x)->prev->next = (x); \
		(q)->end.prev = (x); \
		(x)->next = &((q)->end); \
		(q)->popable++; \
		(q)->pushable--; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_pop_first(q, x, result) \
	do { \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if ((q)->popable == 0 || (q)->head.next == &((q)->end)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			if (ISTRUE((q)->stopped)){ \
				result = ERRCANCEL; \
				break; \
			} \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x) = (q)->head.next; \
		(q)->head.next = (x)->next; \
		(x)->next->prev = &(q)->head; \
		(q)->pushable++; \
		(q)->popable--; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_pop_last(q, x, result) \
	do { \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if ((q)->popable == 0 || (q)->end.prev == &((q)->head)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			if (ISTRUE((q)->stopped)){ \
				result = ERRCANCEL; \
				break; \
			} \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x) = (q)->end.prev; \
		(q)->end.prev = (x)->prev; \
		(x)->prev->next = &(q)->end; \
		(q)->pushable++; \
		(q)->popable--; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_get_first(q, x, result) \
	do { \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if ((q)->popable == 0 || (q)->head.next == &((q)->end)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			if (ISTRUE((q)->stopped)){ \
				result = ERRCANCEL; \
				break; \
			} \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x) = (q)->head.next; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_get_end(q, x, result) \
	do { \
		if (!SR_ATOM_TRYLOCK((q)->lock)){ \
			result = ERRTRYAGAIN; \
			break; \
		} \
		if ((q)->popable == 0 || (q)->end.prev == &((q)->head)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			if (ISTRUE((q)->stopped)){ \
				result = ERRCANCEL; \
				break; \
			} \
			result = ERRTRYAGAIN; \
			break; \
		} \
		(x) = (q)->end.prev; \
		result = 0; \
	}while(0)


#define sr_queue_remove(q, x, result) \
	do { \
		if ((q)->popable < 0 || NULL == (x) || NULL == (x)->prev || NULL == (x)->next){ \
			result = ERRPARAM; \
			break; \
		} \
		SR_ATOM_LOCK((q)->lock); \
		if (ISTRUE((q)->stopped)){ \
			SR_ATOM_UNLOCK((q)->lock); \
			result = ERRCANCEL; \
			break; \
		} \
		(x)->prev->next = (x)->next; \
		(x)->next->prev = (x)->prev; \
		(q)->pushable++; \
		(q)->popable--; \
		SR_ATOM_UNLOCK((q)->lock); \
		result = 0; \
	}while(0)


#define sr_queue_clean(q) \
	do { \
		SR_ATOM_LOCK((q)->lock); \
		while ((q)->popable > 0 && (q)->head.next != &((q)->end)){ \
			(q)->head.prev = (q)->head.next; \
			(q)->head.next = (q)->head.next->next; \
			if ((q)->clean) (q)->clean((q)->head.prev); \
			else free((q)->head.prev); \
			(q)->popable--; \
		} \
		(q)->head.prev = NULL; \
		(q)->head.next = &((q)->end); \
		(q)->end.next = NULL; \
		(q)->end.prev = &((q)->head); \
		(q)->pushable = (q)->size; \
		(q)->popable = 0; \
		SR_ATOM_UNLOCK((q)->lock); \
	}while(0)


#define sr_queue_stop(q) \
	do { \
		SR_ATOM_LOCK((q)->lock); \
		SETTRUE((q)->stopped); \
		SR_ATOM_UNLOCK((q)->lock); \
	}while(0)


#define sr_queue_pushable(q) (q)->pushable


#define sr_queue_popable(q) (q)->popable


#endif /* INCLUDE_SR_QUEUE_H_ */
