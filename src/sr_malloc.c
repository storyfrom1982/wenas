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


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/mman.h>
#include <errno.h>


#ifndef ___MEMORY_DEBUG___
# define DEBUG_MSG_SIZE			0
#else
# define DEBUG_MSG_SIZE			256
#endif

#define	MAX_POOL_NUMBER			1024

#define	MAX_PAGE_NUMBER			1024

#define	RECYCLE_BIN_NUMBER		2


#define	ALIGN_SIZE			( sizeof(size_t) )
#define	ALIGN_MASK			( ALIGN_SIZE - 1 )


#define	FREE_POINTER_SIZE		( DEBUG_MSG_SIZE + ALIGN_SIZE * 4 )

#define	WORK_POINTER_SIZE		( DEBUG_MSG_SIZE + ALIGN_SIZE * 2 )

#define	MIN_MEMORY_SIZE			( FREE_POINTER_SIZE - WORK_POINTER_SIZE )

#define	MIN_ALLOCATE_SIZE		( FREE_POINTER_SIZE )


#define	request2allocation(req) \
		( ( (req) + WORK_POINTER_SIZE < MIN_ALLOCATE_SIZE ) ? MIN_ALLOCATE_SIZE : \
				( (req) + WORK_POINTER_SIZE + ALIGN_MASK ) & ~ALIGN_MASK )



typedef struct Pointer {

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

#ifdef	___MEMORY_DEBUG___
	char debug_msg[DEBUG_MSG_SIZE];
#endif

	/**
	 * 指针队列索引
	 */

	struct Pointer *prev;
	struct Pointer *next;

}Pointer;


#define	pointer2address(p)		( (void *)((char *)( p ) + WORK_POINTER_SIZE ) )
#define	address2pointer(a)		( (Pointer *)( (char *)( a ) - WORK_POINTER_SIZE ) )

#define	INUSE				0x01

#define	mergeable(p)			( ! ( ( p )->flag & INUSE ) )

#define	prev_pointer(p)			( (Pointer *)( (char *)( p ) - ( p )->flag ) )
#define	next_pointer(p)			( (Pointer *)( (char *)( p ) + ( p )->size ) )
#define	assign_pointer(p, s)		( (Pointer *)( (char *)( p ) + ( s ) ) )


typedef struct Pointer_queue{
	bool lock;
	Pointer *head;
	size_t memory_size;
}Pointer_queue;


typedef struct Sr_memory_page{

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
	Pointer *pointer;

	/**
	 * 已释放但未合并的指针队列
	 */
	Pointer *head, *end;

	/**
	 * 为快速释放指针设计的回收池
	 */
	Pointer_queue recycle_bin[RECYCLE_BIN_NUMBER];

}Sr_memory_page;


typedef struct Sr_memory_pool{
	bool lock;
	size_t id;
	size_t page_number;
	void *aligned_address;
	pthread_t tid;
	Sr_memory_page page[MAX_PAGE_NUMBER + 1];
}Sr_memory_pool;


typedef struct Sr_memory_manager{

	bool lock;
	size_t page_size;
	size_t pool_number;
	size_t thread_number;
	size_t preloading_page;
	size_t page_aligned_mask;

	pthread_key_t key;
	Sr_memory_pool pool[MAX_POOL_NUMBER + 1];

}Sr_memory_manager;


static Sr_memory_manager memory_manager = {0}, *mm = &memory_manager;


#define	ISTRUE(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	ISFALSE(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	SETTRUE(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	SETFALSE(x)		__sync_bool_compare_and_swap(&(x), true, false)

#define ATOM_SUB(x, y)		__sync_sub_and_fetch(&(x), (y))
#define ATOM_ADD(x, y)		__sync_add_and_fetch(&(x), (y))

#define ATOM_LOCK(x)		while(!SETTRUE(x)) nanosleep((const struct timespec[]){{0, 10L}}, NULL)
#define ATOM_TRYLOCK(x)		SETTRUE(x)
#define ATOM_UNLOCK(x)		SETFALSE(x)



