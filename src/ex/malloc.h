#ifndef ____UNIX_MALLOC_H__
#define ____UNIX_MALLOC_H__


//不再映射 malloc 调用，使用 dma 分配内存，宏定义方式，在 Debug 模式下，除了跟踪堆栈，还可以定位文件和行数
//增加引用计数，配合 Message Channel 实现跨线程作用域的内存自动释放
//实现 memcpy 与 memcmp

//将c代码在头文件中实现，为适应原子操作在不同环境中的差异 // typedef struct __cpp_atombool __atombool;



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

#endif //#ifndef ____UNIX_MALLOC_H__