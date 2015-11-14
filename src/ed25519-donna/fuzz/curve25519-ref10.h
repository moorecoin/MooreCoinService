#ifndef curve25519_ref10_h
#define curve25519_ref10_h

int crypto_scalarmult_base_ref10(unsigned char *q,const unsigned char *n);
int crypto_scalarmult_ref10(unsigned char *q, const unsigned char *n, const unsigned char *p);

#endif /* curve25519_ref10_h */

