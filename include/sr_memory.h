/*
 * sr_memory.h
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

#ifndef INCLUDE_SR_MEMORY_H_
#define INCLUDE_SR_MEMORY_H_


#include <unistd.h>
#include <stdlib.h>
#include <string.h>


//#define ___MEMORY_MANAGER___
//#define ___MEMORY_DEBUG___

//#undef ___MEMORY_DEBUG___

extern int sr_memory_default_init();
extern int sr_memory_init(size_t page_size, size_t preloading_page);
extern void sr_memory_release();
extern void sr_memory_debug(void (*log_debug)(const char *format, ...));


#ifdef ___MEMORY_MANAGER___


#ifdef ___MEMORY_DEBUG___
extern void* sr_memory_malloc(size_t size, const char *file_name, const char *function_name, int line_number);
#else //___MEMORY_DEBUG___
extern void* sr_memory_malloc(size_t size);
#endif //___MEMORY_DEBUG___

#undef malloc

#ifdef ___MEMORY_DEBUG___
# define malloc(s) sr_memory_malloc((s), __FILE__, __FUNCTION__, __LINE__)
#else //___MEMORY_DEBUG___
# define malloc(s) sr_memory_malloc((s))
#endif //___MEMORY_DEBUG___


#ifdef ___MEMORY_DEBUG___
extern void* sr_memory_calloc(size_t number, size_t size, const char *file_name, const char *function_name, int line_number);
#else //___MEMORY_DEBUG___
extern void* sr_memory_calloc(size_t number, size_t size);
#endif //___MEMORY_DEBUG___

#undef calloc

#ifdef ___MEMORY_DEBUG___
# define calloc(n, s) sr_memory_calloc((n), (s), __FILE__, __FUNCTION__, __LINE__)
#else //___MEMORY_DEBUG___
# define calloc(n, s) sr_memory_calloc((n), (s))
#endif //___MEMORY_DEBUG___


#ifdef ___MEMORY_DEBUG___
extern void* sr_memory_realloc(void *address, size_t size, const char *file_name, const char *function_name, int line_number);
#else //___MEMORY_DEBUG___
extern void* sr_memory_realloc(void *address, size_t size);
#endif //___MEMORY_DEBUG___

#undef realloc

#ifdef ___MEMORY_DEBUG___
# define realloc(a, s) sr_memory_realloc((a), (s), __FILE__, __FUNCTION__, __LINE__)
#else //___MEMORY_DEBUG___
# define realloc(a, s) sr_memory_realloc((a), (s))
#endif //___MEMORY_DEBUG___


extern void sr_memory_free(void *address);

#undef free

#define free(a) \
	do { \
		if ((a)) sr_memory_free((a)); \
	} while(0)



//TODO more string function
extern char* sr_string_duplicate(const char *s);
#undef strdup
#define strdup(s)	sr_string_duplicate(s)


#endif //___MEMORY_MANAGER___


#endif /* INCLUDE_SR_MEMORY_H_ */
