#ifndef __XSHA256_H__
#define __XSHA256_H__

#include "xmalloc.h"

#define SHA2_IS_BIG_ENDIAN	1

#define SHA256_DIGEST_SIZE	32
#define SHA256_BLOCK_SIZE	64
#define SHA256_STATE_WORDS	8

typedef struct {
	uint32_t state[SHA256_STATE_WORDS];
	uint64_t nblocks;
	uint8_t block[SHA256_BLOCK_SIZE];
	size_t num;
} SHA256_CTX;

extern void sha256_init(SHA256_CTX *ctx);
extern void sha256_update(SHA256_CTX *ctx, const uint8_t* data, size_t datalen);
extern void sha256_finish(SHA256_CTX *ctx, uint8_t dgst[SHA256_DIGEST_SIZE]);


#endif