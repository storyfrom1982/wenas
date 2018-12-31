/*
 * sr_lib.c
 *
 *  Created on: 2018年12月7日
 *      Author: yongge
 */


#include "sr_library.h"
#include "sr_malloc.h"


///////////////////////////////////////////////////////////////
////日志
///////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define __log_date_size			32
#define __log_text_size			512


#define __path_clear(path) \
	( strrchr( path, '/' ) ? strrchr( path, '/' ) + 1 : path )



static void log_print(int level, const char *tag, const char *log);

typedef void (*log_callback) (int level, const char *tag, const char *log);

static log_callback g_log_callback = log_print;



static size_t date2string(char *str, size_t len)
{
	time_t current = {0};
	time(&current);
	struct tm *fmt = localtime(&current);
	return strftime(str, len, "%F:%H:%M:%S", fmt);
}


void sr_log_set_callback(void (*cb)(int level, const char *tag, const char *log))
{
	if (cb){
		g_log_callback = cb;
	}
}


void sr_log_warn(const char *fmt, ...)
{
    time_t current = {0};
    time(&current);
    char text[__log_text_size] = {0};
    size_t n = strftime(text, __log_text_size, "%F:%H:%M:%S ", localtime(&current));
    va_list args;
    va_start (args, fmt);
    vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);
    g_log_callback(SR_LOG_LEVEL_WARN, "WARNING", text);
}


void sr_log_debug(const char *path, const char *func, int line, const char *fmt, ...)
{
    time_t current = {0};
    time(&current);
    char text[__log_text_size] = {0};
    size_t n = strftime(text, __log_text_size, "%F:%H:%M:%S", localtime(&current));
    n += snprintf(text + n, __log_text_size - n, " %s(%d): ", func, line);
    va_list args;
    va_start (args, fmt);
    vsnprintf(text + n, __log_text_size - n, fmt, args);
    va_end (args);
	g_log_callback(SR_LOG_LEVEL_DEBUG, __path_clear(path), text);
}


void sr_log_error(const char *path, const char *func, int line, const char *fmt, ...)
{
    time_t current = {0};
    time(&current);
    char text[__log_text_size] = {0};
    size_t n = strftime(text, __log_text_size, "%F:%H:%M:%S", localtime(&current));
	n += snprintf(text + n, __log_text_size - n, " %s(%d){%s}: ", func, line, strerror(errno));
	va_list args;
	va_start (args, fmt);
	vsnprintf(text + n, __log_text_size - n, fmt, args);
	va_end (args);
	g_log_callback(SR_LOG_LEVEL_ERROR, __path_clear(path), text);
	assert(0);
}


static void log_print(int level, const char *tag, const char *log)
{
	if (level == SR_LOG_LEVEL_DEBUG){
		fprintf(stdout, "DEBUG: [%s] %s", tag, log);
	}else if (level == SR_LOG_LEVEL_WARN){
		fprintf(stdout, "WARNING: [%s] %s", tag, log);
	}else if (level == SR_LOG_LEVEL_ERROR){
		fprintf(stderr, "ERROR: [%s] %s", tag, log);
	}
}


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////


struct sr_mutex_t{
	pthread_cond_t cond[1];
	pthread_mutex_t mutex[1];
};


sr_mutex_t* sr_mutex_create()
{
	sr_mutex_t *mutex = NULL;

	if ((mutex = malloc(sizeof(sr_mutex_t))) == NULL){
		loge("malloc failed\n");
		return NULL;
	}

	if (pthread_cond_init(mutex->cond, NULL) != 0){
		loge("pthread_cond_init failed\n");
		free(mutex);
		return NULL;
	}

	if (pthread_mutex_init(mutex->mutex, NULL) != 0){
		pthread_cond_destroy(mutex->cond);
		free(mutex);
		loge("pthread_mutex_init failed\n");
		return NULL;
	}

	return mutex;
}

void sr_mutex_release(sr_mutex_t **pp_mutex)
{
	if (pp_mutex && *pp_mutex){
		sr_mutex_t *mutex = *pp_mutex;
		*pp_mutex = NULL;
		pthread_cond_destroy(mutex->cond);
		pthread_mutex_destroy(mutex->mutex);
		free(mutex);
	}
}

inline void sr_mutex_lock(sr_mutex_t *mutex)
{
	if (mutex){
		if (pthread_mutex_lock(mutex->mutex) != 0){
			loge("pthread_mutex_lock failed\n");
		}
	}else{
		loge("null parameter\n");
	}
}

