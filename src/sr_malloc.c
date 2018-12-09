/*
 * sr_malloc.c
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


#include "sr_malloc.h"


#ifdef ___SR_MALLOC___


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>



#define	__max_pool_number			1024
#define	__max_page_number			1024

#define	__max_recycle_bin_number	3

#define	__align_size				( sizeof(size_t) )
#define	__align_mask				( __align_size - 1 )

#define	__free_pointer_size			( __debug_msg_size + __align_size * 4 )
#define	__work_pointer_size			( __debug_msg_size + __align_size * 2 )

#define	__min_assign_size			( __free_pointer_size )
#define	__min_memory_size			( __free_pointer_size - __work_pointer_size )

#define	request2allocation(req) \
		( ( (req) + __work_pointer_size < __min_assign_size ) ? __min_assign_size : \
				( (req) + __work_pointer_size + __align_mask ) & ~__align_mask )


#ifdef ___SR_MALLOC_DEBUG___
# define __debug_msg_size			(256 - (__align_size << 2))
#else
# define __debug_msg_size			0
#endif //___SR_MALLOC_DEBUG___


typedef struct pointer_t {

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

#ifdef	___SR_MALLOC_DEBUG___
	char debug_msg[__debug_msg_size];
#endif

	/**
	 * 指针队列索引
	 */

	struct pointer_t *prev;
	struct pointer_t *next;

}pointer_t;


#define	__pointer2address(p)		( (void *)((char *)( p ) + __work_pointer_size ) )
#define	__address2pointer(a)		( (pointer_t *)( (char *)( a ) - __work_pointer_size ) )

#define	__inuse						0x01
#define	__mergeable(p)				( ! ( ( p )->flag & __inuse ) )

#define	__prev_pointer(p)			( (pointer_t *)( (char *)( p ) - ( p )->flag ) )
#define	__next_pointer(p)			( (pointer_t *)( (char *)( p ) + ( p )->size ) )
#define	__assign_pointer(p, s)		( (pointer_t *)( (char *)( p ) + ( s ) ) )


typedef struct pointer_queue_t{
	bool lock;
	pointer_t *head;
	size_t memory_size;
}pointer_queue_t;


typedef struct sr_memory_page_t{

	bool lock;

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
	pointer_t *pointer;

	/**
	 * 已释放但未合并的指针队列
	 */
	pointer_t *head, *end;

	/**
	 * 为快速释放指针设计的回收池
	 */
	pointer_queue_t recycle_bin[__max_recycle_bin_number];

}sr_memory_page_t;


typedef struct sr_memory_pool_t{
	bool lock;
	size_t id;
	size_t page_number;
	void *aligned_address;
	pthread_t tid;
	sr_memory_page_t page[__max_page_number + 1];
}sr_memory_pool_t;


typedef struct sr_memory_manager_t{

	bool lock;
	size_t page_size;
	size_t pool_number;
	size_t thread_number;
	size_t preloading_page;
	size_t page_aligned_mask;

	pthread_key_t key;
	sr_memory_pool_t pool[__max_pool_number + 1];

}sr_memory_manager_t;


static sr_memory_manager_t memory_manager = {0}, *mm = &memory_manager;


#define	__is_true(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	__is_false(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	__set_true(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	__set_false(x)		__sync_bool_compare_and_swap(&(x), true, false)

#define __atom_sub(x, y)		__sync_sub_and_fetch(&(x), (y))
#define __atom_add(x, y)		__sync_add_and_fetch(&(x), (y))

#define __atom_lock(x)			while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 10L}}, NULL)
#define __atom_try_lock(x)		__set_true(x)
#define __atom_unlock(x)		__set_false(x)



static int assign_page(sr_memory_pool_t *pool, sr_memory_page_t *page, size_t page_size)
{
	page->size = (page_size + mm->page_aligned_mask) & (~mm->page_aligned_mask);

	pool->aligned_address = NULL;

	do{
		page->start_address = mmap(pool->aligned_address,
				page->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page->start_address == MAP_FAILED){
			return -1;
		}else{
			if (((size_t)(page->start_address) & __align_mask) != 0){
				pool->aligned_address = (void *)(((size_t)(page->start_address) + __align_mask) & ~__align_mask);
				munmap(page->start_address, page->size);
			}else{
				pool->aligned_address = page->start_address + page->size;
				break;
			}
		}
	}while ( true );

	page->head = (pointer_t*)(page->start_address);
	page->head->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	page->head->size = __free_pointer_size;

	page->pointer = __next_pointer(page->head);
	page->pointer->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	page->pointer->size = page->size - (__free_pointer_size << 1);

	page->end = __next_pointer(page->pointer);
	page->end->flag = page->pointer->size;
	page->end->size = __free_pointer_size;

	page->head->prev = NULL;
	page->head->next = page->end;
	page->end->prev = page->head;
	page->end->next = NULL;

	memset(&(page->recycle_bin), 0, sizeof(page->recycle_bin));

	return 0;
}


