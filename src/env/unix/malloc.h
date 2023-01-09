#ifndef ____UNIX_MALLOC_H__
#define ____UNIX_MALLOC_H__

#include <stddef.h>

extern void* malloc(size_t size);
extern void* calloc(size_t number, size_t size);
extern void* realloc(void* address, size_t size);
extern void* memalign(size_t boundary, size_t size);
extern void* aligned_alloc(size_t alignment, size_t size);
extern void* _aligned_alloc(size_t alignment, size_t size);
extern char* strdup(const char *s);
extern char* strndup(const char *s, size_t n);
extern int posix_memalign(void* *ptr, size_t align, size_t size);
extern void free(void* address);

// extern size_t strlen(const char *s);
// extern int memcmp(const void *s1, const void *s2, size_t n);
// extern void* memcpy(void *dest, const void *src, size_t n);

extern void env_malloc_debug(void (*cb)(const char *debug));

#endif //#ifndef ____UNIX_MALLOC_H__