inline void sr_mutex_unlock(sr_mutex_t *mutex)
{
	if (mutex){
		if (pthread_mutex_unlock(mutex->mutex) != 0){
            loge("pthread_mutex_unlock failed\n");
		}
	}else{
		loge("null parameter\n");
	}
}

inline void sr_mutex_wait(sr_mutex_t *mutex)
{
	if (mutex){
		if (pthread_cond_wait(mutex->cond, mutex->mutex) != 0){
			loge("pthread_cond_wait failed\n");
		}
	}else{
		loge("null parameter\n");
	}
}

inline void sr_mutex_signal(sr_mutex_t *mutex)
{
	if (mutex){
		if (pthread_cond_signal(mutex->cond) != 0){
			loge("pthread_cond_signal failed\n");
		}
	}else{
		loge("null parameter\n");
	}
}

inline void sr_mutex_broadcast(sr_mutex_t *mutex)
{
	if (mutex){
		if (pthread_cond_broadcast(mutex->cond) != 0){
			loge("pthread_cond_broadcast failed\n");
		}
	}else{
		loge("null parameter\n");
	}
}


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////


struct sr_queue_t {
	//non-blocking
    bool lock;
    bool stopped;
    int size;
    int pushable;
    int popable;
    void (*free_node_cb)(sr_node_t*);
    sr_node_t head;
    sr_node_t end;
    //blocking
    int push_waiting;
    int pop_waiting;
    sr_mutex_t *mutex;
};


sr_queue_t* sr_queue_create(int max_node_number, void (*free_node_cb)(sr_node_t*))
{
    sr_queue_t *queue = (sr_queue_t*)calloc(1, sizeof(sr_queue_t));
    if (queue == NULL){
        loge("calloc failed\n");
        return NULL;
    }

    if ((queue->mutex = sr_mutex_create()) == NULL){
        loge("sr_mutex_create failed\n");
        free(queue);
        return NULL;
    }

    queue->push_waiting = 0;
    queue->pop_waiting = 0;

    queue->lock = false;
    queue->stopped = false;
    queue->size = max_node_number;
    queue->pushable = max_node_number;
    queue->popable = 0;
    queue->head.prev = NULL;
    queue->head.next = &(queue->end);
    queue->end.next = NULL;
    queue->end.prev = &(queue->head);
    queue->free_node_cb = free_node_cb;

    return queue;
}

void sr_queue_release(sr_queue_t **pp_queue)
{
    if (pp_queue && *pp_queue){
        sr_queue_t *queue = *pp_queue;
        *pp_queue = NULL;
        if (__is_false(queue->stopped)
        		|| queue->popable > 0
        		|| queue->pop_waiting > 0
        		|| queue->push_waiting > 0 ){
        	sr_queue_stop(queue);
        }
        sr_mutex_release(&queue->mutex);
        free(queue);
    }
}