static int assign_page(Sr_memory_pool *pool, Sr_memory_page *page, size_t page_size)
{
	page->size = (page_size + mm->page_aligned_mask) & (~mm->page_aligned_mask);

	pool->aligned_address = NULL;

	do{
		page->start_address = mmap(pool->aligned_address,
				page->size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (page->start_address == MAP_FAILED){
			return -1;
		}else{
			if (((size_t)(page->start_address) & ALIGN_MASK) != 0){
				pool->aligned_address = (void *)(((size_t)(page->start_address) + ALIGN_MASK) & ~ALIGN_MASK);
				munmap(page->start_address, page->size);
			}else{
				pool->aligned_address = page->start_address + page->size;
				break;
			}
		}
	}while ( true );

	page->head = (Pointer*)(page->start_address);
	page->head->flag = ((pool->id << 11) | (page->id << 1) | INUSE);
	page->head->size = FREE_POINTER_SIZE;

	page->pointer = next_pointer(page->head);
	page->pointer->flag = ((pool->id << 11) | (page->id << 1) | INUSE);
	page->pointer->size = page->size - (FREE_POINTER_SIZE << 1);

	page->end = next_pointer(page->pointer);
	page->end->flag = page->pointer->size;
	page->end->size = FREE_POINTER_SIZE;

	page->head->prev = NULL;
	page->head->next = page->end;
	page->end->prev = page->head;
	page->end->next = NULL;

	memset(&(page->recycle_bin), 0, sizeof(page->recycle_bin));

	return 0;
}


static void free_page(Sr_memory_page *page, Sr_memory_pool *pool)
{
	//TODO 是否从最顶端的分页开始释放
	////当然最好能够回收空闲的内存，但是需要在线程退出时回收内存，需要封装线程来配合
	////这里现在什么也不做，不必担心什么，当前进程的内存池的容量会保持在峰值时的大小，不回收空闲的内存也没有关系
}


inline static void free_pointer(Pointer *pointer, Sr_memory_page *page, Sr_memory_pool *pool)
{

	if (next_pointer(pointer) == page->pointer){

		//合并左边指针
		if (mergeable(pointer)){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			prev_pointer(pointer)->size += pointer->size;
			pointer = prev_pointer(pointer);
			pointer->next->prev = pointer->prev;
			pointer->prev->next = pointer->next;
		}

		//更新空闲指针大小
		pointer->size += page->pointer->size;
		//合并指针
		next_pointer(pointer)->flag = pointer->size;
		page->pointer = pointer;

	}else if (next_pointer(page->pointer) == pointer){

		//合并右边指针
		if (mergeable(next_pointer(next_pointer(pointer)))){
			//如果指针已经释放就进行合并
			//把合并的指针从释放队列中移除
			next_pointer(pointer)->prev->next = next_pointer(pointer)->next;
			next_pointer(pointer)->next->prev = next_pointer(pointer)->prev;
			pointer->size += next_pointer(pointer)->size;
		}

		//合并指针
		page->pointer->size += pointer->size;
		next_pointer(page->pointer)->flag = page->pointer->size;

	}else{
		/**
		 * 不能与主指针合并的指针就先尝试与左右两边的指针合并
		 */
		if (mergeable(pointer)){
			prev_pointer(pointer)->size += pointer->size;
			pointer = prev_pointer(pointer);
			next_pointer(pointer)->flag = pointer->size;
			pointer->next->prev = pointer->prev;
			pointer->prev->next = pointer->next;
		}

		if (mergeable(next_pointer(next_pointer(pointer)))){
			next_pointer(pointer)->prev->next = next_pointer(pointer)->next;
			next_pointer(pointer)->next->prev = next_pointer(pointer)->prev;
			pointer->size += next_pointer(pointer)->size;
		}

		//设置释放状态
		next_pointer(pointer)->flag = pointer->size;
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

	if (page->pointer->size
			+ page->recycle_bin[0].memory_size
			+ page->recycle_bin[1].memory_size
			== mm->page_size
			- FREE_POINTER_SIZE
			- FREE_POINTER_SIZE){
		free_page(page, pool);
	}
}


static void flush_page(Sr_memory_page *page, Sr_memory_pool *pool)
{
	Pointer *pointer = NULL;
	for (size_t i = 0; i < RECYCLE_BIN_NUMBER; ++i){
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
	Pointer *pointer = NULL;
	Sr_memory_page *page = NULL;
	Sr_memory_pool *pool = NULL;

	if (address){

		pointer = address2pointer(address);
		pool_id = ((next_pointer(pointer)->flag >> 11) & 0x3FF);
		if ((pool_id >= mm->pool_number) || (pool_id != mm->pool[pool_id].id)){
			return;
		}
		pool = &(mm->pool[pool_id]);

		page_id = ((next_pointer(pointer)->flag >> 1) & 0x3FF);
		if ((page_id >= pool->page_number) || page_id != pool->page[page_id].id){
			return;
		}
		page = &(pool->page[page_id]);

		if (ATOM_TRYLOCK(page->lock)){
			free_pointer(pointer, page, pool);
			ATOM_UNLOCK(page->lock);
		}else{
			for (size_t i = 0; ; i = ++i % RECYCLE_BIN_NUMBER){
				if (ATOM_TRYLOCK(page->recycle_bin[i].lock)){
					pointer->next = page->recycle_bin[i].head;
					page->recycle_bin[i].head = pointer;
					page->recycle_bin[i].memory_size += pointer->size;
					if (ATOM_TRYLOCK(page->lock)){
						while(page->recycle_bin[i].head){
							pointer = page->recycle_bin[i].head;
							page->recycle_bin[i].head = page->recycle_bin[i].head->next;
							page->recycle_bin[i].memory_size -= pointer->size;
							free_pointer(pointer, page, pool);
						}
						ATOM_UNLOCK(page->lock);
					}
					ATOM_UNLOCK(page->recycle_bin[i].lock);
					break;
				}
			}
		}
	}
}


#ifdef ___MEMORY_DEBUG___
void* sr_malloc(size_t size, const char *file_name, const char *function_name, int line_number)
#else //___MEMORY_DEBUG___
void* sr_malloc(size_t size)
#endif //___MEMORY_DEBUG___
{
	size_t reserved_size = 0;
	Pointer *pointer = NULL;

	//获取当前线程的内存池
	Sr_memory_pool *pool = (Sr_memory_pool *)pthread_getspecific(mm->key);

	if (pool == NULL){

		//处理当前线程还没有创建内存池的情况
		//如果当前内存池数量未到达上限就为当前线程创建一个内存池
		while(mm->pool_number < MAX_POOL_NUMBER){
			size_t pool_id = mm->pool_number;
			if (ATOM_TRYLOCK(mm->pool[pool_id].lock)){
				if (mm->pool[pool_id].id == 0){
					mm->pool[pool_id].id = pool_id;
					pool = &(mm->pool[pool_id]);
					break;
				}
				ATOM_UNLOCK(mm->pool[pool_id].lock);
			}
		}

		if (mm->pool_number >= MAX_POOL_NUMBER){

			if (pool != NULL){
				ATOM_UNLOCK(pool->lock);
			}
			size_t pool_id = ATOM_ADD(mm->thread_number, 1) % MAX_POOL_NUMBER;
			pool = &(mm->pool[pool_id]);

			if (pthread_setspecific(mm->key, pool) != 0){
				return NULL;
			}

		}else{

			pool->tid = pthread_self();
			for (size_t page_id = 0; page_id < mm->preloading_page; ++page_id){
				pool->page[page_id].id = page_id;
				if (assign_page(pool, &(pool->page[page_id]), mm->page_size) != 0){
					ATOM_UNLOCK(pool->lock);
					return NULL;
				}
				ATOM_ADD(pool->page_number, 1);
			}

			ATOM_ADD(mm->pool_number, 1);
			ATOM_UNLOCK(pool->lock);

			if (pthread_setspecific(mm->key, pool) != 0){
				return NULL;
			}
		}
	}


	size = request2allocation(size);

	//确保分配一个新的指针之后剩余的内存不会小于一个空闲指针的大小
	reserved_size = size + FREE_POINTER_SIZE;


	for(size_t i = 0; i < pool->page_number; ++i){

		if (!ATOM_TRYLOCK(pool->page[i].lock)){
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
				next_pointer(pool->page[i].pointer)->flag = pool->page[i].pointer->size;
				//使用空闲队列中最大的指针作为当前指针
				pool->page[i].pointer = pool->page[i].head->next;
				//从空闲队列中移除
				pool->page[i].head->next = pool->page[i].head->next->next;
				pool->page[i].head->next->prev = pool->page[i].head;

			}else{

				ATOM_UNLOCK(pool->page[i].lock);
				continue;
			}
		}

		//分配一个新的指针
		pointer = assign_pointer(pool->page[i].pointer, size);
		pointer->flag = ((pool->id << 11) | (i << 1) | INUSE);
		pointer->size = pool->page[i].pointer->size - size;
		next_pointer(pointer)->flag = pointer->size;
		pointer->prev = pool->page[i].pointer;
		pool->page[i].pointer = pointer;
		pointer = pool->page[i].pointer->prev;
		pointer->size = size;

		ATOM_UNLOCK(pool->page[i].lock);

#ifdef ___MEMORY_DEBUG___
		int len = snprintf(pointer->debug_msg, DEBUG_MSG_SIZE - 1, "%s[%s(%d)]", file_name, function_name, line_number);
		pointer->debug_msg[len] = '\0';
#endif

		return pointer2address(pointer);
	}


	//创建一个分页
	Sr_memory_page *page = NULL;

	while(pool->page_number < MAX_PAGE_NUMBER){
		size_t page_id = pool->page_number;
		if (ATOM_TRYLOCK(pool->page[page_id].lock)){
			if (pool->page[page_id].id == 0){
				pool->page[page_id].id = page_id;
				page = &(pool->page[page_id]);
				break;
			}
			ATOM_UNLOCK(pool->page[page_id].lock);
		}
	}

	if (pool->page_number >= MAX_PAGE_NUMBER){
		ATOM_UNLOCK(page->lock);
		return NULL;
	}

	if (size >= mm->page_size >> 2){
		if (size >= mm->page_size){
			if (assign_page(pool, page, size << 1) != 0){
				ATOM_UNLOCK(page->lock);
				return NULL;
			}
		}else{
			if (assign_page(pool, page, mm->page_size << 1) != 0){
				ATOM_UNLOCK(page->lock);
				return NULL;
			}
		}
	}else{
		if (assign_page(pool, page, mm->page_size) != 0){
			ATOM_UNLOCK(page->lock);
			return NULL;
		}
	}

	//分配一个新的指针
	pointer = assign_pointer(page->pointer, size);
	pointer->flag = ((pool->id << 11) | (page->id << 1) | INUSE);
	pointer->size = page->pointer->size - size;
	next_pointer(pointer)->flag = pointer->size;
	pointer->prev = page->pointer;
	page->pointer = pointer;
	pointer = page->pointer->prev;
	pointer->size = size;

	ATOM_ADD(pool->page_number, 1);
	ATOM_UNLOCK(page->lock);

#ifdef ___MEMORY_DEBUG___
	int len = snprintf(pointer->debug_msg, DEBUG_MSG_SIZE - 1, "%s[%s(%d)]", file_name, function_name, line_number);
	pointer->debug_msg[len] = '\0';
#endif

	return pointer2address(pointer);
}


#ifdef ___MEMORY_DEBUG___
void* sr_calloc(size_t number, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___MEMORY_DEBUG___
void* sr_calloc(size_t number, size_t size)
#endif //___MEMORY_DEBUG___
{
	size *= number;

#ifdef ___MEMORY_DEBUG___
	void *pointer = sr_malloc(size, file_name, function_name, line_number);
#else //___MEMORY_DEBUG___
	void *pointer = sr_malloc(size);
#endif //___MEMORY_DEBUG___

	if (pointer != NULL){
    	memset(pointer, 0, size);
    	return pointer;
	}

	return NULL;
}



#ifdef ___MEMORY_DEBUG___
void* sr_realloc(void *address, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___MEMORY_DEBUG___
void* sr_realloc(void *address, size_t size)
#endif //___MEMORY_DEBUG___
{
	void *new_address = NULL;
	Pointer *old_pointer = address2pointer(address);

	if (size > 0){

#ifdef ___MEMORY_DEBUG___
		new_address = sr_malloc(size, file_name, function_name, line_number);
#else //___MEMORY_DEBUG___
		new_address = sr_malloc(size);
#endif //___MEMORY_DEBUG___

		if (new_address != NULL){

			if (address != NULL){
				if (size > old_pointer->size - WORK_POINTER_SIZE){
					memcpy(new_address, address, old_pointer->size - WORK_POINTER_SIZE);
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


#ifdef ___MEMORY_DEBUG___
void* sr_aligned_alloc(size_t alignment, size_t size, const char *file_name, const char *function_name, int line_number)
#else //___MEMORY_DEBUG___
void* sr_aligned_alloc(size_t alignment, size_t size)
#endif //___MEMORY_DEBUG___
{
	void *address = NULL, *aligned_address = NULL;
	Pointer *pointer = NULL, *aligned_pointer = NULL;

	if (size == 0 || alignment == 0
			|| (alignment & (sizeof (void *) - 1)) != 0
			|| (alignment & (alignment - 1)) != 0){
		errno = EINVAL;
		return NULL;
	}


#ifdef ___MEMORY_DEBUG___
	address = sr_malloc(size + alignment + FREE_POINTER_SIZE, file_name, function_name, line_number);
#else //___MEMORY_DEBUG___
	address = sr_malloc(size + alignment + FREE_POINTER_SIZE);
#endif //___MEMORY_DEBUG___


	if (address != NULL){
		if (((size_t)( address ) & ~(alignment - 1)) == ((size_t)( address ))){
			return address;
		}
		aligned_address = (void*)(((size_t)(address) + alignment + FREE_POINTER_SIZE) & ~(alignment - 1));
		aligned_pointer = address2pointer(aligned_address);
		pointer = address2pointer(address);
		size_t free_size = (size_t)aligned_pointer - (size_t)pointer;

		aligned_pointer->size = pointer->size - free_size;
		aligned_pointer->flag = next_pointer(pointer)->flag;
		pointer->size = free_size;

#ifdef ___MEMORY_DEBUG___
		memcpy(aligned_pointer->debug_msg, pointer->debug_msg, DEBUG_MSG_SIZE);
#endif

		sr_free(address);
		return aligned_address;
	}

	errno = ENOMEM;

	return NULL;
}


#ifdef ___MEMORY_DEBUG___
char* sr_strdup(const char *s, const char *file_name, const char *function_name, int line_number)
#else //___MEMORY_DEBUG___
char* sr_strdup(const char *s)
#endif //___MEMORY_DEBUG___
{
	char *result = NULL;
	if (s){
		size_t len = strlen(s);

#ifdef ___MEMORY_DEBUG___
		result = (char *)sr_malloc(len + 1, file_name, function_name, line_number);
#else //___MEMORY_DEBUG___
		result = (char *)sr_malloc(len + 1);
#endif //___MEMORY_DEBUG___

		if (result != NULL){
		    memcpy(result, s, len);
		    result[len] = '\0';
		}
	}

	return result;
}


int sr_malloc_initialize(size_t page_size, size_t preloaded_page)
{
	if (ATOM_TRYLOCK(mm->lock)){

		size_t pool_id = mm->pool_number;
		mm->pool_number ++;

		if (page_size < 1024 << 11){
			page_size = 1024 << 11;
		}

		if (preloaded_page < 1){
			preloaded_page = 2;
		}

		if (pthread_key_create(&(mm->key), NULL) != 0){
			ATOM_UNLOCK(mm->lock);
			return -1;
		}

		mm->page_size = page_size;
		mm->preloading_page = preloaded_page < MAX_PAGE_NUMBER ? preloaded_page : MAX_PAGE_NUMBER;
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
			ATOM_UNLOCK(mm->lock);
			return -1;
		}
	}

	return 0;
}


void sr_malloc_release()
{
	if (ATOM_UNLOCK(mm->lock)){
		for (int pool_id = 0; pool_id < mm->pool_number; ++pool_id){
			Sr_memory_pool *pool = &(mm->pool[pool_id]);
			for (int page_id = 0; page_id < pool->page_number; ++page_id){
				if ( pool->page[page_id].start_address != NULL){
					munmap(pool->page[page_id].start_address, pool->page[page_id].size);
				}
			}
		}

		pthread_key_delete(mm->key);

		memset(mm, 0, sizeof(Sr_memory_manager));
	}
}


void sr_malloc_debug(void (*log_debug)(const char *format, ...))
{
	Sr_memory_pool *pool = NULL;
	Pointer *pointer = NULL;

	if (log_debug == NULL){
		return;
	}

	flush_cache();

	for (size_t pool_id = 0; pool_id < mm->pool_number; ++pool_id){

		pool = &(mm->pool[pool_id]);

		for (size_t page_id = 0; page_id < pool->page_number; ++page_id){
			if (pool->page[page_id].start_address
				&& pool->page[page_id].pointer->size
				!= pool->page[page_id].size - FREE_POINTER_SIZE * 2){
				log_debug("[!!! Found a memory leak !!!]\n");
				pointer = (next_pointer(pool->page[page_id].head));
				while(pointer != pool->page[page_id].end){
#ifdef ___MEMORY_DEBUG___
					if (!mergeable(next_pointer(pointer))){
						log_debug("[!!! Locate Memory Leaks !!! ===> %s]\n", pointer->debug_msg);
					}
#endif
					pointer = next_pointer(pointer);
				}
			}
		}
	}
}
