#include "malloc.h"

#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>

#include "xatom.h"

#ifdef XMALLOC_BACKTRACE

#define __USE_GNU
#include <dlfcn.h>
#include <stdio.h>
#include <unwind.h>
#define XMALLOC_BACKTRACE_DEPTH	    16

struct backtrace_stack {
    void** head;
    void** end;
};

static inline _Unwind_Reason_Code __unwind_backtrace_callback(struct _Unwind_Context* unwind_context, void* vp)
{
    struct backtrace_stack* stack = (struct backtrace_stack*)vp;
    if (stack->head == stack->end) {
		//数组已满
        return _URC_END_OF_STACK;
    }
    *stack->head = (void*)_Unwind_GetIP(unwind_context);
    if (*stack->head == NULL) {
        return _URC_END_OF_STACK;
    }
    ++stack->head;
    return _URC_NO_REASON;
}

static inline int64_t __backtrace(void** array, int depth)
{
    struct backtrace_stack stack = {0};
    stack.head = &array[0];
    stack.end = &array[0] + (depth - 2);
    _Unwind_Backtrace(__unwind_backtrace_callback, &stack);
    return stack.head - &array[0];
}

#endif //#ifdef XMALLOC_BACKTRACE

// typedef size_t	__atombool;

// #define	__is_true(x)	    	__sync_bool_compare_and_swap(&(x), true, true)
// #define	__is_false(x)	    	__sync_bool_compare_and_swap(&(x), false, false)
// #define	__set_true(x)		    __sync_bool_compare_and_swap(&(x), false, true)
// #define	__set_false(x)		    __sync_bool_compare_and_swap(&(x), true, false)

// #define __atom_sub(x, y)		__sync_sub_and_fetch(&(x), (y))
// #define __atom_add(x, y)		__sync_add_and_fetch(&(x), (y))

// #define __atom_lock(x)			while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 10L}}, NULL)
// #define __atom_try_lock(x)		__set_true(x)
// #define __atom_unlock(x)		__set_false(x)

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

#ifdef XMALLOC_PAGE_SIZE
# define __page_size                XMALLOC_PAGE_SIZE
#else
# define __page_size                0xA00000
#endif

#ifdef XMALLOC_MAX_POOL
# define	__max_pool_number		XMALLOC_MAX_POOL
#else
# define	__max_pool_number		16
#endif

#define	__max_page_number			1024

#define	__max_recycle_pool			3

#define	__align_size				( sizeof(size_t) )
#define	__align_mask				( __align_size - 1 )


/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////


#ifdef XMALLOC_BACKTRACE_DEPTH
# define __backtrace_size			( sizeof(void*) * XMALLOC_BACKTRACE_DEPTH )
#else
# define __backtrace_size			0
#endif //XMALLOC_BACKTRACE


/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////


#define	__free_ptr_size				( __backtrace_size + __align_size * 4 )
#define	__work_ptr_size				( __backtrace_size + __align_size * 2 )
#define	__req2align(req)			( ( (req) + __work_ptr_size + __align_mask ) & ~__align_mask )


/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////


typedef struct xpointer {

	/**
	 * 已分配状态：
	 * bit(21->31)		未使用
	 * bit(11->20)		内存池索引
	 * bit(1->10)		分页索引
	 * bit(0)		指针状态标志位		bit(0)==1 已分配		bit(0)==0 已释放
	 * 已释放状态：
	 * 前一个指针的大小
	 */

	size_t flag;


	/**
	 * 当前指针的大小
	 */

	size_t size;


	/**
	 * 定位指针信息
	 */

#ifdef	XMALLOC_BACKTRACE
	char trace[__backtrace_size];
#endif

	/**
	 * 指针队列索引
	 */

	struct xpointer *prev;
	struct xpointer *next;

}__xptr;


#define	__pointer2address(p)		( (void *)((char *)( p ) + __work_ptr_size ) )
#define	__address2pointer(a)		( (__xptr *)( (char *)( a ) - __work_ptr_size ) )

#define	__inuse						0x01
#define	__mergeable(p)				( ! ( ( p )->flag & __inuse ) )

#define	__prev_pointer(p)			( (__xptr *)( (char *)( p ) - ( p )->flag ) )
#define	__next_pointer(p)			( (__xptr *)( (char *)( p ) + ( p )->size ) )
#define	__assign_pointer(p, s)		( (__xptr *)( (char *)( p ) + ( s ) ) )