int sr_queue_push_front(sr_queue_t *queue, sr_node_t *node)
{
    if (NULL == queue || NULL == node){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    if (__is_true(queue->stopped)){
        return QUEUE_RESULT_USER_INTERRUPT;
    }

    if (!__sr_atom_trylock(queue->lock)){
        return QUEUE_RESULT_TRY_AGAIN;
    }

    if (queue->pushable == 0){
        __sr_atom_unlock(queue->lock);
        return QUEUE_RESULT_TRY_AGAIN;
    }

    node->prev = queue->end.prev;
    node->prev->next = node;
    queue->end.prev = node;
    node->next = &(queue->end);

    node->next = queue->head.next;
    node->next->prev = node;
    node->prev = &(queue->head);
    queue->head.next = node;

    queue->popable++;
    queue->pushable--;

    __sr_atom_unlock(queue->lock);

    return QUEUE_RESULT_OK;
}

int sr_queue_push_back(sr_queue_t *queue, sr_node_t *node)
{
    if (NULL == queue || NULL == node){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    if (__is_true(queue->stopped)){
        return QUEUE_RESULT_USER_INTERRUPT;
    }

    if (!__sr_atom_trylock(queue->lock)){
        return QUEUE_RESULT_TRY_AGAIN;
    }

    if (queue->pushable == 0){
        __sr_atom_unlock(queue->lock);
        return QUEUE_RESULT_TRY_AGAIN;
    }

    node->prev = queue->end.prev;
    node->prev->next = node;
    queue->end.prev = node;
    node->next = &(queue->end);

    queue->popable++;
    queue->pushable--;

    __sr_atom_unlock(queue->lock);

    return QUEUE_RESULT_OK;
}

int sr_queue_pop_front(sr_queue_t *queue, sr_node_t **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    if (!__sr_atom_trylock(queue->lock)){
        return QUEUE_RESULT_TRY_AGAIN;
    }

    if (queue->popable == 0){
        __sr_atom_unlock(queue->lock);
        if (__is_true(queue->stopped)){
            return QUEUE_RESULT_USER_INTERRUPT;
        }
        return QUEUE_RESULT_TRY_AGAIN;
    }

    (*pp_node) = queue->head.next;
    queue->head.next = (*pp_node)->next;
    (*pp_node)->next->prev = &(queue->head);

    queue->pushable++;
    queue->popable--;

    __sr_atom_unlock(queue->lock);

    return QUEUE_RESULT_OK;
}

int sr_queue_pop_back(sr_queue_t *queue, sr_node_t **pp_node)
{
    if (NULL == queue || NULL == pp_node){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    if (!__sr_atom_trylock(queue->lock)){
        return QUEUE_RESULT_TRY_AGAIN;
    }
    if (queue->popable == 0){
        __sr_atom_unlock(queue->lock);
        if (__is_true(queue->stopped)){
            return QUEUE_RESULT_USER_INTERRUPT;
        }
        return QUEUE_RESULT_TRY_AGAIN;
    }

    (*pp_node) = queue->end.prev;
    queue->end.prev = (*pp_node)->prev;
    (*pp_node)->prev->next = &(queue->end);

    queue->pushable++;
    queue->popable--;

    __sr_atom_unlock(queue->lock);

    return QUEUE_RESULT_OK;
}

int sr_queue_remove_node(sr_queue_t *queue, sr_node_t *node)
{
    if (NULL == queue || NULL == node
    		|| NULL == node->prev || NULL == node->next){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node->next = NULL;

    queue->pushable++;
    queue->popable--;

    return QUEUE_RESULT_OK;
}

void sr_queue_lock(sr_queue_t *queue)
{
	if (queue != NULL){
		sr_mutex_lock(queue->mutex);
		__sr_atom_lock(queue->lock);
	}else{
        loge("null parameter\n");
    }
}

void sr_queue_unlock(sr_queue_t *queue)
{
	if (queue != NULL){
		__sr_atom_unlock(queue->lock);
		sr_mutex_unlock(queue->mutex);
	}else{
        loge("null parameter\n");
    }
}

sr_node_t* sr_queue_get_first(sr_queue_t *queue)
{
	if (queue == NULL){
		loge("null parameter\n");
		return NULL;
	}
	return queue->head.next;
}

sr_node_t* sr_queue_get_last(sr_queue_t *queue)
{
	if (queue == NULL){
		loge("null parameter\n");
		return NULL;
	}
	return queue->end.prev;
}

int sr_queue_pushable(sr_queue_t *queue)
{
    if (NULL == queue){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }
    return queue->pushable;
}

int sr_queue_popable(sr_queue_t *queue)
{
    if (NULL == queue){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }
    return queue->popable;
}

void sr_queue_clean(sr_queue_t *q)
{
	while ((q)->head.next != &((q)->end)){
		(q)->head.prev = (q)->head.next;
		(q)->head.next = (q)->head.next->next;
		(q)->popable--;
		if ((q)->free_node_cb){
            (q)->free_node_cb((q)->head.prev);
        } else{
            free((q)->head.prev);
        }
	}

	assert((q)->popable == 0);
	(q)->head.prev = NULL;
	(q)->head.next = &((q)->end);
	(q)->end.next = NULL;
	(q)->end.prev = &((q)->head);
	(q)->pushable = (q)->size;
}


bool sr_queue_is_stopped(sr_queue_t *queue)
{
	if (queue){
		return __is_true(queue->stopped);
	}
	return true;
}


void sr_queue_stop(sr_queue_t *queue)
{
    if (queue){
    	if (__set_true(queue->stopped)){
        	sr_queue_lock(queue);
        	sr_queue_clean(queue);
            sr_mutex_broadcast(queue->mutex);
            sr_queue_unlock(queue);
            while (queue->pop_waiting > 0 || queue->push_waiting > 0){
                nanosleep((const struct timespec[]){{0, 10000L}}, NULL);
                sr_mutex_lock(queue->mutex);
                sr_mutex_broadcast(queue->mutex);
                sr_mutex_unlock(queue->mutex);
            }
    	}
    }else{
        loge("null parameter\n");
    }
}


void sr_queue_finish(sr_queue_t *queue)
{
    if (queue){
    	__set_true(queue->stopped);
    }else{
        loge("null parameter\n");
    }
}


int sr_queue_block_push_front(sr_queue_t *queue, sr_node_t *node)
{
    int result = 0;

    if (queue == NULL || node == NULL){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    sr_mutex_lock(queue->mutex);

    while ((result = sr_queue_push_front(queue, node)) == QUEUE_RESULT_TRY_AGAIN){

        if (sr_queue_pushable(queue) == 0){
            if (__is_false(queue->stopped)){
                __sr_atom_add(queue->push_waiting, 1);
                sr_mutex_wait(queue->mutex);
                __sr_atom_sub(queue->push_waiting, 1);
            }
            if (__is_true(queue->stopped)){
            	sr_mutex_unlock(queue->mutex);
                return QUEUE_RESULT_USER_INTERRUPT;
            }
        }
    }

    if (result == 0){
        if (queue->pop_waiting > 0){
            sr_mutex_broadcast(queue->mutex);
        }
    }

    sr_mutex_unlock(queue->mutex);

    return result;
}


int sr_queue_block_push_back(sr_queue_t *queue, sr_node_t *node)
{
    int result = 0;

    if (queue == NULL || node == NULL){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    sr_mutex_lock(queue->mutex);

    while ((result = sr_queue_push_back(queue, node)) == QUEUE_RESULT_TRY_AGAIN){

        if (sr_queue_pushable(queue) == 0){
            if (__is_false(queue->stopped)){
                __sr_atom_add(queue->push_waiting, 1);
                sr_mutex_wait(queue->mutex);
                __sr_atom_sub(queue->push_waiting, 1);
            }
            if (__is_true(queue->stopped)){
            	sr_mutex_unlock(queue->mutex);
                return QUEUE_RESULT_USER_INTERRUPT;
            }
        }
    }

    if (result == 0){
        if (queue->pop_waiting > 0){
            sr_mutex_broadcast(queue->mutex);
        }
    }

    sr_mutex_unlock(queue->mutex);

    return result;
}


int sr_queue_block_pop_front(sr_queue_t *queue, sr_node_t **pp_node)
{
    int result = 0;

    if (queue == NULL || pp_node == NULL){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    sr_mutex_lock(queue->mutex);

    while ((result = sr_queue_pop_front(queue, pp_node)) == QUEUE_RESULT_TRY_AGAIN){

        if (sr_queue_popable(queue) == 0){
            if (__is_false(queue->stopped)){
                __sr_atom_add(queue->pop_waiting, 1);
                sr_mutex_wait(queue->mutex);
                __sr_atom_sub(queue->pop_waiting, 1);
            }
            if (__is_true(queue->stopped)){
            	sr_mutex_unlock(queue->mutex);
                return QUEUE_RESULT_USER_INTERRUPT;
            }
        }
    }

    if (result == 0){
        if (queue->push_waiting > 0){
            sr_mutex_broadcast(queue->mutex);
        }
    }

    sr_mutex_unlock(queue->mutex);

    return result;
}


int sr_queue_block_pop_back(sr_queue_t *queue, sr_node_t **pp_node)
{
    int result = 0;

    if (queue == NULL || pp_node == NULL){
        loge("null parameter\n");
        return QUEUE_RESULT_ERROR;
    }

    sr_mutex_lock(queue->mutex);

    while ((result = sr_queue_pop_back(queue, pp_node)) == QUEUE_RESULT_TRY_AGAIN){

        if (sr_queue_popable(queue) == 0){
            if (__is_false(queue->stopped)){
                __sr_atom_add(queue->pop_waiting, 1);
                sr_mutex_wait(queue->mutex);
                __sr_atom_sub(queue->pop_waiting, 1);
            }
            if (__is_true(queue->stopped)){
            	sr_mutex_unlock(queue->mutex);
                return QUEUE_RESULT_USER_INTERRUPT;
            }
        }
    }

    if (result == 0){
        if (queue->push_waiting > 0){
            sr_mutex_broadcast(queue->mutex);
        }
    }

    sr_mutex_unlock(queue->mutex);

    return result;
}


void sr_queue_block_clean(sr_queue_t *queue)
{
    if (queue){
    	sr_queue_lock(queue);
    	sr_queue_clean(queue);
        sr_queue_unlock(queue);
    }else{
        loge("null parameter\n");
    }
}


///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////

struct sr_pipe_t {
	//non-blocking
	bool stopped;
	uint8_t *buf;
	unsigned int len;
	unsigned int writer;
	unsigned int reader;
    //blocking
    int write_waiting;
    int read_waiting;
    sr_mutex_t *mutex;
};



sr_pipe_t* sr_pipe_create(unsigned int len)
{
	sr_pipe_t *pipe = NULL;

	if (len == 0 ){
		loge("null parameter\n");
		return NULL;
	}

	if ((pipe = (sr_pipe_t *)calloc(1, sizeof(sr_pipe_t))) == NULL){
		loge("calloc failed\n");
		return NULL;
	}

	if ( ( len & ( len - 1 ) ) == 0 ){
		pipe->len = len;
	}else{
		pipe->len = 2;
		while(len >>= 1){
			pipe->len <<= 1;
		}
	}

	if ((pipe->buf = ( uint8_t * )malloc( pipe->len )) == NULL){
		loge("malloc failed\n");
		sr_pipe_release(&pipe);
		free(pipe);
		return NULL;
	}

	if ((pipe->mutex = sr_mutex_create()) == NULL){
        loge("sr_mutex_create failed\n");
        free(pipe->buf);
        free(pipe);
        return NULL;
    }

    pipe->stopped = false;
	pipe->writer = pipe->reader = 0;
	pipe->read_waiting = pipe->write_waiting = 0;

	return pipe;
}


void sr_pipe_release(sr_pipe_t **pp_pipe)
{
	if (pp_pipe && *pp_pipe){
		sr_pipe_t *pipe = *pp_pipe;
		*pp_pipe = NULL;
		sr_pipe_stop(pipe);
		sr_mutex_release(&pipe->mutex);
		free(pipe->buf);
		free(pipe);
	}
}


void sr_pipe_stop(sr_pipe_t *pipe)
{
	if (pipe != NULL){
		__set_true(pipe->stopped);
		sr_pipe_clean(pipe);
		sr_mutex_lock(pipe->mutex);
		sr_mutex_broadcast(pipe->mutex);
		sr_mutex_unlock(pipe->mutex);
		while(!__is_false(pipe->write_waiting) || !__is_false(pipe->read_waiting)){
			nanosleep((const struct timespec[]){{0, 10000L}}, NULL);
			sr_mutex_lock(pipe->mutex);
			sr_mutex_broadcast(pipe->mutex);
			sr_mutex_unlock(pipe->mutex);
		}
	}else{
		loge("null parameter\n");
	}
}


void sr_pipe_finish(sr_pipe_t *pipe)
{
	if (pipe != NULL){
		__set_true(pipe->stopped);
	}else{
		loge("null parameter\n");
	}
}


bool sr_pipe_is_stopped(sr_pipe_t *pipe)
{
    if (pipe != NULL){
        return pipe->stopped;
    }
    return true;
}


void sr_pipe_clean(sr_pipe_t *pipe)
{
	if (pipe){
		pipe->writer = pipe->reader = 0;
	}else{
		loge("null parameter\n");
	}
}


void sr_pipe_reset(sr_pipe_t *pipe)
{
    if (pipe != NULL){
        sr_pipe_clean(pipe);
        __set_false(pipe->stopped);
    }else{
		loge("null parameter\n");
	}
}


unsigned int sr_pipe_writable(sr_pipe_t *pipe)
{
	if (pipe){
		return pipe->len - pipe->writer + pipe->reader;
	}
	return 0;
}


unsigned int sr_pipe_readable(sr_pipe_t *pipe)
{
	if (pipe){
		return pipe->writer - pipe->reader;
	}
	return 0;
}


unsigned int sr_pipe_write(sr_pipe_t *pipe, char *data, unsigned int size)
{
	if (pipe == NULL || data == NULL || size == 0){
		loge("null parameter\n");
		return 0;
	}

	unsigned int writable = pipe->len - pipe->writer + pipe->reader;
	unsigned int remain = pipe->len - ( pipe->writer & ( pipe->len - 1 ) );

	if ( writable == 0 ){
		return 0;
	}

	size = writable < size ? writable : size;

	if ( remain >= size ){
		memcpy( pipe->buf + ( pipe->writer & ( pipe->len - 1 ) ), data, size);
	}else{
		memcpy( pipe->buf + ( pipe->writer & ( pipe->len - 1 ) ), data, remain);
		memcpy( pipe->buf, data + remain, size - remain);
	}

	__sr_atom_add( pipe->writer, size );

	return size;
}


unsigned int sr_pipe_read(sr_pipe_t *pipe, char *buffer, unsigned int size)
{
	if (pipe == NULL || buffer == NULL || size == 0){
		loge("null parameter\n");
		return 0;
	}

	unsigned int readable = pipe->writer - pipe->reader;
	unsigned int remain = pipe->len - ( pipe->reader & ( pipe->len - 1 ) );

	if ( readable == 0 ){
		return 0;
	}

	size = readable < size ? readable : size;

	if ( remain >= size ){
		memcpy( buffer, pipe->buf + ( pipe->reader & ( pipe->len - 1 ) ), size);
	}else{
		memcpy( buffer, pipe->buf + ( pipe->reader & ( pipe->len - 1 ) ), remain);
		memcpy( buffer + remain, pipe->buf, size - remain);
	}

	__sr_atom_add( pipe->reader, size );

	return size;
}


unsigned int sr_pipe_block_write(sr_pipe_t *pipe, char *data, unsigned int size)
{
	if (pipe == NULL || data == NULL || size == 0){
		loge("null parameter\n");
		return 0;
	}

	if (__is_true(pipe->stopped)){
		return 0;
	}

	sr_mutex_lock(pipe->mutex);

	unsigned int ret = 0, pos = 0;
	while (pos < size ) {
		ret = sr_pipe_write(pipe, data + pos, size - pos);
		pos += ret;
		if (pos != size){
			if (__is_true(pipe->stopped)){
				sr_mutex_unlock(pipe->mutex);
				return pos;
			}
			if ((pipe->len - pipe->writer + pipe->reader) == 0){
				__sr_atom_add(pipe->write_waiting, 1);
				if (ret > 0 && pipe->read_waiting > 0){
					sr_mutex_broadcast(pipe->mutex);
				}
				sr_mutex_wait(pipe->mutex);
				__sr_atom_sub(pipe->write_waiting, 1);
			}
		}
	}

	if (pipe->read_waiting){
		sr_mutex_broadcast(pipe->mutex);
	}

	sr_mutex_unlock(pipe->mutex);

	return pos;
}


unsigned int sr_pipe_block_read(sr_pipe_t *pipe, char *buf, unsigned int size)
{
	if (pipe == NULL || buf == NULL || size == 0){
		loge("null parameter\n");
		return 0;
	}

	sr_mutex_lock(pipe->mutex);

	unsigned int pos = 0;
	for(unsigned int ret = 0; pos < size; ){
		ret = sr_pipe_read(pipe, buf + pos, size - pos);
		pos += ret;
		if (pos != size){
			if (__is_true(pipe->stopped)){
				sr_mutex_unlock(pipe->mutex);
				return pos;
			}
			if ((pipe->writer - pipe->reader) == 0){
				__sr_atom_add(pipe->read_waiting, 1);
				if (ret > 0 && pipe->write_waiting > 0){
					sr_mutex_broadcast(pipe->mutex);
				}
				sr_mutex_wait(pipe->mutex);
				__sr_atom_sub(pipe->read_waiting, 1);
			}
		}
	}

	if (pipe->write_waiting){
		sr_mutex_broadcast(pipe->mutex);
	}

	sr_mutex_unlock(pipe->mutex);

	return pos;
}

///////////////////////////////////////////////////////////////
////缓冲区
///////////////////////////////////////////////////////////////

#define __sr_msg_size			(sizeof(sr_message_t))
#define __sr_max_extra_size			(1024*1024*2)

struct sr_messenger_t
{
	bool running;
	bool stopped;
	pthread_t tid;
	sr_pipe_t *pipe;
	sr_messenger_callback_t *cb;
};

static void *sr_messenger_loop(void *p)
{
	logd("enter\n");

	int result = 0;
	sr_message_t msg = {0};
	sr_messenger_t *messenger = (sr_messenger_t *) p;

	while (__is_true(messenger->running)) {
		if ((result = sr_messenger_receive(messenger, &msg)) < 0) {
			break;
		}
		messenger->cb->notify(messenger->cb, msg);
		if (msg.extra != NULL && msg.extra_size > 0){
			free(msg.extra);
			msg.extra = NULL;
			msg.extra_size = 0;
		}
	}

	__set_true(messenger->stopped);

	logd("exit\n");

	return NULL;
}

sr_messenger_t* sr_messenger_create(sr_messenger_callback_t *cb)
{
	logd("enter\n");

	sr_messenger_t *messenger = NULL;

	if (cb == NULL) {
		loge("null parameter\n");
		return NULL;
	}

	if ((messenger = (sr_messenger_t *) calloc(1, sizeof(sr_messenger_t))) == NULL) {
		loge("calloc failed\n");
		return NULL;
	}

	messenger->cb = cb;

	messenger->pipe = sr_pipe_create(__sr_msg_size * 10000);
	if (!messenger->pipe) {
		free(messenger);
		loge("sr_pipe_create failed\n");
		return NULL;
	}

	__set_true(messenger->running);

	if (messenger->cb != NULL && messenger->cb->notify != NULL){
		if (pthread_create(&(messenger->tid), NULL, sr_messenger_loop, messenger) != 0) {
			loge("pthread_create failed\n");
			messenger->tid = 0;
			sr_pipe_release(&messenger->pipe);
			free(messenger);
			return NULL;
		}
	}

	logd("exit\n");

	return messenger;
}

void sr_messenger_release(sr_messenger_t **pp_messenger)
{
	logd("enter\n");

	if (pp_messenger && *pp_messenger) {
		sr_messenger_t *messenger = *pp_messenger;
		*pp_messenger = NULL;
		__set_false(messenger->running);
		sr_pipe_stop(messenger->pipe);
		if (messenger->tid != 0){
			while(__is_false(messenger->stopped)){
				sr_pipe_stop(messenger->pipe);
				nanosleep((const struct timespec[]){{0, 1000L}}, NULL);
			}
			pthread_join(messenger->tid, NULL);
		}
		sr_pipe_release(&messenger->pipe);
		free(messenger);
	}

	logd("exit\n");
}


void sr_messenger_finish(sr_messenger_t *messenger)
{
	if (!messenger){
		loge("null parameter\n");
		return;
	}

	sr_pipe_finish(messenger->pipe);
}

int sr_messenger_send(sr_messenger_t *messenger, sr_message_t msg)
{
	if (messenger == NULL) {
		loge("null parameter\n");
		return -1;
	}

	if (msg.extra_size > __sr_max_extra_size) {
		loge("null parameter\n");
		return -1;
	}

	if (sr_pipe_block_write(messenger->pipe, (char*)&msg, __sr_msg_size) != __sr_msg_size){
		return -1;
	}

	if (msg.extra_size > 0 && msg.extra != NULL){
		if (sr_pipe_block_write(messenger->pipe, (char*)msg.extra, msg.extra_size) != msg.extra_size){
			return -1;
		}
	}

	return 0;
}

int sr_messenger_notify(sr_messenger_t *messenger, int msg_type)
{
	sr_message_t msg = { .type = msg_type, .if64.int64 = 0, .extra_size = 0, .extra = NULL };

	if (messenger == NULL) {
		loge("null parameter\n");
		return -1;
	}

	if (sr_pipe_block_write(messenger->pipe, (char*)&msg, __sr_msg_size) != __sr_msg_size){
		return -1;
	}

	return 0;
}

bool sr_messenger_is_arrive(sr_messenger_t *messenger)
{
	if (messenger != NULL){
		return sr_pipe_readable(messenger->pipe) >= __sr_msg_size;
	}
	return false;
}

int sr_messenger_receive(sr_messenger_t *messenger, sr_message_t *msg)
{
	if (messenger == NULL || msg == NULL) {
		loge("null parameter\n");
		return -1;
	}

	if (sr_pipe_block_read(messenger->pipe, (char*)msg, __sr_msg_size) != __sr_msg_size){
		return -1;
	}

	if (msg->extra_size > 0){
		if ((msg->extra = (void *)malloc(msg->extra_size)) == NULL){
			loge("malloc failed\n");
			return -1;
		}
		if (sr_pipe_block_read(messenger->pipe, (char*)msg->extra, msg->extra_size) != msg->extra_size){
			return -1;
		}
	}

	return 0;
}