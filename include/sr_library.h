/*
 * sr_lib.h
 *
 *  Created on: 2018年12月7日
 *      Author: yongge
 */

#ifndef SR_LIBRARY_H_
#define SR_LIBRARY_H_

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

///////////////////////////////////////////////////////////////
////时间
///////////////////////////////////////////////////////////////

static inline int64_t sr_time_begin()
{
	struct timeval tv = {0};
	if (gettimeofday(&tv, NULL) != 0){
		assert(0);
	}
	return (1000000LL * tv.tv_sec + tv.tv_usec);
}

static inline int64_t sr_time_passed(int64_t begin_microsecond)
{
	struct timeval tv = {0};
	if (gettimeofday(&tv, NULL) != 0){
		assert(0);
	}
	return ((1000000LL * tv.tv_sec + tv.tv_usec) - begin_microsecond);
}

#define __sr_time_begin(begin) \
	do { \
		struct timeval tv = {0}; \
		if (gettimeofday(&tv, NULL) == 0) { \
			(begin) = 1000000LL * tv.tv_sec + tv.tv_usec; \
		} else { (begin) = 0; assert((begin)); } \
	}while(0)


#define __sr_time_passed(begin, passed) \
	do { \
		struct timeval tv = {0}; \
		if (gettimeofday(&tv, NULL) == 0) { \
			(passed) = (1000000LL * tv.tv_sec + tv.tv_usec) - (begin); \
		} else { (passed) = 0; assert((passed)); } \
	}while(0)

///////////////////////////////////////////////////////////////
////原子
///////////////////////////////////////////////////////////////

#define	__is_true(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	__is_false(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	__set_true(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	__set_false(x)		__sync_bool_compare_and_swap(&(x), true, false)

#define __sr_atom_sub(x, y)		__sync_sub_and_fetch(&(x), (y))
#define __sr_atom_add(x, y)		__sync_add_and_fetch(&(x), (y))
#define __sr_atom_lock(x)		while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define __sr_atom_trylock(x)	__set_true(x)
#define __sr_atom_unlock(x)		__set_false(x)


typedef union
{
	int32_t int32;
	float float32;
}if32_t;

typedef union
{
	int64_t int64;
	double float64;
}if64_t;

#define __sr_pop_int8(p, i) \
	(i) = (*(unsigned char *)(p)++)

#define __sr_pop_int16(p, i) \
	(i) \
	= (*(unsigned char *)(p)++ << 8) \
	| (*(unsigned char *)(p)++ << 0)

#define __sr_pop_int24(p, i) \
	(i) \
	= (*(unsigned char *)(p)++ << 16) \
	| (*(unsigned char *)(p)++ << 8) \
	| (*(unsigned char *)(p)++ << 0)

#define __sr_pop_int32(p, i) \
	(i) \
	= (*(unsigned char *)(p)++ << 24) \
	| (*(unsigned char *)(p)++ << 16) \
	| (*(unsigned char *)(p)++ << 8) \
	| (*(unsigned char *)(p)++ << 0)

#define __sr_pop_int64(p, i) \
	(i) \
	= ((int64_t)*(unsigned char *)(p)++ << 56) \
	| ((int64_t)*(unsigned char *)(p)++ << 48) \
	| ((int64_t)*(unsigned char *)(p)++ << 40) \
	| ((int64_t)*(unsigned char *)(p)++ << 32) \
	| ((int64_t)*(unsigned char *)(p)++ << 24) \
	| ((int64_t)*(unsigned char *)(p)++ << 16) \
	| ((int64_t)*(unsigned char *)(p)++ << 8) \
	| ((int64_t)*(unsigned char *)(p)++ << 0)


#define __sr_push_int8(p, i) \
	(*p++) = (unsigned char)(i)

#define __sr_push_int16(p, i) \
	*(p)++ = (unsigned char)((i) >> 8); \
	*(p)++ = (unsigned char)((i) >> 0)

#define __sr_push_int24(p, i) \
	*(p)++ = (unsigned char)((i) >> 16); \
	*(p)++ = (unsigned char)((i) >> 8); \
	*(p)++ = (unsigned char)((i) >> 0)

