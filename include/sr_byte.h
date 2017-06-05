/*
 * sr_byte.h
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

#ifndef INCLUDE_SR_BYTE_H_
#define INCLUDE_SR_BYTE_H_


#include <stdint.h>



#define PUSHINT8(p, i) \
	(*p++) = (int8_t)(i)

#define POPINT8(p, i) \
	(i) = (*p++)



#define PUSHINT16(p, i)	\
	(*p++) = (int16_t)(i) >> 8;	\
	(*p++) = (int16_t)(i) & 0xFF

#define POPINT16(p, i) \
	(i) = (*p++) << 8; \
	(i) |= (*p++)



#define PUSHINT24(p, i)	\
	(*p++) = ((int32_t)(i) >> 16) & 0xFF; \
	(*p++) = ((int32_t)(i) >> 8) & 0xFF; \
	(*p++) = (int32_t)(i) & 0xFF

#define POPINT24(p, i) \
	(i) = (*p++) << 16;	\
	(i) |= (*p++) << 8; \
	(i) |= (*p++)



#define PUSHINT32(p, i)	\
	(*p++) = (int32_t)(i) >> 24; \
	(*p++) = ((int32_t)(i) >> 16) & 0xFF; \
	(*p++) = ((int32_t)(i) >> 8) & 0xFF; \
	(*p++) = (int32_t)(i) & 0xFF

#define POPINT32(p, i) \
	(i) = (*p++) << 24;	\
	(i) |= (*p++) << 16; \
	(i) |= (*p++) << 8;	\
	(i) |= (*p++)



#define PUSHINT64(p, i)	\
	(*p++) = (int64_t)(i) >> 56; \
	(*p++) = ((int64_t)(i) >> 48) & 0xFF; \
	(*p++) = ((int64_t)(i) >> 40) & 0xFF; \
	(*p++) = ((int64_t)(i) >> 32) & 0xFF; \
	(*p++) = ((int64_t)(i) >> 24) & 0xFF; \
	(*p++) = ((int64_t)(i) >> 16) & 0xFF; \
	(*p++) = ((int64_t)(i) >> 8) & 0xFF; \
	(*p++) = (int64_t)(i) & 0xFF

#define POPINT64(p, i) \
	(i) = (int64_t)(*p++) << 56; \
	(i) |= (int64_t)(*p++) << 48; \
	(i) |= (int64_t)(*p++) << 40; \
	(i) |= (int64_t)(*p++) << 32; \
	(i) |= (int64_t)(*p++) << 24; \
	(i) |= (int64_t)(*p++) << 16; \
	(i) |= (int64_t)(*p++) << 8; \
	(i) |= (int64_t)(*p++)


#endif /* INCLUDE_SR_BYTE_H_ */