typedef struct xptr_list{
	__atom_bool lock;
	__xptr *head;
	size_t memory_size;
}xptr_list_t;


typedef struct xmalloc_page{

	__atom_bool lock;

	//本页的索引
	size_t id;

	//本页内存大小
	size_t size;

	//本页内存起始地址
	void *start_address;

	/**
	 * 本页的最大可分陪指针
	 * 每个新指针从这个指针中分裂出来
	 * 每个释放的指针都与这个指针合并
	 */
	__xptr *ptr;

	/**
	 * 已释放但未合并的指针队列
	 */
	__xptr *head, *end;

	/**
	 * 为快速释放指针设计的回收池
	 */
	xptr_list_t recycle_pool[__max_recycle_pool];

}xmalloc_page_t;


typedef struct xmalloc_pool{
    __atom_bool lock;
    size_t page_number;
    size_t id;
	void *aligned_address;
	xmalloc_page_t page[__max_page_number + 1];
}xmalloc_pool_t;


typedef struct xmalloc{

    __atom_bool lock;
    __atom_bool init;
    size_t pool_index;
	size_t page_size;
	size_t pool_number;
	size_t preloading_page;
	size_t page_aligned_mask;
	xmalloc_pool_t pool[__max_pool_number + 1];

}xmalloc_t;


static xmalloc_t memory_manager = {0}, *mm = &memory_manager;


static int malloc_page(xmalloc_pool_t *pool, xmalloc_page_t *page, size_t page_size)
{
	page->size = (page_size + (__free_ptr_size * 4) + mm->page_aligned_mask) & (~mm->page_aligned_mask);
	pool->aligned_address = NULL;

	do{
		page->start_address = mmap(pool->aligned_address,
				page->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page->start_address == MAP_FAILED){
			abort();
		}else{
			if (((size_t)(page->start_address) & __align_mask) != 0){
				pool->aligned_address = (void *)(((size_t)(page->start_address) + __align_mask) & ~__align_mask);
				munmap(page->start_address, page->size);
			}else{
				pool->aligned_address = page->start_address + page->size;
				break;
			}
		}
	}while ( 1 );
	// printf(">>>>-------------------------------------------> %lu\n", page->id);
	page->head = (__xptr*)(page->start_address);
	page->head->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	page->head->size = __free_ptr_size;

	page->ptr = __next_pointer(page->head);
	page->ptr->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	page->ptr->size = page->size - (__free_ptr_size << 1);

	page->end = __next_pointer(page->ptr);
	page->end->flag = page->ptr->size;
	page->end->size = __free_ptr_size;

	page->head->prev = NULL;
	page->head->next = page->end;
	page->end->prev = page->head;
	page->end->next = NULL;

	memset(&(page->recycle_pool), 0, sizeof(page->recycle_pool));

	return 0;
}


static int malloc_pool()
{
    __atom_lock(mm->lock);

    if (__is_true(mm->init)){
        __atom_unlock(mm->lock);
        return 0;
    }

    mm->page_size = __page_size;
    mm->preloading_page = 2;
    mm->page_aligned_mask = sysconf(_SC_PAGESIZE) - 1;

    for (mm->pool_number = 0; mm->pool_number < __max_pool_number; ++mm->pool_number){
        mm->pool[mm->pool_number].id = mm->pool_number;
        for (size_t page_id = 0; page_id < mm->preloading_page; ++page_id){
            mm->pool[mm->pool_number].page[page_id].id = page_id;
            mm->pool[mm->pool_number].page_number ++;
            if (malloc_page(&(mm->pool[mm->pool_number]),
                            &(mm->pool[mm->pool_number].page[page_id]), mm->page_size) != 0){
                __atom_unlock(mm->lock);
                return -1;
            }
        }
    }

    __set_true(mm->init);
    __atom_unlock(mm->lock);

    return 0;
}


static void free_page(xmalloc_page_t *page, xmalloc_pool_t *pool)
{
	//TODO 是否从最顶端的分页开始释放
	////当然最好能够回收空闲的内存，但是需要在线程退出时回收内存，需要封装线程来配合
	////这里现在什么也不做，不必担心什么，当前进程的内存池的容量会保持在峰值时的大小，不回收空闲的内存也没有关系
}


