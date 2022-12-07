//
// Created by liyong kang on 2022/12/2.
//

#ifndef __ENV_ATOMIC_H__
#define __ENV_ATOMIC_H__

#include <stdbool.h>

#ifdef ENV_HAVE_STDATOMIC

#include <stdatomic.h>

static bool __const_true = true;
static bool __const_false = false;

#define	__is_true(x)		    atomic_compare_exchange_strong(&(x), &__const_true, true)
#define	__is_false(x)		    atomic_compare_exchange_strong(&(x), &__const_false, false)
#define	__set_true(x)		    atomic_compare_exchange_strong(&(x), &__const_false, true)
#define	__set_false(x)		    atomic_compare_exchange_strong(&(x), &__const_true, false)

#define __atom_sub(x, y)		atomic_fetch_sub(&(x), (y))
#define __atom_add(x, y)		atomic_fetch_add(&(x), (y))

#define __atom_lock(x)			while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 10L}}, NULL)
#define __atom_try_lock(x)		__set_true(x)
#define __atom_unlock(x)		__set_false(x)

#else

#define	__is_true(x)	    	__sync_bool_compare_and_swap(&(x), true, true)
#define	__is_false(x)	    	__sync_bool_compare_and_swap(&(x), false, false)
#define	__set_true(x)		    __sync_bool_compare_and_swap(&(x), false, true)
#define	__set_false(x)		    __sync_bool_compare_and_swap(&(x), true, false)

#define __atom_sub(x, y)		__sync_sub_and_fetch(&(x), (y))
#define __atom_add(x, y)		__sync_add_and_fetch(&(x), (y))

#define __atom_lock(x)			while(!__set_true(x)) nanosleep((const struct timespec[]){{0, 10L}}, NULL)
#define __atom_try_lock(x)		__set_true(x)
#define __atom_unlock(x)		__set_false(x)

#endif

#endif //__ENV_ATOMIC_H__
