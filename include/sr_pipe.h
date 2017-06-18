/*
 * sr_pipe.h
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

#ifndef INCLUDE_SR_PIPE_H_
#define INCLUDE_SR_PIPE_H_


typedef struct Sr_pipe Sr_pipe;

extern int sr_pipe_create(unsigned int size, Sr_pipe **pp_pipe);
extern void sr_pipe_release(Sr_pipe **pp_pipe);
extern void sr_pipe_stop(Sr_pipe *pipe);
extern int sr_pipe_is_stopped(Sr_pipe *pipe);
extern void sr_pipe_restart(Sr_pipe *pipe);

extern void sr_pipe_clean(Sr_pipe *pipe);

extern int sr_pipe_read(Sr_pipe *pipe, char *data, unsigned int size);
extern int sr_pipe_write(Sr_pipe *pipe, char *data, unsigned int size);

extern int sr_pipe_writable(Sr_pipe *pipe);
extern int sr_pipe_readable(Sr_pipe *pipe);


#endif /* INCLUDE_SR_PIPE_H_ */
