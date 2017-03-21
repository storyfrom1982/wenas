/*
 * sr_pipe.h
 *
 * Author: storyfrom1982@gmail.com
 *
 * Copyright (C) 2017 storyfrom1982@gmail.com all rights reserved.
 *
 * This file is part of self-reliance.
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


#include <stdint.h>
#include "sr_atom.h"


typedef struct SR_Pipe SR_Pipe;

extern int sr_pipe_create(unsigned int size, SR_Pipe **pp_pipe);
extern void sr_pipe_release(SR_Pipe **pp_pipe);
extern void sr_pipe_stop(SR_Pipe *pipe);
extern bool sr_pipe_is_stopped(SR_Pipe *pipe);
extern void sr_pipe_restart(SR_Pipe *pipe);

extern void sr_pipe_clean(SR_Pipe *pipe);

extern int sr_pipe_read(SR_Pipe *pipe, uint8_t *data, unsigned int size);
extern int sr_pipe_write(SR_Pipe *pipe, uint8_t *data, unsigned int size);

extern int sr_pipe_writable(SR_Pipe *pipe);
extern int sr_pipe_readable(SR_Pipe *pipe);


#endif /* INCLUDE_SR_PIPE_H_ */
