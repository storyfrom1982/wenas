/*
 * sr_mutex.c
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


#include "sr_pipe.h"


#include "sr_log.h"
#include "sr_error.h"
#include "sr_atom.h"
#include "sr_malloc.h"




struct Sr_pipe {
	bool stopped;
	uint8_t *buffer;
	unsigned int size;
	unsigned int writer;
	unsigned int reader;
};


static inline int fls( int x )
{
	int i, position;

	if ( x != 0 ){
		for ( i = x >> 1, position = 1; i != 0; position ++ ){
			i >>= 1;
		}
	}else{
		position = ( -1 );
	}

	return position;
}


int sr_pipe_create(unsigned int size, Sr_pipe **pp_pipe)
{
	int result = 0;
	Sr_pipe *pipe = NULL;

	if ( pp_pipe == NULL || size == 0 ){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if ((pipe = (Sr_pipe *)calloc(1, sizeof(Sr_pipe))) == NULL){
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

	if ( ( size != 0 && ( size & ( size - 1 ) ) == 0 ) ){
		pipe->size = size;
	}else{
		pipe->size = (unsigned int)(1UL << fls( size - 1 ));
	}

	if ((pipe->buffer = ( uint8_t * )malloc( pipe->size )) == NULL){
		sr_pipe_release(&pipe);
		loge(ERRMALLOC);
		return ERRMALLOC;
	}

    pipe->stopped = false;
	pipe->writer = pipe->reader = 0;

	*pp_pipe = pipe;

	return 0;
}


void sr_pipe_release(Sr_pipe **pp_pipe)
{
	if (pp_pipe && *pp_pipe){
		Sr_pipe *pipe = *pp_pipe;
		*pp_pipe = NULL;
		free(pipe->buffer);
		free(pipe);
	}
}


void sr_pipe_stop(Sr_pipe *pipe)
{
	if (pipe != NULL){
		SETTRUE(pipe->stopped);
	}
}


bool sr_pipe_is_stopped(Sr_pipe *pipe)
{
    if (pipe != NULL){
        return pipe->stopped;
    }
    return true;
}


void sr_pipe_restart(Sr_pipe *pipe)
{
    if (pipe != NULL){
        pipe->writer = pipe->reader = 0;
        SETFALSE(pipe->stopped);
    }
}


int sr_pipe_writable(Sr_pipe *pipe)
{
	if (pipe){
		return pipe->size - pipe->writer + pipe->reader;
	}
	return 0;
}


int sr_pipe_readable(Sr_pipe *pipe)
{
	if (pipe){
		return pipe->writer - pipe->reader;
	}
	return 0;
}


void sr_pipe_clean(Sr_pipe *pipe)
{
	if (pipe){
		pipe->writer = pipe->reader = 0;
	}
}


int sr_pipe_write(Sr_pipe *pipe, uint8_t *data, unsigned int size)
{
	if (pipe == NULL || data == NULL || size == 0){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	if (ISTRUE(pipe->stopped)){
		loge(ERREOF);
		return ERREOF;
	}

	unsigned int writable = pipe->size - pipe->writer + pipe->reader;
	unsigned int remain = pipe->size - ( pipe->writer & ( pipe->size - 1 ) );

	if ( writable == 0 ){
		return 0;
	}

	size = writable < size ? writable : size;

	if ( remain >= size ){
		memcpy( pipe->buffer + ( pipe->writer & ( pipe->size - 1 ) ), data, size);
	}else{
		memcpy( pipe->buffer + ( pipe->writer & ( pipe->size - 1 ) ), data, remain);
		memcpy( pipe->buffer, data + remain, size - remain);
	}

	SR_ATOM_ADD( pipe->writer, size );

	return size;
}


int sr_pipe_read(Sr_pipe *pipe, uint8_t *buffer, unsigned int size)
{
	if (pipe == NULL || buffer == NULL || size == 0){
		loge(ERRPARAM);
		return ERRPARAM;
	}

	unsigned int readable = pipe->writer - pipe->reader;
	unsigned int remain = pipe->size - ( pipe->reader & ( pipe->size - 1 ) );

	if ( readable == 0 ){
		if (ISTRUE(pipe->stopped)){
			loge(ERREOF);
			return ERREOF;
		}
		return 0;
	}

	size = readable < size ? readable : size;

	if ( remain >= size ){
		memcpy( buffer, pipe->buffer + ( pipe->reader & ( pipe->size - 1 ) ), size);
	}else{
		memcpy( buffer, pipe->buffer + ( pipe->reader & ( pipe->size - 1 ) ), remain);
		memcpy( buffer + remain, pipe->buffer, size - remain);
	}

	SR_ATOM_ADD( pipe->reader, size );

	return size;
}