inline static void free_pointer(__xptr *ptr, xmalloc_page_t *page, xmalloc_pool_t *pool)
{

	if (__next_pointer(ptr) == page->ptr){

		//合并左边指针
		if (__mergeable(ptr)){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			__prev_pointer(ptr)->size += ptr->size;
			ptr = __prev_pointer(ptr);
			ptr->next->prev = ptr->prev;
			ptr->prev->next = ptr->next;
		}

		//更新空闲指针大小
		ptr->size += page->ptr->size;
		//合并指针
		__next_pointer(ptr)->flag = ptr->size;
		page->ptr = ptr;

	}else if (__next_pointer(page->ptr) == ptr){

		//合并右边指针
		if (__mergeable(__next_pointer(__next_pointer(ptr)))){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			__next_pointer(ptr)->prev->next = __next_pointer(ptr)->next;
			__next_pointer(ptr)->next->prev = __next_pointer(ptr)->prev;
			ptr->size += __next_pointer(ptr)->size;
		}

		//合并指针
		page->ptr->size += ptr->size;
		__next_pointer(page->ptr)->flag = page->ptr->size;

	}else{
		/**
		 * 不能与主指针合并的指针就先尝试与左右两边的指针合并
		 */
		if (__mergeable(ptr)){
			__prev_pointer(ptr)->size += ptr->size;
			ptr = __prev_pointer(ptr);
			__next_pointer(ptr)->flag = ptr->size;
			ptr->next->prev = ptr->prev;
			ptr->prev->next = ptr->next;
		}

		if (__mergeable(__next_pointer(__next_pointer(ptr)))){
			__next_pointer(ptr)->prev->next = __next_pointer(ptr)->next;
			__next_pointer(ptr)->next->prev = __next_pointer(ptr)->prev;
			ptr->size += __next_pointer(ptr)->size;
		}

		//设置释放状态
		__next_pointer(ptr)->flag = ptr->size;
		//把最大指针放入队列首
		if (ptr->size >= page->head->next->size){
			ptr->next = page->head->next;
			ptr->prev = page->head;
			ptr->next->prev = ptr;
			ptr->prev->next = ptr;
		}else{
			ptr->next = page->end;
			ptr->prev = page->end->prev;
			ptr->next->prev = ptr;
			ptr->prev->next = ptr;
		}
	}
}


static void flush_page(xmalloc_page_t *page, xmalloc_pool_t *pool)
{
	__xptr *ptr = NULL;
	for (size_t i = 0; i < __max_recycle_pool; ++i){
		while(page->recycle_pool[i].head){
			ptr = page->recycle_pool[i].head;
			page->recycle_pool[i].head = page->recycle_pool[i].head->next;
			free_pointer(ptr, page, pool);
		}
	}
}


static void flush_cache()
{
	for (size_t pool_id = 0; pool_id < mm->pool_number; ++pool_id){
		for (size_t page_id = 0; page_id < mm->pool[pool_id].page_number; ++page_id){
			if (mm->pool[pool_id].page[page_id].start_address != NULL){
				flush_page(&(mm->pool[pool_id].page[page_id]), &(mm->pool[pool_id]));
			}
		}
	}
}

// #include <stdio.h>
// void free_test(void* address)
// {
// 	size_t page_id = 0;
// 	size_t pool_id = 0;
// 	__xptr *ptr = NULL;
// 	xmalloc_page_t *page = NULL;
// 	xmalloc_pool_t *pool = NULL;

// 	if (address){
// 		ptr = __address2pointer(address);
// 		printf("ptr->size == %lu || ptr->flag == %lu\n", ptr->size, ptr->flag);

// 		pool_id = ((__next_pointer(ptr)->flag >> 11) & 0x3FF);
// 		printf("pool_number %lu pool_id %lu pool[pool_id].id %lu\n", mm->pool_number, pool_id, mm->pool[pool_id].id);

// 		pool = &(mm->pool[pool_id]);
// 		page_id = ((__next_pointer(ptr)->flag >> 1) & 0x3FF);
// 		printf("page_number %lu page_id %lu page[page_id].id %lu\n", pool->page_number, page_id, pool->page[page_id].id);
// 	}
// }

