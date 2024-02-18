#ifndef __XMALLOC_H__
#define __XMALLOC_H__

#include <ex/ex.h>

#define ENDSYM  0xFFFFFFFFFFFFFFFF
// #define NULL  ((void*)0)

#ifdef __cplusplus
extern "C" {
#endif

extern void* malloc(size_t size);
extern void* calloc(size_t number, size_t size);
extern void* realloc(void* address, size_t size);
extern void* memalign(size_t boundary, size_t size);
extern void* aligned_alloc(size_t alignment, size_t size);
extern void* _aligned_alloc(size_t alignment, size_t size);
extern int posix_memalign(void* *ptr, size_t align, size_t size);
extern void free(void* address);
// extern void free_test(void* address);
extern void xmalloc_leak_trace(void (*cb)(const char *leak_location));

extern char* strdup(const char *s);
extern char* strndup(const char *s, size_t n);

// 如果编译报错，跟 C 库冲突，就改成 slength, mcopy, mcompare
extern size_t slength(const char *s);
extern int mcompare(const void *s1, const void *s2, size_t n);
extern void* mcopy(void *dst, const void *src, size_t n);
extern void mclear(void *ptr, size_t len);

#ifdef __cplusplus
}
#endif

#endif //__XMALLOC_H__