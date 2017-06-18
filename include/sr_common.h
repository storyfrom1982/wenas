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
#define	ERRPARAMETER		(-10002)
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


#define SR_POP_BIT8(p, i)	\
	(i) = (*p++)

#define SR_POP_BIT16(p, i)	\
	(i)  = (*p++) << 8;		\
	(i) |= (*p++)

#define SR_POP_BIT24(p, i)	\
	(i)  = (*p++) << 16;	\
	(i) |= (*p++) << 8;		\
	(i) |= (*p++)

#define SR_POP_BIT32(p, i)	\
	(i)  = (*p++) << 24;	\
	(i) |= (*p++) << 16;	\
	(i) |= (*p++) << 8;		\
	(i) |= (*p++)

#define SR_POP_BIT64(p, i)	\
	(i)  = (*p++) << 56;	\
	(i) |= (*p++) << 48;	\
	(i) |= (*p++) << 40;	\
	(i) |= (*p++) << 32;	\
	(i) |= (*p++) << 24;	\
	(i) |= (*p++) << 16;	\
	(i) |= (*p++) << 8;		\
	(i) |= (*p++)


#define SR_PUSH_BIT8(p, i)		\
	(*p++) = (char)(i)

#define SR_PUSH_BIT16(p, i)		\
	(*p++) = (char)((i) >> 8);	\
	(*p++) = (char)(i)

#define SR_PUSH_BIT24(p, i)		\
	(*p++) = (char)((i) >> 16);	\
	(*p++) = (char)((i) >> 8);	\
	(*p++) = (char)(i)

#define SR_PUSH_BIT32(p, i)		\
	(*p++) = (char)((i) >> 24);	\
	(*p++) = (char)((i) >> 16);	\
	(*p++) = (char)((i) >> 8);	\
	(*p++) = (char)(i)

#define SR_PUSH_BIT64(p, i)		\
	(*p++) = (char)((i) >> 56);	\
	(*p++) = (char)((i) >> 48);	\
	(*p++) = (char)((i) >> 40);	\
	(*p++) = (char)((i) >> 32);	\
	(*p++) = (char)((i) >> 24);	\
	(*p++) = (char)((i) >> 16);	\
	(*p++) = (char)((i) >> 8);	\
	(*p++) = (char)(i)


extern int64_t sr_starting_time();
extern int64_t sr_calculate_time(int64_t start_microsecond);


#endif /* INCLUDE_SR_COMMON_H_ */