#define __sr_push_int32(p, i) \
	*(p)++ = (unsigned char)((i) >> 24); \
	*(p)++ = (unsigned char)((i) >> 16); \
	*(p)++ = (unsigned char)((i) >> 8); \
	*(p)++ = (unsigned char)((i) >> 0)

#define __sr_push_int64(p, i) \
	*(p)++ = (unsigned char)((int64_t)(i) >> 56); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 48); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 40); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 32); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 24); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 16); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 8); \
	*(p)++ = (unsigned char)((int64_t)(i) >> 0)

///////////////////////////////////////////////////////////////
////日志
///////////////////////////////////////////////////////////////

enum{
	SR_LOG_LEVEL_DEBUG = 0,
	SR_LOG_LEVEL_WARN,
	SR_LOG_LEVEL_ERROR
};

extern void sr_log_set_callback(void (*cb)(int level, const char *tag, const char *log));
extern void sr_log_warn(const char *fmt, ...);
extern void sr_log_debug(const char *path, const char *func, int line, const char *fmt, ...);
extern void sr_log_error(const char *path, const char *func, int line, const char *fmt, ...);

#ifdef logd
 #undef logd
#endif

#ifdef SR_LOG_DEBUG
#define logd(__FORMAT__, ...) \
	sr_log_debug(__FILE__, __FUNCTION__, __LINE__, __FORMAT__, ##__VA_ARGS__)
#else
#define logd(__FORMAT__, ...)	do {} while(0)
#endif

#ifdef loge
 #undef loge
#endif

#define loge(__FORMAT__, ...) \
	sr_log_error(__FILE__, __FUNCTION__, __LINE__, __FORMAT__, ##__VA_ARGS__)

#ifdef logw
 #undef logw
#endif

#define logw(__FORMAT__, ...) \
	sr_log_warn(__FORMAT__, ##__VA_ARGS__)

///////////////////////////////////////////////////////////////
////线程
///////////////////////////////////////////////////////////////

#include <pthread.h>

typedef struct sr_mutex_t sr_mutex_t;

extern sr_mutex_t* sr_mutex_create();
extern void sr_mutex_release(sr_mutex_t **pp_mutex);

extern void sr_mutex_lock(sr_mutex_t *mutex);
extern void sr_mutex_unlock(sr_mutex_t *mutex);

extern void sr_mutex_wait(sr_mutex_t *mutex);
extern void sr_mutex_signal(sr_mutex_t *mutex);
extern void sr_mutex_broadcast(sr_mutex_t *mutex);


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////

enum {
    QUEUE_RESULT_USER_INTERRUPT = -3,
    QUEUE_RESULT_TRY_AGAIN = -2,
    QUEUE_RESULT_ERROR = -1,
	QUEUE_RESULT_OK = 0
};

typedef struct sr_node_t{
    struct sr_node_t *prev;
    struct sr_node_t *next;
}sr_node_t;

typedef struct sr_queue_t sr_queue_t;

extern sr_queue_t* sr_queue_create(int max_node_number, void (*free_node_cb)(sr_node_t*));
extern void sr_queue_release(sr_queue_t **pp_queue);

extern int sr_queue_push_front(sr_queue_t *queue, sr_node_t *node);
extern int sr_queue_push_back(sr_queue_t *queue, sr_node_t *node);

extern int sr_queue_pop_front(sr_queue_t *queue, sr_node_t **pp_node);
extern int sr_queue_pop_back(sr_queue_t *queue, sr_node_t **pp_node);

extern int sr_queue_remove_node(sr_queue_t *queue, sr_node_t *node);

extern void sr_queue_clean(sr_queue_t *queue);
extern void sr_queue_stop(sr_queue_t *queue);
extern bool sr_queue_is_stopped(sr_queue_t *queue);
extern void sr_queue_finish(sr_queue_t *queue);

extern void sr_queue_lock(sr_queue_t *queue);
extern void sr_queue_unlock(sr_queue_t *queue);

extern sr_node_t* sr_queue_get_first(sr_queue_t *queue);
extern sr_node_t* sr_queue_get_last(sr_queue_t *queue);

extern int sr_queue_pushable(sr_queue_t *queue);
extern int sr_queue_popable(sr_queue_t *queue);

extern int sr_queue_block_push_front(sr_queue_t *queue, sr_node_t *node);
extern int sr_queue_block_push_back(sr_queue_t *queue, sr_node_t *node);

extern int sr_queue_block_pop_front(sr_queue_t *queue, sr_node_t **pp_node);
extern int sr_queue_block_pop_back(sr_queue_t *queue, sr_node_t **pp_node);

extern void sr_queue_block_clean(sr_queue_t *queue);


#define __sr_queue_push_front(queue, node) \
		sr_queue_push_front((queue), (sr_node_t*)(node))

#define __sr_queue_push_back(queue, node) \
		sr_queue_push_back((queue), (sr_node_t*)(node))

#define __sr_queue_pop_front(queue, node) \
		sr_queue_pop_front((queue), (sr_node_t**)(&(node)))

#define __sr_queue_pop_back(queue, node) \
		sr_queue_pop_back((queue), (sr_node_t**)(&(node)))

#define __sr_queue_remove_node(queue, node) \
		sr_queue_remove_node((queue), (sr_node_t*)(node))

#define __sr_queue_block_push_front(queue, node) \
		sr_queue_block_push_front((queue), (sr_node_t*)(node))

#define __sr_queue_block_push_back(queue, node) \
		sr_queue_block_push_back((queue), (sr_node_t*)(node))

#define __sr_queue_block_pop_front(queue, node) \
		sr_queue_block_pop_front((queue), (sr_node_t**)(&(node)))

#define __sr_queue_block_pop_back(queue, node) \
		sr_queue_block_pop_back((queue), (sr_node_t**)(&(node)))


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////


typedef struct sr_pipe_t sr_pipe_t;

extern sr_pipe_t* sr_pipe_create(unsigned int size);
extern void sr_pipe_release(sr_pipe_t **pp_pipe);

extern void sr_pipe_finish(sr_pipe_t *pipe);
extern void sr_pipe_stop(sr_pipe_t *pipe);
extern bool sr_pipe_is_stopped(sr_pipe_t *pipe);

extern void sr_pipe_clean(sr_pipe_t *pipe);
extern void sr_pipe_reset(sr_pipe_t *pipe);

extern unsigned int sr_pipe_writable(sr_pipe_t *pipe);
extern unsigned int sr_pipe_readable(sr_pipe_t *pipe);

extern unsigned int sr_pipe_read(sr_pipe_t *pipe, char *buf, unsigned int size);
extern unsigned int sr_pipe_write(sr_pipe_t *pipe, char *data, unsigned int size);

extern unsigned int sr_pipe_block_read(sr_pipe_t *pipe, char *buf, unsigned int size);
extern unsigned int sr_pipe_block_write(sr_pipe_t *pipe, char *data, unsigned int size);


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////

typedef struct sr_message_t{
	int type;
	if64_t if64;
	int extra_size;
	void *extra;
}sr_message_t;

typedef struct sr_messenger_callback_t{
	void *handler;
	void (*notify)(struct sr_messenger_callback_t *cb, sr_message_t msg);
}sr_messenger_callback_t;

typedef struct sr_messenger_t sr_messenger_t;

extern sr_messenger_t* sr_messenger_create(sr_messenger_callback_t *cb);
extern void sr_messenger_release(sr_messenger_t **pp_messenger);

extern void sr_messenger_finish(sr_messenger_t *messenger);
extern bool sr_messenger_is_arrive(sr_messenger_t *messenger);
extern int sr_messenger_receive(sr_messenger_t *messenger, sr_message_t *msg);
extern int sr_messenger_send(sr_messenger_t *messenger, sr_message_t msg);
extern int sr_messenger_notify(sr_messenger_t *messenger, int msg_type);


///////////////////////////////////////////////////////////////
////signal
///////////////////////////////////////////////////////////////

extern void sr_setup_crash_backtrace(void (*log_callback)(const char *format, ...));

#endif /* SR_LIBRARY_H_ */