static void free_page(sr_memory_page_t *page, sr_memory_pool_t *pool)
{
	//TODO 是否从最顶端的分页开始释放
	////当然最好能够回收空闲的内存，但是需要在线程退出时回收内存，需要封装线程来配合
	////这里现在什么也不做，不必担心什么，当前进程的内存池的容量会保持在峰值时的大小，不回收空闲的内存也没有关系
}


inline static void free_pointer(pointer_t *pointer, sr_memory_page_t *page, sr_memory_pool_t *pool)
{

	if (__next_pointer(pointer) == page->pointer){

		//合并左边指针
		if (__mergeable(pointer)){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			__prev_pointer(pointer)->size += pointer->size;
			pointer = __prev_pointer(pointer);
			pointer->next->prev = pointer->prev;
			pointer->prev->next = pointer->next;
		}

		//更新空闲指针大小
		pointer->size += page->pointer->size;
		//合并指针
		__next_pointer(pointer)->flag = pointer->size;
		page->pointer = pointer;

	}else if (__next_pointer(page->pointer) == pointer){

		//合并右边指针
		if (__mergeable(__next_pointer(__next_pointer(pointer)))){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			__next_pointer(pointer)->prev->next = __next_pointer(pointer)->next;
			__next_pointer(pointer)->next->prev = __next_pointer(pointer)->prev;
			pointer->size += __next_pointer(pointer)->size;
		}

		//合并指针
		page->pointer->size += pointer->size;
		__next_pointer(page->pointer)->flag = page->pointer->size;

	}else{
		/**
		 * 不能与主指针合并的指针就先尝试与左右两边的指针合并
		 */
		if (__mergeable(pointer)){
			__prev_pointer(pointer)->size += pointer->size;
			pointer = __prev_pointer(pointer);
			__next_pointer(pointer)->flag = pointer->size;
			pointer->next->prev = pointer->prev;
			pointer->prev->next = pointer->next;
		}

		if (__mergeable(__next_pointer(__next_pointer(pointer)))){
			__next_pointer(pointer)->prev->next = __next_pointer(pointer)->next;
			__next_pointer(pointer)->next->prev = __next_pointer(pointer)->prev;
			pointer->size += __next_pointer(pointer)->size;
		}

		//设置释放状态
		__next_pointer(pointer)->flag = pointer->size;
		//把最大指针放入队列首
		if (pointer->size >= page->head->next->size){
			pointer->next = page->head->next;
			pointer->prev = page->head;
			pointer->next->prev = pointer;
			pointer->prev->next = pointer;
		}else{
			pointer->next = page->end;
			pointer->prev = page->end->prev;
			pointer->next->prev = pointer;
			pointer->prev->next = pointer;
		}
	}
#if 0
	if (page->pointer->size
			+ page->recycle_bin[0].memory_size
			+ page->recycle_bin[1].memory_size
			+ page->recycle_bin[2].memory_size
			== mm->page_size
			- __free_pointer_size
			- __free_pointer_size){
		free_page(page, pool);
	}
#endif
}


