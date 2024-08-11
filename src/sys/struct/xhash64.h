#ifndef __XHASH_H__
#define __XHASH_H__

#include "xmalloc.h"

extern uint32_t xhash32(const void* input, size_t len, uint32_t seed);
extern uint64_t xhash64(const void* input, size_t len, uint64_t seed);

#endif //__XHASH_H__