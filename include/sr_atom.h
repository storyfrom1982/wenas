/*
 * sr_atom.h
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

#ifndef INCLUDE_SR_ATOM_H_
#define INCLUDE_SR_ATOM_H_


#include <time.h>
#include <stdint.h>
#include <stdbool.h>


#define	ISTRUE(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	ISFALSE(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	SETTRUE(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	SETFALSE(x)		__sync_bool_compare_and_swap(&(x), true, false)


#define SR_ATOM_SUB(x, y)		__sync_sub_and_fetch(&(x), (y))
#define SR_ATOM_ADD(x, y)		__sync_add_and_fetch(&(x), (y))
#define SR_ATOM_LOCK(x)			while(!SETTRUE(x)) nanosleep((const struct timespec[]){{0, 1L}}, NULL)
#define SR_ATOM_TRYLOCK(x)		SETTRUE(x)
#define SR_ATOM_UNLOCK(x)		SETFALSE(x)


#endif /* INCLUDE_SR_ATOM_H_ */