static void flush_page(sr_memory_page_t *page, sr_memory_pool_t *pool)
{
	pointer_t *pointer = NULL;
	for (size_t i = 0; i < __max_recycle_bin_number; ++i){
		while(page->recycle_bin[i].head){
			pointer = page->recycle_bin[i].head;
			page->recycle_bin[i].head = page->recycle_bin[i].head->next;
			free_pointer(pointer, page, pool);
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


void sr_free(void *address)
{
	size_t page_id = 0;
	size_t pool_id = 0;
	pointer_t *pointer = NULL;
	sr_memory_page_t *page = NULL;
	sr_memory_pool_t *pool = NULL;

	if (address){

		pointer = __address2pointer(address);
		pool_id = ((__next_pointer(pointer)->flag >> 11) & 0x3FF);
		if ((pool_id >= mm->pool_number) || (pool_id != mm->pool[pool_id].id)){
			return;
		}
		pool = &(mm->pool[pool_id]);

		page_id = ((__next_pointer(pointer)->flag >> 1) & 0x3FF);
		if ((page_id >= pool->page_number) || page_id != pool->page[page_id].id){
			return;
		}
		page = &(pool->page[page_id]);

		if (__atom_try_lock(page->lock)){
			free_pointer(pointer, page, pool);
			__atom_unlock(page->lock);
		}else{
			for (size_t i = 0; ; i = ++i % __max_recycle_bin_number){
				if (__atom_try_lock(page->recycle_bin[i].lock)){
					pointer->next = page->recycle_bin[i].head;
					page->recycle_bin[i].head = pointer;
					page->recycle_bin[i].memory_size += pointer->size;
					if (__atom_try_lock(page->lock)){
						while(page->recycle_bin[i].head){
							pointer = page->recycle_bin[i].head;
							page->recycle_bin[i].head = page->recycle_bin[i].head->next;
							page->recycle_bin[i].memory_size -= pointer->size;
							free_pointer(pointer, page, pool);
						}
						__atom_unlock(page->lock);
					}
					__atom_unlock(page->recycle_bin[i].lock);
					break;
				}
			}
		}
	}
}


#ifdef ___SR_MALLOC_DEBUG___
void* sr_malloc(size_t size, const char *file_name, const char *function_name, int line_number)
#else //___SR_MALLOC_DEBUG___
void* sr_malloc(size_t size)
#endif //___SR_MALLOC_DEBUG___
{
	size_t reserved_size = 0;
	pointer_t *pointer = NULL;

	//获取当前线程的内存池
	sr_memory_pool_t *pool = (sr_memory_pool_t *)pthread_getspecific(mm->key);

	if (pool == NULL){

		//处理当前线程还没有创建内存池的情况
		//如果当前内存池数量未到达上限就为当前线程创建一个内存池
		while(mm->pool_number < __max_pool_number){
			size_t pool_id = mm->pool_number;
			if (__atom_try_lock(mm->pool[pool_id].lock)){
				if (mm->pool[pool_id].id == 0){
					mm->pool[pool_id].id = pool_id;
					pool = &(mm->pool[pool_id]);
					break;
				}
				__atom_unlock(mm->pool[pool_id].lock);
			}
		}

		if (mm->pool_number >= __max_pool_number){

			if (pool != NULL){
				__atom_unlock(pool->lock);
			}
			size_t pool_id = __atom_add(mm->thread_number, 1) % __max_pool_number;
			pool = &(mm->pool[pool_id]);

			if (pthread_setspecific(mm->key, pool) != 0){
				return NULL;
			}

		}else{

			pool->tid = pthread_self();
			for (size_t page_id = 0; page_id < mm->preloading_page; ++page_id){
				pool->page[page_id].id = page_id;
				if (assign_page(pool, &(pool->page[page_id]), mm->page_size) != 0){
					__atom_unlock(pool->lock);
					return NULL;
				}
				__atom_add(pool->page_number, 1);
			}

			__atom_add(mm->pool_number, 1);
			__atom_unlock(pool->lock);

			if (pthread_setspecific(mm->key, pool) != 0){
				return NULL;
			}
		}
	}


	size = request2allocation(size);

	//确保分配一个新的指针之后剩余的内存不会小于一个空闲指针的大小
	reserved_size = size + __free_pointer_size;


	for(size_t i = 0; i < pool->page_number; ++i){

		if (!__atom_try_lock(pool->page[i].lock)){
			continue;
		}

		if (reserved_size > pool->page[i].pointer->size){

			if (pool->page[i].head->next->size > reserved_size){

				//把当前指针加入到释放队列尾
				pool->page[i].pointer->next = pool->page[i].end;
				pool->page[i].pointer->prev = pool->page[i].end->prev;
				pool->page[i].pointer->next->prev = pool->page[i].pointer;
				pool->page[i].pointer->prev->next = pool->page[i].pointer;
				//设置当前指针为释放状态
				__next_pointer(pool->page[i].pointer)->flag = pool->page[i].pointer->size;
				//使用空闲队列中最大的指针作为当前指针
				pool->page[i].pointer = pool->page[i].head->next;
				//从空闲队列中移除
				pool->page[i].head->next = pool->page[i].head->next->next;
				pool->page[i].head->next->prev = pool->page[i].head;

			}else{

				__atom_unlock(pool->page[i].lock);
				continue;
			}
		}

		//分配一个新的指针
		pointer = __assign_pointer(pool->page[i].pointer, size);
		pointer->flag = ((pool->id << 11) | (i << 1) | __inuse);
		pointer->size = pool->page[i].pointer->size - size;
		__next_pointer(pointer)->flag = pointer->size;
		pointer->prev = pool->page[i].pointer;
		pool->page[i].pointer = pointer;
		pointer = pool->page[i].pointer->prev;
		pointer->size = size;

		__atom_unlock(pool->page[i].lock);

#ifdef ___SR_MALLOC_DEBUG___
		int len = snprintf(pointer->debug_msg, __debug_msg_size - 1, "%s [ %s(%d) ]", file_name, function_name, line_number);
		pointer->debug_msg[len] = '\0';
#endif

		return __pointer2address(pointer);
	}


	//创建一个分页
	sr_memory_page_t *page = NULL;

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
		return NULL;
	}

	if (size >= mm->page_size >> 2){
		if (size >= mm->page_size){
			if (assign_page(pool, page, size << 1) != 0){
				__atom_unlock(page->lock);
				return NULL;
			}
		}else{
			if (assign_page(pool, page, mm->page_size << 1) != 0){
				__atom_unlock(page->lock);
				return NULL;
			}
		}
	}else{
		if (assign_page(pool, page, mm->page_size) != 0){
			__atom_unlock(page->lock);
			return NULL;
		}
	}

	//分配一个新的指针
	pointer = __assign_pointer(page->pointer, size);
	pointer->flag = ((pool->id << 11) | (page->id << 1) | __inuse);
	pointer->size = page->pointer->size - size;
	__next_pointer(pointer)->flag = pointer->size;
	pointer->prev = page->pointer;
	page->pointer = pointer;
	pointer = page->pointer->prev;
	pointer->size = size;

	__atom_add(pool->page_number, 1);
	__atom_unlock(page->lock);

#ifdef ___SR_MALLOC_DEBUG___
	int len = snprintf(pointer->debug_msg, __debug_msg_size - 1, "%s [%s(%d)]", file_name, function_name, line_number);
	pointer->debug_msg[len] = '\0';
#endif

	return __pointer2address(pointer);
}


#ifdef ___SR_MALLOC_DEBUG___
void* sr_calloc(size_t number, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___SR_MALLOC_DEBUG___
void* sr_calloc(size_t number, size_t size)
#endif //___SR_MALLOC_DEBUG___
{
	size *= number;

#ifdef ___SR_MALLOC_DEBUG___
	void *pointer = sr_malloc(size, file_name, function_name, line_number);
#else //___SR_MALLOC_DEBUG___
	void *pointer = sr_malloc(size);
#endif //___SR_MALLOC_DEBUG___

	if (pointer != NULL){
    	memset(pointer, 0, size);
    	return pointer;
	}

	return NULL;
}



#ifdef ___SR_MALLOC_DEBUG___
void* sr_realloc(void *address, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___SR_MALLOC_DEBUG___
void* sr_realloc(void *address, size_t size)
#endif //___SR_MALLOC_DEBUG___
{
	void *new_address = NULL;
	pointer_t *old_pointer = __address2pointer(address);

	if (size > 0){

#ifdef ___SR_MALLOC_DEBUG___
		new_address = sr_malloc(size, file_name, function_name, line_number);
#else //___SR_MALLOC_DEBUG___
		new_address = sr_malloc(size);
#endif //___SR_MALLOC_DEBUG___

		if (new_address != NULL){

			if (address != NULL){
				if (size > old_pointer->size - __work_pointer_size){
					memcpy(new_address, address, old_pointer->size - __work_pointer_size);
				}else{
					memcpy(new_address, address, size);
				}
			}

			sr_free(address);
			return new_address;
		}

	}else{

		if (address != NULL){
			sr_free(address);
		}
	}

    return NULL;
}


#ifdef ___SR_MALLOC_DEBUG___
void* sr_aligned_alloc(size_t alignment, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___SR_MALLOC_DEBUG___
void* sr_aligned_alloc(size_t alignment, size_t size)
#endif //___SR_MALLOC_DEBUG___
{
	void *address = NULL, *aligned_address = NULL;
	pointer_t *pointer = NULL, *aligned_pointer = NULL;

	if (size == 0 || alignment == 0
			|| (alignment & (sizeof (void *) - 1)) != 0
			|| (alignment & (alignment - 1)) != 0){
		errno = EINVAL;
		return NULL;
	}


#ifdef ___SR_MALLOC_DEBUG___
	address = sr_malloc(size + alignment + __free_pointer_size, file_name, function_name, line_number);
#else //___SR_MALLOC_DEBUG___
	address = sr_malloc(size + alignment + __free_pointer_size);
#endif //___SR_MALLOC_DEBUG___


	if (address != NULL){
		if (((size_t)( address ) & ~(alignment - 1)) == ((size_t)( address ))){
			return address;
		}
		aligned_address = (void*)(((size_t)(address) + alignment + __free_pointer_size) & ~(alignment - 1));
		aligned_pointer = __address2pointer(aligned_address);
		pointer = __address2pointer(address);
		size_t free_size = (size_t)aligned_pointer - (size_t)pointer;

		aligned_pointer->size = pointer->size - free_size;
		aligned_pointer->flag = __next_pointer(pointer)->flag;
		pointer->size = free_size;

#ifdef ___SR_MALLOC_DEBUG___
		memcpy(aligned_pointer->debug_msg, pointer->debug_msg, __debug_msg_size);
#endif

		sr_free(address);
		return aligned_address;
	}

	errno = ENOMEM;

	return NULL;
}


#ifdef ___SR_MALLOC_DEBUG___
char* sr_strdup(const char *s, const char *file_name, const char *function_name, int line_number)
#else //___SR_MALLOC_DEBUG___
char* sr_strdup(const char *s)
#endif //___SR_MALLOC_DEBUG___
{
	char *result = NULL;
	if (s){
		size_t len = strlen(s);

#ifdef ___SR_MALLOC_DEBUG___
		result = (char *)sr_malloc(len + 1, file_name, function_name, line_number);
#else //___SR_MALLOC_DEBUG___
		result = (char *)sr_malloc(len + 1);
#endif //___SR_MALLOC_DEBUG___

		if (result != NULL){
		    memcpy(result, s, len);
		    result[len] = '\0';
		}
	}

	return result;
}
#endif //___SR_MALLOC___

int sr_malloc_initialize(size_t page_size, size_t preloaded_page)
{
#ifdef ___SR_MALLOC___
	if (__atom_try_lock(mm->lock)){

		size_t pool_id = mm->pool_number;
		mm->pool_number ++;

		if (page_size < 1024 << 11){
			page_size = 1024 << 11;
		}

		if (preloaded_page < 1){
			preloaded_page = 2;
		}

		if (pthread_key_create(&(mm->key), NULL) != 0){
			__atom_unlock(mm->lock);
			return -1;
		}

		mm->page_size = page_size;
		mm->preloading_page = preloaded_page < __max_page_number ? preloaded_page : __max_page_number;
		mm->page_aligned_mask = (size_t) sysconf(_SC_PAGESIZE) - 1;

		mm->pool[pool_id].id = pool_id;

		for (size_t page_id = 0; page_id < mm->preloading_page; ++page_id){
			mm->pool[pool_id].page[page_id].id = page_id;
			mm->pool[pool_id].page_number ++;
			if (assign_page(&(mm->pool[pool_id]), &(mm->pool[pool_id].page[page_id]), mm->page_size) != 0){
				return -1;
			}
		}

		if (pthread_setspecific(mm->key, &(mm->pool[pool_id])) != 0){
			__atom_unlock(mm->lock);
			return -1;
		}
	}
#endif //___SR_MALLOC___

	return 0;
}


void sr_malloc_release()
{
#ifdef ___SR_MALLOC___
	if (__atom_unlock(mm->lock)){
		for (int pool_id = 0; pool_id < mm->pool_number; ++pool_id){
			sr_memory_pool_t *pool = &(mm->pool[pool_id]);
			for (int page_id = 0; page_id < pool->page_number; ++page_id){
				if ( pool->page[page_id].start_address != NULL){
					munmap(pool->page[page_id].start_address, pool->page[page_id].size);
				}
			}
		}

		pthread_key_delete(mm->key);

		memset(mm, 0, sizeof(sr_memory_manager_t));
	}
#endif //___SR_MALLOC___
}


void sr_malloc_debug(void (*log_debug)(const char *format, ...))
{
#ifdef ___SR_MALLOC___

	sr_memory_pool_t *pool = NULL;
	pointer_t *pointer = NULL;

	if (log_debug == NULL){
		return;
	}

	flush_cache();

	for (size_t pool_id = 0; pool_id < mm->pool_number; ++pool_id){

		pool = &(mm->pool[pool_id]);

		for (size_t page_id = 0; page_id < pool->page_number; ++page_id){
			if (pool->page[page_id].start_address
				&& pool->page[page_id].pointer->size
				!= pool->page[page_id].size - __free_pointer_size * 2){
				log_debug("[!!! Found a memory leak !!!]\n");
				pointer = (__next_pointer(pool->page[page_id].head));
				while(pointer != pool->page[page_id].end){
#ifdef ___SR_MALLOC_DEBUG___
					if (!__mergeable(__next_pointer(pointer))){
						log_debug("[!!! Locate Memory Leaks !!! ===> %s]\n", pointer->debug_msg);
					}
#endif
					pointer = __next_pointer(pointer);
				}
			}
		}
	}
#endif //___SR_MALLOC___
}
