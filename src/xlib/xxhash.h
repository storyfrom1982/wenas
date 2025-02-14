#ifndef __XXHASH_H__
#define __XXHASH_H__

#include "xnet/xalloc.h"

extern uint32_t xxhash32(const void* input, size_t len, uint32_t seed);
extern uint64_t xxhash64(const void* input, size_t len, uint64_t seed);

#endif //__XXHASH_H__