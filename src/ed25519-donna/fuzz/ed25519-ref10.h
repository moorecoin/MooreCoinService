#ifndef ed25519_ref10_h
#define ed25519_ref10_h

int crypto_sign_pk_ref10(unsigned char *pk,unsigned char *sk);
int crypto_sign_ref10(unsigned char *sm,unsigned long long *smlen,const unsigned char *m,unsigned long long mlen,const unsigned char *sk);
int crypto_sign_open_ref10(unsigned char *m,unsigned long long *mlen,const unsigned char *sm,unsigned long long smlen,const unsigned char *pk);

#endif /* ed25519_ref10_h */

