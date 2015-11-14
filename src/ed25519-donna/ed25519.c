/*
	public domain by andrew m. <liquidsun@gmail.com>

	ed25519 reference implementation using ed25519-donna
*/


/* define ed25519_suffix to have it appended to the end of each public function */
#if !defined(ed25519_suffix)
#define ed25519_suffix 
#endif

#define ed25519_fn3(fn,suffix) fn##suffix
#define ed25519_fn2(fn,suffix) ed25519_fn3(fn,suffix)
#define ed25519_fn(fn)         ed25519_fn2(fn,ed25519_suffix)

#include "ed25519-donna.h"
#include "ed25519.h"
#include "ed25519-randombytes.h"
#include "ed25519-hash.h"

/*
	generates a (extsk[0..31]) and aext (extsk[32..63])
*/

donna_inline static void
ed25519_extsk(hash_512bits extsk, const ed25519_secret_key sk) {
	ed25519_hash(extsk, sk, 32);
	extsk[0] &= 248;
	extsk[31] &= 127;
	extsk[31] |= 64;
}

static void
ed25519_hram(hash_512bits hram, const ed25519_signature rs, const ed25519_public_key pk, const unsigned char *m, size_t mlen) {
	ed25519_hash_context ctx;
	ed25519_hash_init(&ctx);
	ed25519_hash_update(&ctx, rs, 32);
	ed25519_hash_update(&ctx, pk, 32);
	ed25519_hash_update(&ctx, m, mlen);
	ed25519_hash_final(&ctx, hram);
}

void
ed25519_fn(ed25519_publickey) (const ed25519_secret_key sk, ed25519_public_key pk) {
	bignum256modm a;
	ge25519 align(16) a;
	hash_512bits extsk;

	/* a = ab */
	ed25519_extsk(extsk, sk);
	expand256_modm(a, extsk, 32);
	ge25519_scalarmult_base_niels(&a, ge25519_niels_base_multiples, a);
	ge25519_pack(pk, &a);
}


void
ed25519_fn(ed25519_sign) (const unsigned char *m, size_t mlen, const ed25519_secret_key sk, const ed25519_public_key pk, ed25519_signature rs) {
	ed25519_hash_context ctx;
	bignum256modm r, s, a;
	ge25519 align(16) r;
	hash_512bits extsk, hashr, hram;

	ed25519_extsk(extsk, sk);

	/* r = h(aext[32..64], m) */
	ed25519_hash_init(&ctx);
	ed25519_hash_update(&ctx, extsk + 32, 32);
	ed25519_hash_update(&ctx, m, mlen);
	ed25519_hash_final(&ctx, hashr);
	expand256_modm(r, hashr, 64);

	/* r = rb */
	ge25519_scalarmult_base_niels(&r, ge25519_niels_base_multiples, r);
	ge25519_pack(rs, &r);

	/* s = h(r,a,m).. */
	ed25519_hram(hram, rs, pk, m, mlen);
	expand256_modm(s, hram, 64);

	/* s = h(r,a,m)a */
	expand256_modm(a, extsk, 32);
	mul256_modm(s, s, a);

	/* s = (r + h(r,a,m)a) */
	add256_modm(s, s, r);

	/* s = (r + h(r,a,m)a) mod l */	
	contract256_modm(rs + 32, s);
}

int
ed25519_fn(ed25519_sign_open) (const unsigned char *m, size_t mlen, const ed25519_public_key pk, const ed25519_signature rs) {
	ge25519 align(16) r, a;
	hash_512bits hash;
	bignum256modm hram, s;
	unsigned char checkr[32];

	if ((rs[63] & 224) || !ge25519_unpack_negative_vartime(&a, pk))
		return -1;

	/* hram = h(r,a,m) */
	ed25519_hram(hash, rs, pk, m, mlen);
	expand256_modm(hram, hash, 64);

	/* s */
	expand256_modm(s, rs + 32, 32);

	/* sb - h(r,a,m)a */
	ge25519_double_scalarmult_vartime(&r, &a, hram, s);
	ge25519_pack(checkr, &r);

	/* check that r = sb - h(r,a,m)a */
	return ed25519_verify(rs, checkr, 32) ? 0 : -1;
}

#include "ed25519-donna-batchverify.h"

/*
	fast curve25519 basepoint scalar multiplication
*/

void
ed25519_fn(curved25519_scalarmult_basepoint) (curved25519_key pk, const curved25519_key e) {
	curved25519_key ec;
	bignum256modm s;
	bignum25519 align(16) yplusz, zminusy;
	ge25519 align(16) p;
	size_t i;

	/* clamp */
	for (i = 0; i < 32; i++) ec[i] = e[i];
	ec[0] &= 248;
	ec[31] &= 127;
	ec[31] |= 64;

	expand_raw256_modm(s, ec);

	/* scalar * basepoint */
	ge25519_scalarmult_base_niels(&p, ge25519_niels_base_multiples, s);

	/* u = (y + z) / (z - y) */
	curve25519_add(yplusz, p.y, p.z);
	curve25519_sub(zminusy, p.z, p.y);
	curve25519_recip(zminusy, zminusy);
	curve25519_mul(yplusz, yplusz, zminusy);
	curve25519_contract(pk, yplusz);
}