void free(void* address)
{
	size_t page_id = 0;
	size_t pool_id = 0;
	__xptr *ptr = NULL;
	xmalloc_page_t *page = NULL;
	xmalloc_pool_t *pool = NULL;

	if (address){
		ptr = __address2pointer(address);

		if (ptr->size == 0 || ptr->flag == 0){
			// printf("ptr->size == 0 || ptr->flag == 0");
			abort();
		}

		pool_id = ((__next_pointer(ptr)->flag >> 11) & 0x3FF);
		if ((pool_id >= mm->pool_number) || (pool_id != mm->pool[pool_id].id)){
			// printf("pool_number %lu pool_id %lu pool[pool_id].id %lu\n", mm->pool_number, pool_id, mm->pool[pool_id].id);
			abort();
		}

		pool = &(mm->pool[pool_id]);
		page_id = ((__next_pointer(ptr)->flag >> 1) & 0x3FF);
		if ((page_id >= pool->page_number) || page_id != pool->page[page_id].id){
			// printf("page_number %lu page_id %lu page[page_id].id %lu\n", pool->page_number, page_id, pool->page[page_id].id);
			abort();
		}

		page = &(pool->page[page_id]);
		if (__atom_try_lock(page->lock)){
			free_pointer(ptr, page, pool);
			__atom_unlock(page->lock);
		}else{
			for (size_t i = 0; ; i = (i + 1) % __max_recycle_pool){
				if (__atom_try_lock(page->recycle_pool[i].lock)){
					ptr->next = page->recycle_pool[i].head;
					page->recycle_pool[i].head = ptr;
					page->recycle_pool[i].memory_size += ptr->size;
					if (__atom_try_lock(page->lock)){
						while(page->recycle_pool[i].head){
							ptr = page->recycle_pool[i].head;
							page->recycle_pool[i].head = page->recycle_pool[i].head->next;
							page->recycle_pool[i].memory_size -= ptr->size;
							free_pointer(ptr, page, pool);
						}
						__atom_unlock(page->lock);
					}
					__atom_unlock(page->recycle_pool[i].lock);
					break;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void* malloc(size_t size)
{
	// printf("malloc >>>>>>>--------------------------------------------------->>>> enter\n");
	__xptr *ptr = NULL;
	size_t reserved_size = 0;

	if (__is_false(mm->init)){
        if (malloc_pool() != 0){
            return NULL;
        }
	}

    xmalloc_pool_t *pool = &mm->pool[(__atom_add(mm->pool_index, 1) & (__max_pool_number-1))];

	size = __req2align(size);

	//确保分配一个新的指针之后剩余的内存不会小于一个空闲指针的大小
	reserved_size = size + __free_ptr_size;


	for(size_t i = 0; i < pool->page_number; ++i){

		if (!__atom_try_lock(pool->page[i].lock)){
			continue;
		}

		if (reserved_size > pool->page[i].ptr->size){

			if (pool->page[i].head->next->size > reserved_size){

				//把当前指针加入到释放队列尾
				pool->page[i].ptr->next = pool->page[i].end;
				pool->page[i].ptr->prev = pool->page[i].end->prev;
				pool->page[i].ptr->next->prev = pool->page[i].ptr;
				pool->page[i].ptr->prev->next = pool->page[i].ptr;
				//设置当前指针为释放状态
				__next_pointer(pool->page[i].ptr)->flag = pool->page[i].ptr->size;
				//使用空闲队列中最大的指针作为当前指针
				pool->page[i].ptr = pool->page[i].head->next;
				//从空闲队列中移除
				pool->page[i].head->next = pool->page[i].head->next->next;
				pool->page[i].head->next->prev = pool->page[i].head;

			}else{

				__atom_unlock(pool->page[i].lock);
				continue;
			}
		}

		//分配一个新的指针
		ptr = __assign_pointer(pool->page[i].ptr, size);
		ptr->flag = ((pool->id << 11) | (i << 1) | __inuse);
		ptr->size = pool->page[i].ptr->size - size;
		__next_pointer(ptr)->flag = ptr->size;
		ptr->prev = pool->page[i].ptr;
		pool->page[i].ptr = ptr;
		ptr = pool->page[i].ptr->prev;
		ptr->size = size;

		__atom_unlock(pool->page[i].lock);

#ifdef XMALLOC_BACKTRACE
		// ptr->trace 的开始位置用来存储获取跟踪堆栈的深度
		*((int64_t*)ptr->trace) = __backtrace(((void**)ptr->trace) + 1, XMALLOC_BACKTRACE_DEPTH - 1);
#endif

		// printf("malloc >>>>>>>--------------------------------------------------->>>> exit\n");
		return __pointer2address(ptr);
	}


	//创建一个分页
	xmalloc_page_t *page = NULL;

	while(pool->page_number < __max_page_number){
		size_t page_id = pool->page_number;
		if (__atom_try_lock(pool->page[page_id].lock)){
			if (pool->page[page_id].id == 0){
				pool->page[page_id].id = page_id;
				page = &(pool->page[page_id]);
				break;
			}
			__atom_unlock(pool->page[page_id].lock);
		}
	}

	if (pool->page_number >= __max_page_number){
		__atom_unlock(page->lock);
		abort();
	}

	if (size > mm->page_size){
		if (malloc_page(pool, page, size) != 0){
			__atom_unlock(page->lock);
			abort();
		}
	}else{
		if (malloc_page(pool, page, mm->page_size) != 0){
			__atom_unlock(page->lock);
			abort();
		}
	}

	//分配一个新的指针
	ptr = __assign_pointer(page->ptr, size);
	ptr->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	ptr->size = page->ptr->size - size;
	__next_pointer(ptr)->flag = ptr->size;
	ptr->prev = page->ptr;
	page->ptr = ptr;
	ptr = page->ptr->prev;
	ptr->size = size;

	__atom_add(pool->page_number, 1);
	__atom_unlock(page->lock);

#ifdef XMALLOC_BACKTRACE
    *((int64_t*)ptr->trace) = __backtrace(((void**)ptr->trace) + 1, XMALLOC_BACKTRACE_DEPTH - 1);
#endif

	return __pointer2address(ptr);
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void* calloc(size_t number, size_t size)
{
	size *= number;
	// size += 3;
	// printf("number=%lu size=%lu\n", number, size);
	
	void *ptr = malloc(size);

	if (ptr != NULL){
		size_t l = size >> 3;
		for (size_t i = 0; i < l; ++i){
			*(((size_t*)ptr) + i) = 0;
		}
		for (size_t r = size % 8; r > 0; r--){
			*(((char*)ptr) + (size - r)) = 0;
		}
		//检测是否清零
		// char *c = (char*)ptr;
		// for (size_t t = 0; t < size; ++t){
		// 	if (c[t] != 0){
		// 		printf("memset failed\n");
		// 		exit(0);
		// 	}
		// }
	}

	return ptr;
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void* realloc(void* address, size_t size)
{
	void *new_address = NULL;
	__xptr *old_pointer = __address2pointer(address);

	if (size > 0){

		new_address = malloc(size);

		if (new_address != NULL){

			if (address != NULL){
				if (size > old_pointer->size - __work_ptr_size){
					memcpy(new_address, address, old_pointer->size - __work_ptr_size);
				}else{
					memcpy(new_address, address, size);
				}
			}

			free(address);
			return new_address;
		}

	}else{

		if (address != NULL){
			free(address);
		}
	}

    return NULL;
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void* aligned_alloc(size_t alignment, size_t size)
{
    if (alignment == __align_size){
        return malloc(size);
    }

	void *address = NULL, *aligned_address = NULL;
	__xptr *pointer = NULL, *aligned_pointer = NULL;

	if (size == 0 || alignment == 0
			|| (alignment & (sizeof (void *) - 1)) != 0
			|| (alignment & (alignment - 1)) != 0){
		errno = EINVAL;
		return NULL;
	}

	address = malloc(size + alignment + __free_ptr_size);

	if (address != NULL){
		if (((size_t)( address ) & ~(alignment - 1)) == ((size_t)( address ))){
			return address;
		}
		aligned_address = (void*)(((size_t)(address) + alignment + __free_ptr_size) & ~(alignment - 1));
		aligned_pointer = __address2pointer(aligned_address);
		pointer = __address2pointer(address);
		size_t free_size = (size_t)aligned_pointer - (size_t)pointer;

		aligned_pointer->size = pointer->size - free_size;
		aligned_pointer->flag = __next_pointer(pointer)->flag;
		pointer->size = free_size;

#ifdef XMALLOC_BACKTRACE
		memcpy(aligned_pointer->trace, pointer->trace, __backtrace_size);
#endif

		free(address);
		return aligned_address;
	}

	errno = ENOMEM;

	return NULL;
}

void* memalign(size_t boundary, size_t size)
{
	return aligned_alloc(boundary, size);
}

void* _aligned_alloc(size_t alignment, size_t size)
{
    return aligned_alloc(alignment, size);
}

int posix_memalign(void* *ptr, size_t align, size_t size)
{
    *ptr = aligned_alloc(align, size);
    if (NULL == *ptr){
        return -1;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

char* strdup(const char *s)
{
	char *result = NULL;
	if (s){
		size_t len = strlen(s);

		result = (char *)malloc(len + 1);

		if (result != NULL){
		    memcpy(result, s, len);
		    result[len] = '\0';
		}
	}

	return result;
}

char* strndup(const char *s, size_t n)
{
	char *result = NULL;
	if (s && n > 0){

		result = (char *)malloc(n + 1);

		if (result != NULL){
			memcpy(result, s, n);
			result[n] = '\0';
		}
	}

	return result;
}

size_t slength(const char *s)
{
    if (!s){
		return 0;
	}
    size_t l = 0;
    while (s[l] != '\0'){
        l++;
    }
    return l;
}

int mcompare(const void *s1, const void *s2, size_t n){
    assert(s1);
    assert(s2);
    if (!n){
        return 0;
    }
    while (--n && *(char*)s1 == *(char*)s2){
        s1 = (char*)s1 + 1;
        s2 = (char*)s2 + 1;
    }
    return (*(char*)s1 - *(char*)s2);
}

void* mcopy(void *dst, const void *src, size_t n)
{
    assert(dst);
    assert(src);
    if (!n){
        return dst;
    }
    size_t l = n / 8;
    for (size_t i = 0; i < l; ++i){
        *(((size_t*)dst) + i) = *(((size_t*)src) + i);
    }
    for (size_t r = n % 8; r > 0; r--){
        *(((char*)dst) + (n - r)) = *(((char*)src) + (n - r));
    }
    return dst;
}

void mclear(void *ptr, size_t len)
{
	if (ptr != NULL){
		size_t l = len >> 3;
		for (size_t i = 0; i < l; ++i){
			*(((size_t*)ptr) + i) = 0;
		}
		for (size_t r = len % 8; r > 0; r--){
			*(((char*)ptr) + (len - r)) = 0;
		}
		// char *c = (char*)ptr;
		// for (size_t t = 0; t < len; ++t){
		// 	if (c[t] != 0){
		// 		// printf("memset failed\n");
		// 		exit(0);
		// 	}
		// }
	}
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void xmalloc_release()
{
    __atom_lock(mm->lock);
    for (int pool_id = 0; pool_id < mm->pool_number; ++pool_id){
        xmalloc_pool_t *pool = &(mm->pool[pool_id]);
        for (int page_id = 0; page_id < pool->page_number; ++page_id){
            if ( pool->page[page_id].start_address != NULL){
                munmap(pool->page[page_id].start_address, pool->page[page_id].size);
            }
        }
    }
    memset(mm, 0, sizeof(xmalloc_t));
    __atom_unlock(mm->lock);
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////

void xmalloc_leak_trace(void (*cb)(const char *leak_location))
{
	__xptr *ptr = NULL;
	xmalloc_pool_t *pool = NULL;

	if (cb == NULL){
		return;
	}

	size_t buf_size = 10240;
	char buf[10240];

	flush_cache();

	for (size_t pool_id = 0; pool_id < mm->pool_number; ++pool_id){
		pool = &(mm->pool[pool_id]);
		for (size_t page_id = 0; page_id < pool->page_number; ++page_id){
			if (pool->page[page_id].start_address
				&& pool->page[page_id].ptr->size
				!= pool->page[page_id].size - __free_ptr_size * 2){
				cb("[!!! Found a memory leak !!!]");
				ptr = (__next_pointer(pool->page[page_id].head));
				while(ptr != pool->page[page_id].end){
#ifdef XMALLOC_BACKTRACE
					if (!__mergeable(__next_pointer(ptr))){
						size_t n = 0;
						int64_t count = *((int64_t*)ptr->trace);
						void** buffer = ((void**)ptr->trace) + 1;
						for (size_t idx = 0; idx < count; ++idx) {
							const void* addr = buffer[idx];
							Dl_info info= {0};
							if (dladdr(addr, &info) && info.dli_sname) {
								if (n + strlen(info.dli_sname) > buf_size - 1){
									break;
								}
								n += snprintf(buf + n, buf_size - 1 - n, "@%s ", info.dli_sname);
							}
						}
						buf[n] = '\0';
						cb(buf);
					}
#endif
					ptr = __next_pointer(ptr);
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
////
/////////////////////////////////////////////////////////////////////////////
