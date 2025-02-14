#ifndef __XALLOC_H__
#define __XALLOC_H__

#include <xapi/xapi.h>

#define XEOF    0xFFFFFFFFFFFFFFFFUL
// #define XNULL   ((void*)XEOF)

#define __sizeof_ptr    sizeof(void*)

#ifdef __cplusplus
extern "C" {
#endif

#ifdef XALLOC_ENABLE
extern void* malloc(size_t size);
extern void* calloc(size_t number, size_t size);
extern void* realloc(void* address, size_t size);
extern void* memalign(size_t boundary, size_t size);
extern void* aligned_alloc(size_t alignment, size_t size);
extern void* _aligned_alloc(size_t alignment, size_t size);
extern int posix_memalign(void* *ptr, size_t align, size_t size);
extern void free(void* address);
// extern void free_test(void* address);
extern char* strdup(const char *s);
extern char* strndup(const char *s, size_t n);
#else //XALLOC_ENABLE
// # include <stdlib.h>
#endif //XALLOC_ENABLE

extern void* xalloc(size_t size);
extern void* xcalloc(size_t count, size_t size);
extern void xfree(void* address);
// 如果编译报错，跟 C 库冲突，就改成 xlen, xcopy, xcompare
extern size_t xlen(const char *s);
extern void xclear(void *ptr, size_t len);
extern void* xcopy(void *dst, const void *src, size_t n);
extern int xcompare(const void *s1, const void *s2, size_t n);
extern void xalloc_leak_trace(void (*cb)(const char *s, uint64_t pid));

#ifdef __cplusplus
}
#endif

#endif //__XALLOC_H__
