/*
 * sr_common.h
 *
 *  Created on: 2017年6月18日
 *      Author: kly
 */

#ifndef INCLUDE_SR_COMMON_H_
#define INCLUDE_SR_COMMON_H_

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>


#define	ERRNONE				(0)
#define	ERRUNKONWN			(-1)
#define	ERRSYSCALL			(-10000)
#define	ERRMALLOC			(-10001)
#define	ERRPARAM			(-10002)
#define	ERRTIMEOUT			(-10003)
#define	ERRTRYAGAIN			(-10004)
#define	ERREOF				(-10005)


#define	ISTRUE(x)		__sync_bool_compare_and_swap(&(x), true, true)
#define	ISFALSE(x)		__sync_bool_compare_and_swap(&(x), false, false)
#define	SETTRUE(x)		__sync_bool_compare_and_swap(&(x), false, true)
#define	SETFALSE(x)		__sync_bool_compare_and_swap(&(x), true, false)


#define SR_ATOM_SUB(x, y)		__sync_sub_and_fetch(&(x), (y))
#define SR_ATOM_ADD(x, y)		__sync_add_and_fetch(&(x), (y))
#define SR_ATOM_LOCK(x)			while(!SETTRUE(x)) nanosleep((const struct timespec[]){{0, 1000L}}, NULL)
#define SR_ATOM_TRYLOCK(x)		SETTRUE(x)
#define SR_ATOM_UNLOCK(x)		SETFALSE(x)


extern int64_t sr_starting_time();
extern int64_t sr_calculate_time(int64_t start_microsecond);


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

#endif /* INCLUDE_SR_COMMON_H_ */
