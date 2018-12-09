/*
 * sr_malloc.h
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

#ifndef INCLUDE_SR_MEMORY_H_
#define INCLUDE_SR_MEMORY_H_


#ifdef ___SR_MALLOC___
#include <unistd.h>
#include <string.h>
#else
#include <stdlib.h>
#endif //___SR_MALLOC___



extern int sr_malloc_initialize(size_t page_size, size_t preloaded_page);
extern void sr_malloc_release();
extern void sr_malloc_debug(void (*log_debug)(const char *format, ...));



#ifdef ___SR_MALLOC___

#ifdef ___SR_MALLOC_DEBUG___
extern void* sr_malloc(size_t size, const char *file_name, const char *function_name, int line_number);
#else //___SR_MALLOC_DEBUG___
extern void* sr_malloc(size_t size);
#endif //___SR_MALLOC_DEBUG___

#undef malloc

#ifdef ___SR_MALLOC_DEBUG___
# define malloc(s)	sr_malloc((s), __FILE__, __FUNCTION__, __LINE__)
#else //___SR_MALLOC_DEBUG___
# define malloc(s)	sr_malloc((s))
#endif //___SR_MALLOC_DEBUG___


#ifdef ___SR_MALLOC_DEBUG___
extern void* sr_calloc(size_t number, size_t size, const char *file_name, const char *function_name, int line_number);
#else //___SR_MALLOC_DEBUG___
extern void* sr_calloc(size_t number, size_t size);
#endif //___SR_MALLOC_DEBUG___

#undef calloc

#ifdef ___SR_MALLOC_DEBUG___
# define calloc(n, s)	sr_calloc((n), (s), __FILE__, __FUNCTION__, __LINE__)
#else //___SR_MALLOC_DEBUG___
# define calloc(n, s)	sr_calloc((n), (s))
#endif //___SR_MALLOC_DEBUG___


#ifdef ___SR_MALLOC_DEBUG___
extern void* sr_realloc(void *address, size_t size, const char *file_name, const char *function_name, int line_number);
#else //___SR_MALLOC_DEBUG___
extern void* sr_realloc(void *address, size_t size);
#endif //___SR_MALLOC_DEBUG___

#undef realloc

#ifdef ___SR_MALLOC_DEBUG___
# define realloc(a, s)	sr_realloc((a), (s), __FILE__, __FUNCTION__, __LINE__)
#else //___SR_MALLOC_DEBUG___
# define realloc(a, s)	sr_realloc((a), (s))
#endif //___SR_MALLOC_DEBUG___


#ifdef ___SR_MALLOC_DEBUG___
extern void* sr_aligned_alloc(size_t alignment, size_t size, const char *file_name, const char *function_name, int line_number);
#else //___SR_MALLOC_DEBUG___
extern void* sr_aligned_alloc(size_t alignment, size_t size);
#endif //___SR_MALLOC_DEBUG___

#undef aligned_alloc

#ifdef ___SR_MALLOC_DEBUG___
# define aligned_alloc(a, s)	sr_aligned_alloc((a), (s), __FILE__, __FUNCTION__, __LINE__)
#else //___SR_MALLOC_DEBUG___
# define aligned_alloc(a, s)	sr_aligned_alloc((a), (s))
#endif //___SR_MALLOC_DEBUG___


//TODO more string function
#ifdef ___SR_MALLOC_DEBUG___
extern char* sr_strdup(const char *s, const char *file_name, const char *function_name, int line_number);
#else //___SR_MALLOC_DEBUG___
extern char* sr_strdup(const char *s);
#endif //___SR_MALLOC_DEBUG___

#undef strdup

#ifdef ___SR_MALLOC_DEBUG___
# define strdup(s)	sr_strdup((s), __FILE__, __FUNCTION__, __LINE__)
#else //___SR_MALLOC_DEBUG___
# define strdup(s)	sr_strdup((s))
#endif //___SR_MALLOC_DEBUG___


extern void sr_free(void *address);

#undef free

#define free(a) \
	do { \
		if ((a)) sr_free((a)); \
	} while(0)

#endif //___SR_MALLOC___


#endif /* INCLUDE_SR_MEMORY_H_ */
