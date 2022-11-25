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

#include <stddef.h>

extern void sr_malloc_debug(void (*cb)(const char *fmt, ...));

extern void* malloc(size_t size);
extern void* calloc(size_t number, size_t size);
extern void* realloc(void *address, size_t size);
extern void* memalign(size_t boundary, size_t size);
extern void* aligned_alloc(size_t alignment, size_t size);
extern void* _aligned_alloc(size_t alignment, size_t size);
extern void free(void *address);
extern char* strdup(const char *s);
extern char* strndup(const char *s, size_t n);
extern int posix_memalign(void **ptr, size_t align, size_t size);

#endif /* INCLUDE_SR_MEMORY_H_ */
