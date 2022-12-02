#ifndef __ENV_MALLOC_H__
#define __ENV_MALLOC_H__

#include <stddef.h>

extern void env_malloc_debug(void (*cb)(const char *fmt));

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

#endif /* __ENV_MALLOC_H__ */
