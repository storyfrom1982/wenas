/*
 * sr_error.h
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

#ifndef INCLUDE_SR_ERROR_H_
#define INCLUDE_SR_ERROR_H_


#include <errno.h>

#define	ERRNONE				(0)
#define	ERRUNKONWN			(-1)
#define	ERRSYSCALL			(-10000)
#define	ERRMALLOC			(-10001)
#define	ERRPARAM			(-10002)
#define	ERRTIMEOUT			(-10003)
#define	ERRTRYAGAIN			(-10004)
#define	ERREOF				(-10005)


#endif /* INCLUDE_SR_ERROR_H_ */
