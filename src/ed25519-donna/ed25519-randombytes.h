#if defined(ed25519_test)
/*
	isaac+ "variant", the paper is not clear on operator precedence and other
	things. this is the "first in, first out" option!

	not threadsafe or securely initialized, only for deterministic testing
*/
typedef struct isaacp_state_t {
	uint32_t state[256];
	unsigned char buffer[1024];
	uint32_t a, b, c;
	size_t left;
} isaacp_state;

#define isaacp_step(offset, mix) \
	x = mm[i + offset]; \
	a = (a ^ (mix)) + (mm[(i + offset + 128) & 0xff]); \
	y = (a ^ b) + mm[(x >> 2) & 0xff]; \
	mm[i + offset] = y; \
	b = (x + a) ^ mm[(y >> 10) & 0xff]; \
	u32to8_le(out + (i + offset) * 4, b);

static void
isaacp_mix(isaacp_state *st) {
	uint32_t i, x, y;
	uint32_t a = st->a, b = st->b, c = st->c;
	uint32_t *mm = st->state;
	unsigned char *out = st->buffer;

	c = c + 1;
	b = b + c;

	for (i = 0; i < 256; i += 4) {
		isaacp_step(0, rotl32(a,13))
		isaacp_step(1, rotr32(a, 6))
		isaacp_step(2, rotl32(a, 2))
		isaacp_step(3, rotr32(a,16))
	}

	st->a = a;
	st->b = b;
	st->c = c;
	st->left = 1024;
}

static void
isaacp_random(isaacp_state *st, void *p, size_t len) {
	size_t use;
	unsigned char *c = (unsigned char *)p;
	while (len) {
		use = (len > st->left) ? st->left : len;
		memcpy(c, st->buffer + (sizeof(st->buffer) - st->left), use);

		st->left -= use;
		c += use;
		len -= use;

		if (!st->left)
			isaacp_mix(st);
	}
}

void
ed25519_fn(ed25519_randombytes_unsafe) (void *p, size_t len) {
	static int initialized = 0;
	static isaacp_state rng;

	if (!initialized) {
		memset(&rng, 0, sizeof(rng));
		isaacp_mix(&rng);
		isaacp_mix(&rng);
		initialized = 1;
	}

	isaacp_random(&rng, p, len);
}
#elif defined(ed25519_customrng)

#include "ed25519-randombytes-custom.h"

#else

#include <openssl/rand.h>

void
ed25519_fn(ed25519_randombytes_unsafe) (void *p, size_t len) {

  rand_bytes(p, (int) len);

}
#endif
